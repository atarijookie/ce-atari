// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <signal.h>

#include "../utils.h"
#include "../debug.h"
#include "../settings.h"
#include "acsidatatrans.h"
#include "imagesilo.h"
#include "floppysetup.h"
#include "floppyencoder.h"
#include "../display/displaythread.h"

pthread_mutex_t floppyEncoderMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  floppyEncoderShouldWork = PTHREAD_COND_INITIALIZER;

volatile bool floppyEncodingRunning = false;
volatile bool shouldStop;

//-------------------------------
// silo slots are global objects, as CCoreThread needs to stream from them and
// encoder thread needs to check all of them for tracks that need (re)encoding.
// Access is protected by floppyEncoderMutex.
// Individual tracks in slot can be streamed without using mutex when: slot.encImage.tracks[index].isReady
// (that means we're not encoding that track at that moment).

volatile int currentSlot;

SiloSlot slots[SLOT_COUNT];

WrittenMfmSector writtenSectors[WRITTENMFMSECTOR_COUNT];
//-------------------------------

extern TFlags flags;

void floppyEncoder_addEncodeWholeImageRequest(int slotNo, const char *imageFileName)
{
    Debug::out(LOG_DEBUG, "floppyEncoder_addEncodeWholeImageRequest -- slotNo: %d, image: %s", slotNo, imageFileName);

    pthread_mutex_lock(&floppyEncoderMutex);    		// lock the mutex

    SiloSlot *slot = &slots[slotNo];            		// get pointer to the right slot
    slot->openRequested = true;
    slot->openRequestTime = Utils::getCurrentMs();		// mark time when we requested opening of file
    slot->imageFileName = std::string(imageFileName);	// store image file name

    pthread_cond_signal(&floppyEncoderShouldWork);  	// wake up encoder
    pthread_mutex_unlock(&floppyEncoderMutex);      	// unlock the mutex
}

void floppyEncoder_addReencodeTrackRequest(int track, int side)
{
    Debug::out(LOG_DEBUG, "floppyEncoder_addReencodeTrackRequest - track: %d, side: %d", track, side);

    pthread_mutex_lock(&floppyEncoderMutex);        // lock the mutex

    SiloSlot *slot = &slots[currentSlot];           // get pointer to the right slot
    slot->encImage.askToReencodeTrack(track, side); // this specific track needs to be reencoded

    pthread_cond_signal(&floppyEncoderShouldWork);  // wake up encoder
    pthread_mutex_unlock(&floppyEncoderMutex);      // unlock the mutex
}

static int findEmptyWrittenSector(void)
{
    int i;
    for(i=0; i<WRITTENMFMSECTOR_COUNT; i++) {           // go through the written sector storage
        if(!writtenSectors[i].hasData) {                // this one doesn't have data? use it
            if(writtenSectors[i].data == NULL) {        // buffer not allocated? allocate!
                writtenSectors[i].data = (BYTE *) malloc(WRITTENMFMSECTOR_SIZE);
            }

            memset(writtenSectors[i].data, 0, WRITTENMFMSECTOR_SIZE);
            return i;       // return index to this sector
        }
    }

    // if came here, no empty sector in storage found
    return -1;
}

static int findWrittenSectorForProcessing(void)
{
    int i;
    for(i=0; i<WRITTENMFMSECTOR_COUNT; i++) {           // go through the written sector storage
        if(writtenSectors[i].hasData) {                 // this one has data? use it
            return i;       // return index to this sector
        }
    }

    // if came here, no empty sector in storage found
    return -1;
}

static void freeWrittenSectorStorage(void)
{
    int i;
    for(i=0; i<WRITTENMFMSECTOR_COUNT; i++) {       // go through the written sector storage
        if(writtenSectors[i].data != NULL) {        // buffer allocated? free it
            free(writtenSectors[i].data);
        }
    }
}

void floppyEncoder_decodeMfmWrittenSector(int track, int side, int sector, BYTE *data, DWORD size)
{
    if(currentSlot == EMPTY_IMAGE_SLOT) {           // if this is the empty image slot (used when no slot is selected), don't write
        Debug::out(LOG_DEBUG, "floppyEncoder_decodeMfmWrittenSector - NOT writing to empty image slot");
        return;
    }

    Debug::out(LOG_DEBUG, "floppyEncoder_decodeMfmWrittenSector - track: %d, side: %d, sector: %d, size: %d", track, side, sector, size);

    if(size > WRITTENMFMSECTOR_SIZE) {              // if data too big to fit, fail
        Debug::out(LOG_ERROR, "floppyEncoder_decodeMfmWrittenSector - size: %d > %d !!! sector not stored", size, WRITTENMFMSECTOR_SIZE);
        return;
    }

    pthread_mutex_lock(&floppyEncoderMutex);        // lock the mutex

    int index = findEmptyWrittenSector();           // try to find where this new sector could be stored

    if(index != -1) {                               // if was able to find empty place for this sector, store it
        WrittenMfmSector *wrSector = &writtenSectors[index];    // get pointer to it and store data and params
        wrSector->hasData = true;
        wrSector->slotNo = currentSlot;
        wrSector->track = track;
        wrSector->side = side;
        wrSector->sector = sector;

        memcpy(wrSector->data, data, size);         // copy in the data
        wrSector->size = size;

        SiloSlot *slot = &slots[currentSlot];           // get pointer to the right slot
        slot->encImage.askToReencodeTrack(track, side); // this specific track needs to be reencoded
    }

    pthread_cond_signal(&floppyEncoderShouldWork);  // wake up encoder
    pthread_mutex_unlock(&floppyEncoderMutex);      // unlock the mutex
}

void floppyEncoder_stop(void)
{
    pthread_mutex_lock(&floppyEncoderMutex);        // lock the mutex

    shouldStop = true;

    pthread_cond_signal(&floppyEncoderShouldWork);  // wake up encoder
    pthread_mutex_unlock(&floppyEncoderMutex);      // unlock the mutex
}

static void floppyEncoder_handleLoadFiles(void)
{
    for(int i=0; i<SLOT_COUNT; i++) {
        SiloSlot *slot = &slots[i];					        // get one slot

        if(!slot->openRequested) {	                        // if image open not request, skip it
            continue;
        }

        pthread_mutex_lock(&floppyEncoderMutex);            // lock the mutex

        slot->encImage.clearWholeCachedImage();             // clear remains of previous stream
        slot->openActionTime = Utils::getCurrentMs();       // store that we're opening the file now
        std::string imageFileName = slot->imageFileName;    // make a copy of the image file name, so we can unlock mutex but still work with the filename

        pthread_mutex_unlock(&floppyEncoderMutex);	        // unlock the mutex - the open bellow might take long, but slot->image is not touched by any other thread than floppyEncoder, so don't leave it locked

        if(slot->image) {                                   // if slot already contains image, get rid of it
            Debug::out(LOG_DEBUG, "ImageSilo::addEncodeWholeImageRequest -- deleting old image from memory");
            delete slot->image;                             // floppy image destructor will check if something needs to be written and does write if some changes need to be written
        }

        // try to load image from disk
        slot->image = FloppyImageFactory::getImage(imageFileName.c_str());

        if(!slot->image || !slot->image->isLoaded()) { // not supported image format or failed to open file?
            Debug::out(LOG_DEBUG, "ImageSilo::addEncodeWholeImageRequest - failed to load image %s", imageFileName);

            if(slot->image) {                       // if got the object, but failed to open, destory object and set pointer to null
                delete slot->image;
                slot->image = NULL;
            }
        } else {                                    // image loaded? good
            Debug::out(LOG_DEBUG, "ImageSilo::addEncodeWholeImageRequest - image %s loaded", imageFileName);
            slot->encImage.storeImageParams(slot->image);   // sets tracksToBeEncoded to all tracks count
        }

        // if no open request happened during the encoding (since openActionTime), clear the openRequested flag and we're done here
        if(slot->openRequestTime <= slot->openActionTime) {
            slot->openRequested = false;
        }
    }
}

static void floppyEncoder_handleSaveFiles(void)
{
    DWORD now = Utils::getCurrentMs();              // current time

    for(int i=0; i<SLOT_COUNT; i++) {
        if(slots[i].image == NULL) {                // if image in this slot not present, skip it
            continue;
        }

        if(!slots[i].image->gotUnsavedChanges()) {  // if this image doesn't have unsaved changes, skip it
            continue;
        }

        DWORD timeSinceLastWrite = now - slots[i].image->getLastWriteTime();
        if(timeSinceLastWrite < 5000) {             // if last write happened too recently, wait with the write
            continue;
        }

        slots[i].image->save();                     // save the changes
    }
}

static void floppyEncoder_doBeforeTerminating(void)
{
    for(int i=0; i<SLOT_COUNT; i++) {
        if(slots[i].image) {                    // if image in this slot is present
            slots[i].image->clear();            // save image if some unwritten data needs to be saved, free memory
            delete slots[i].image;              // delete image
            slots[i].image = NULL;
        }
    }

    freeWrittenSectorStorage();                 // free the storage for written sectors

    Debug::out(LOG_DEBUG, "Floppy encode thread terminated.");
}

SiloSlot *findSlotToEncode(void)
{
    // if the current slot has something to be encoded, return it - this is now priority (to be available for streaming)
    if(slots[currentSlot].encImage.somethingToBeEncoded()) {
        return &slots[currentSlot];
    }

    // go through the other slots, if one of them has something for encoding, return that
    for(int i=0; i<SLOT_COUNT; i++) {
        if(slots[i].encImage.somethingToBeEncoded()) {
            return &slots[i];
        }
    }

    // nothing found for encoding
    return NULL;
}

void *floppyEncodeThreadCode(void *ptr)
{
    Debug::out(LOG_DEBUG, "Floppy encode thread starting...");

    // the following code should encode all the slots, but one track at the time, so when there are multiple not encoded images,
    // the one which is selected for streaming, will be encoded first, and then the rest. This should also act fine if you switch
    // current slot in the middle of encoding image (to prioritize the selected one)
    DWORD after50ms = Utils::getEndTime(50);        // this will help to add pauses at least every 50 ms to allow other threads to do stuff

    while(true) {
        floppyEncoder_handleSaveFiles();            // save all those images which need to be saved
        floppyEncoder_handleLoadFiles();            // check if some file needs loading and then load it

        SiloSlot *slot = findSlotToEncode();        // check if there is something to encode
        int writtenIdx = findWrittenSectorForProcessing();  // check if something was written

        if(!slot && writtenIdx == -1) {              // nothing to encode and nothing to decode after write? sleep
            pthread_mutex_lock(&floppyEncoderMutex);    // lock the mutex
            pthread_cond_wait(&floppyEncoderShouldWork, &floppyEncoderMutex);   // unlock, sleep; wake up, lock
            pthread_mutex_unlock(&floppyEncoderMutex);  // unlock the mutex

            after50ms = Utils::getEndTime(50);      // after sleep - work for some time without pause
        }

        if(shouldStop) {                            // if we should already stop
            floppyEncoder_doBeforeTerminating();
            return 0;
        }

        if(Utils::getCurrentMs() > after50ms) {     // if at least 50 ms passed since start or previous pause, add a small pause so other threads could do stuff
            Utils::sleepMs(5);
            after50ms = Utils::getEndTime(50);
        }

        slot = findSlotToEncode();  // find some slot which needs encoding

        if(slot) {                  // if some slot needs encoding, do it, otherwise skip encoding
            floppyEncodingRunning = true;

            int track, side;
            slot->encImage.findNotReadyTrackAndEncodeIt(slot->image, track, side);

            if(track != -1) {       // if something was encoded, dump it to log
                Debug::out(LOG_DEBUG, "Encoding of [track %d, side %d] of image %s done", track, side, slot->image->getFileName());
            }

            floppyEncodingRunning = false;
        }

        writtenIdx = findWrittenSectorForProcessing();  // check if something was written

        if(writtenIdx != -1) {                          // something was written? decode, write to image
            WrittenMfmSector *ws = &writtenSectors[writtenIdx];     // get pointer to written sector
            SiloSlot *ss = &slots[ws->slotNo];                      // get pointer to slot where the sector should be stored

            BYTE sectorData[512];
            bool good;
            good = ss->encImage.decodeMfmBuffer(ws->data, ws->size, sectorData);   // decode written sector data

            if(good && ss->image) {     // if got the image pointer, write new sector data
                ss->image->writeSector(ws->track, ws->side, ws->sector, sectorData);
            }

            ws->hasData = false;        // this written sector doesn't hold any data anymore
        }
    }

    floppyEncoder_doBeforeTerminating();
    return 0;
}
