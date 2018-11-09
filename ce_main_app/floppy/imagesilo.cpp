// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <signal.h>
#include <atomic>

#include "../utils.h"
#include "../debug.h"
#include "../settings.h"
#include "acsidatatrans.h"
#include "imagesilo.h"
#include "floppysetup.h"
#include "../display/displaythread.h"

pthread_mutex_t floppyEncoderMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  floppyEncoderShouldWork = PTHREAD_COND_INITIALIZER;

volatile bool floppyEncodingRunning = false;
volatile bool shouldStop;

//-------------------------------
// silo slots are global objects now, as CCoreThread needs to stream from them and
// encoder thread needs to check all of them for tracks that need (re)encoding.
// Access is protected by floppyEncoderMutex.
// Individual tracks in slot can be streamed without using mutex when: slot.encImage.tracks[index].isReady
// (that means we're not encoding that track at that moment).

volatile std::atomic<int> currentSlot;

SiloSlot slots[SLOT_COUNT];
//-------------------------------

SiloSlotSimple  ImageSilo::floppyImages[3];
int ImageSilo::floppyImageSelected = EMPTY_IMAGE_SLOT;

extern TFlags flags;

void ImageSilo::addEncodeWholeImageRequest(int slotNo, const char *imageFileName)
{
    Debug::out(LOG_DEBUG, "ImageSilo::addEncodeWholeImageRequest -- slotNo: %d, image: %s", slotNo, imageFileName);

    floppyEncodingRunning = true;

    pthread_mutex_lock(&floppyEncoderMutex);    // lock the mutex
    SiloSlot *slot = &slots[slotNo];            // get pointer to the right slot

    slot->encImage.clearWholeCachedImage();     // clear remains of previous stream

    if(slot->image) {                           // if slot already contains image, get rid of it
        Debug::out(LOG_DEBUG, "ImageSilo::addEncodeWholeImageRequest -- deleting old image from memory", slotNo);
        delete slot->image;
    }

    // try to load image from disk
    slot->image = FloppyImageFactory::getImage(imageFileName);

    if(!slot->image || !slot->image->isOpen()) { // not supported image format or failed to open file?
        Debug::out(LOG_DEBUG, "ImageSilo::addEncodeWholeImageRequest - failed to load image %s", imageFileName);

        if(slot->image) {                       // if got the object, but failed to open, destory object and set pointer to null
            delete slot->image;
            slot->image = NULL;
        }
    } else {                                    // image loaded? good
        Debug::out(LOG_DEBUG, "ImageSilo::addEncodeWholeImageRequest - image %s loaded", imageFileName);
        slot->encImage.storeImageParams(slot->image);   // sets tracksToBeEncoded to all tracks count
    }

    pthread_cond_signal(&floppyEncoderShouldWork);  // wake up encoder
    pthread_mutex_unlock(&floppyEncoderMutex);      // unlock the mutex
}

void ImageSilo::addReencodeTrackRequest(int slotNo, int track, int side)
{
    Debug::out(LOG_DEBUG, "ImageSilo::addEncodeWholeImageRequest");

    pthread_mutex_lock(&floppyEncoderMutex);        // lock the mutex

    SiloSlot *slot = &slots[slotNo];                // get pointer to the right slot
    slot->encImage.askToReencodeTrack(track, side); // this specific track needs to be reencoded

    pthread_cond_signal(&floppyEncoderShouldWork);  // wake up encoder
    pthread_mutex_unlock(&floppyEncoderMutex);      // unlock the mutex
}

void ImageSilo::stop(void)
{
    pthread_mutex_lock(&floppyEncoderMutex);        // lock the mutex

    shouldStop = true;

    pthread_cond_signal(&floppyEncoderShouldWork);  // wake up encoder
    pthread_mutex_unlock(&floppyEncoderMutex);      // unlock the mutex
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

    pthread_mutex_lock(&floppyEncoderMutex);    // lock the mutex

    while(true) {                               // while we shouldn't stop
        // code should come to the start of this loop when there is nothing to be encoded and the encoder should sleep
        floppyEncodingRunning = false;              // not encoding right now

        SiloSlot *slot = findSlotToEncode();    // check if there is something to encode

        if(!slot) {                             // nothing to encode? sleep
            // unlock mutex, wait until should work. When should work, lock mutex and continue
            pthread_cond_wait(&floppyEncoderShouldWork, &floppyEncoderMutex);
        }

        if(shouldStop) {    // if we should already stop
            pthread_mutex_unlock(&floppyEncoderMutex);  // unlock the mutex
            Debug::out(LOG_DEBUG, "Floppy encode thread terminated.");
            return 0;
        }

        floppyEncodingRunning = true;               // we're encoding

        // the following code should encode all the slots, but one track at the time, so when there are multiple not encoded images,
        // the one which is selected for streaming, will be encoded first, and then the rest. This should also act fine if you switch
        // current slot in the middle of encoding image (to prioritize the selected one)
        DWORD after50ms = Utils::getEndTime(50);    // this will help to add pauses at least every 50 ms to allow other threads to do stuff

        while(true) {
            slot = findSlotToEncode();  // find some slot which needs encoding

            if(!slot) {                 // nothing to encode or should stop? quit loop and sleep
                floppyEncodingRunning = false;              // not encoding right now
                break;
            }

            if(shouldStop) {            // if we should stop, unlock mutex and quit
                floppyEncodingRunning = false;              // not encoding right now
                pthread_mutex_unlock(&floppyEncoderMutex);  // unlock the mutex
                Debug::out(LOG_DEBUG, "Floppy encode thread terminated.");
                return 0;
            }

            if(Utils::getCurrentMs() > after50ms) {     // if at least 50 ms passed since start or previous pause, add a small pause so other threads could do stuff
                Utils::sleepMs(5);
                after50ms = Utils::getEndTime(50);
            }

            // encode single track in image
            DWORD start = Utils::getCurrentMs();

            int track, side;
            slot->encImage.findNotReadyTrackAndEncodeIt(slot->image, track, side);

            if(track != -1) {       // if something was encoded, dump it to log
                DWORD end = Utils::getCurrentMs();
                Debug::out(LOG_DEBUG, "Encoding of [track %d, side %d] of image %s done, took %d ms", track, side, slot->image->getFileName(), (int) (end - start));
            }
        }
    }

    Debug::out(LOG_DEBUG, "Floppy encode thread terminated.");
    return 0;
}

ImageSilo::ImageSilo()
{
    //-----------
    // create empty track, which can be used when the equested track is out of range (or image doesn't exist)
    emptyTrack = new BYTE[MFM_STREAM_SIZE];
    memset(emptyTrack, 0, MFM_STREAM_SIZE);

    BYTE *p = emptyTrack;
    for(int i=0; i<(MFM_STREAM_SIZE/3); i++) {        // fill the empty track with 0x4e sync bytes
        *p++ = 0xa9;
        *p++ = 0x6a;
        *p++ = 0x96;
    }

    //-----------
    // init slots
    for(int i=0; i<SLOT_COUNT; i++) {
        clearSlot(i);
    }

    currentSlot = EMPTY_IMAGE_SLOT;
    reloadProxy = NULL;

    floppyImageSelected = -1;
    //-----------
    // create empty image and encode it (will be used when no slot is selected)
    bool res = FloppySetup::createNewImage(EMPTY_IMAGE_PATH);                   // create empty image in /tmp/ directory

    if(res) {                                                                   // if succeeded, encode this empty image
        Debug::out(LOG_DEBUG, "ImageSilo created empty image (for no selected image)");
        addEncodeWholeImageRequest(EMPTY_IMAGE_SLOT, EMPTY_IMAGE_PATH);
    } else {
        Debug::out(LOG_DEBUG, "ImageSilo failed to create empty image! (for no selected image)");
    }
    //-----------

    loadSettings();
}

ImageSilo::~ImageSilo()
{
    delete []emptyTrack;
}

BYTE *ImageSilo::getEmptyTrack(void)
{
    return emptyTrack;
}

void ImageSilo::loadSettings(void)
{
    Settings s;

    char key[32];
    for(int slot=0; slot<3; slot++) {
        sprintf(key, "FLOPPY_IMAGE_%d", slot);                                  // create settings key

        const char *img = s.getString(key, "");                              // try to read the value

        std::string pathAndFile, path, file;
        pathAndFile = img;

        //-----------------------------
        // if we're in the testing mode
        if(flags.test) {
            pathAndFile = CE_CONF_FDD_IMAGE_PATH_AND_FILENAME;
        }
        //-----------------------------

        if(pathAndFile.empty()) {                                               // nothing stored? skip it
            continue;
        }

        Utils::splitFilenameFromPath(pathAndFile, path, file);                  // split path from file
        std::string fileInTmp = "/tmp/" + file;
        std::string empty;

        bool res = Utils::copyFile(pathAndFile, fileInTmp);                     // copy the file to /tmp/

        if(!res) {                                                              // failed to copy?
            Debug::out(LOG_ERROR, "ImageSilo::loadSettings - didn't load image %s", img);
            continue;
        }

        add(slot, file, fileInTmp, empty, pathAndFile, false);                  // add this image, don't save to settings
    }

    // now tell the core thread that floppy images have changed
    if(reloadProxy) {
        reloadProxy->reloadSettings(SETTINGSUSER_FLOPPYIMGS);
    }
}

void ImageSilo::saveSettings(void)
{
    Settings s;

    char key[32];
    for(int slot=0; slot<3; slot++) {
        sprintf(key, "FLOPPY_IMAGE_%d", slot);                                  // create settings key

        const char *oldVal = s.getString(key, "");                           // try to read the old value
        std::string oldValStr = oldVal;

        if(oldValStr == slots[slot].hostSrcPath) {                              // if old value matches what we would save, skip it
            continue;
        }

        s.setString(key, slots[slot].hostSrcPath.c_str());             // store the value at that slot
    }

    // if something changed and got settings reload proxy, invoke reload
    if(reloadProxy) {
        reloadProxy->reloadSettings(SETTINGSUSER_FLOPPYIMGS);
    }
}

void ImageSilo::setSettingsReloadProxy(SettingsReloadProxy *rp)
{
    reloadProxy = rp;
}

void ImageSilo::add(int positionIndex, std::string &filename, std::string &hostDestPath, std::string &atariSrcPath, std::string &hostSrcPath, bool saveToSettings)
{
    if(positionIndex < 0 || positionIndex > 2) {
        return;
    }

    std::string filenameNoExt, ext;
    Utils::splitFilenameFromExt(filename, filenameNoExt, ext);   // create filename without extension (ZIPed image in list might be extracted under different extension)

    // store the info about slot
    slots[positionIndex].imageFile      = filename;         // just file name:                     bla.st
    slots[positionIndex].imageFileNoExt = filenameNoExt;    // just file name without extension:   bla
    slots[positionIndex].hostDestPath   = hostDestPath;     // where the file is stored when used: /tmp/bla.st
    slots[positionIndex].atariSrcPath   = atariSrcPath;     // from where the file was uploaded:   C:\gamez\bla.st
    slots[positionIndex].hostSrcPath    = hostSrcPath;      // for translated disk, host path:     /mnt/sda/gamez/bla.st

    floppyImages[positionIndex].imageFile = filename;

    // create and add floppy encode request
    addEncodeWholeImageRequest(positionIndex, hostDestPath.c_str());

    if(saveToSettings) {                    // should we save this to settings? (false when loading settings)
        saveSettings();
    }
}

void ImageSilo::swap(int index)
{
    if(index < 0 || index > 2) {
        return;
    }

    SiloSlot *a, *b;

    // find out which two slots to swap
    switch(index) {
        case 0:
            a = &slots[0];
            b = &slots[1];
            break;

        case 1:
            a = &slots[1];
            b = &slots[2];
            break;

        case 2:
            a = &slots[2];
            b = &slots[0];
            break;
    }

    // swap image files
    a->imageFile.swap(b->imageFile);
    a->hostDestPath.swap(b->hostDestPath);
    a->atariSrcPath.swap(b->atariSrcPath);
    a->hostSrcPath.swap(b->hostSrcPath);

    for(int i=0; i<3; i++) {
        floppyImages[i].imageFile = slots[i].imageFile;
    }

    // save it to settings
    saveSettings();
}

void ImageSilo::remove(int index)                   // remove image at specified slot
{
    if(index < 0 || index > 2) {
        return;
    }

    floppyImages[index].imageFile = "";

    if(slots[index].imageFile.empty()) {            // no image in this slot? skip the rest
        return;
    }

    // delete the file from /tmp
    unlink(slots[index].hostDestPath.c_str());

    clearSlot(index);

    // save it to settings
    saveSettings();
}

void ImageSilo::dumpStringsToBuffer(BYTE *bfr)      // copy the strings to buffer
{
    memset(bfr, 0, 512);

    for(int i=0; i<3; i++) {
        strncpy((char *) &bfr[(i * 160)     ], slots[i].imageFile.c_str(), 79);
    }
}

void ImageSilo::clearSlot(int index)
{
    slots[index].imageFile.clear();
    slots[index].hostDestPath.clear();
    slots[index].atariSrcPath.clear();
    slots[index].hostSrcPath.clear();

    if(index >= 0 && index < 3) {
        floppyImages[index].imageFile.clear();
    }
}

BYTE ImageSilo::getSlotBitmap(void)
{
    BYTE bmp = 0;

    for(int i=0; i<3; i++) {
        if(!slots[i].imageFile.empty()) {        // if slot is used, set the bit
            bmp |= (1 << i);
        }
    }

    return bmp;
}

void ImageSilo::setCurrentSlot(int index)
{
    if(index >= 0 && index <= 2) {                      // index good? use it
        currentSlot         = index;
        floppyImageSelected = index;
    } else {                                            // index bad? use slot with empty image
        currentSlot         = EMPTY_IMAGE_SLOT;
        floppyImageSelected = -1;
    }

    slots[currentSlot].encImage.newContent = false;     // current slot content not changed

    // set the floppy line on display
    char tmp[32];

    if(currentSlot == EMPTY_IMAGE_SLOT) {       // empty floppy?
        strcpy  (tmp,     "FDD : empty");
    } else {                                    // something selected?
        snprintf(tmp, 32, "FDD%d: %s", (int) currentSlot, slots[currentSlot].imageFile.c_str());
    }

    display_setLine(DISP_LINE_FLOPPY, tmp);     // store the floppy display line
    display_showNow(DISP_SCREEN_HDD1_IDX);      // show it right now - floppy image changed

    beeper_beep(BEEP_SHORT);                    // do a beep on button press / changing floppy slot
}

int ImageSilo::getCurrentSlot(void)
{
    return currentSlot;
}

BYTE *ImageSilo::getEncodedTrack(int track, int side, int &bytesInBuffer)
{
    BYTE *pTrack;

    if(!slots[currentSlot].encImage.encodedTrackIsReady(track, side)) { // track not ready?
        addReencodeTrackRequest(currentSlot, track, side);              // ask for reencoding

        // wait short while to see if the image gets encoded
        DWORD endTime = Utils::getEndTime(500);
        bool isReady = false;

        while(Utils::getCurrentMs() < endTime) {    // still should wait?
            isReady = slots[currentSlot].encImage.encodedTrackIsReady(track, side); // check if it's ready

            if(isReady) {   // ready? quit loop
                break;
            }
        }

        if(!isReady) {      // not ready? return empty track
            return emptyTrack;
        }
    }

    // is ready? return that track
    pTrack = slots[currentSlot].encImage.getEncodedTrack(track, side, bytesInBuffer);   // get data from current slot
    return pTrack;
}

bool ImageSilo::getParams(int &tracks, int &sides, int &sectorsPerTrack)
{
    return slots[currentSlot].encImage.getParams(tracks, sides, sectorsPerTrack);
}

bool ImageSilo::containsImage(const char *filename)    // check if image with this filename exists in silo
{
    std::string fnameStr = filename;

    for(int i=0; i<3; i++) {
        if(slots[i].imageFile == fnameStr) {
            return true;
        }
    }

    return false;
}

// check if image with this filename exists in silo and fill buffer with string on which slots it's inserted
void ImageSilo::containsImageInSlots(std::string &filenameWExt, std::string &out)
{
    std::string filename, ext;
    Utils::splitFilenameFromExt(filenameWExt, filename, ext);   // create filename without extension (ZIPed image in list might be extracted under different extension)
    char tmp[32];

    for(int i=0; i<3; i++) {
        bool isInSlot = (slots[i].imageFileNoExt == filename);  // if this file is in this slot
        bool isSlotSelected = (i == currentSlot);               // if this slot is selecter

        if(isInSlot) {                                          // if image is in slot
            if(isSlotSelected) {                                // if this slot is selected
                out += "\033p";                                 // inverse on
            }

            sprintf(tmp, "%d", i + 1);                          // slot number - integer to string (std::to_string not present on Jessie)
            out += tmp;                                         // add slot number

            if(isSlotSelected) {                                // if this slot is selected
                out += "\033q";                                 // inverse off
            }
        } else {                                                // if this image is not in this slot
            out += ".";                                         // insert just place holder
        }
    }
}

// check if image with this filename exists in silo and eject it from each slot it is in
void ImageSilo::removeByFileName(std::string &filenameWExt)
{
    std::string filename, ext;
    Utils::splitFilenameFromExt(filenameWExt, filename, ext);   // create filename without extension (ZIPed image in list might be extracted under different extension)

    for(int i=0; i<3; i++) {
        bool isInSlot = (slots[i].imageFileNoExt == filename);  // if this file is in this slot

        if(isInSlot) {                                          // if image is in slot
             remove(i);
        }
    }
}

bool ImageSilo::currentSlotHasNewContent(void)
{
    if(slots[currentSlot].encImage.newContent) {            // if the current slot has new content
        slots[currentSlot].encImage.newContent = false;     // set flag to false
        return true;                                        // return that the content is new
    }

    return false;                                           // otherwise no new content
}

SiloSlot *ImageSilo::getSiloSlot(int index)
{
    if(index < 0 || index > 2) {
        return NULL;
    }

    return &slots[index];
}

int ImageSilo::getFloppyImageSelectedId(void)
{
    return floppyImageSelected;
}

SiloSlotSimple * ImageSilo::getFloppyImageSimple(int index)
{
    if(index < 0 || index >= 3)
        return NULL;
    return &floppyImages[index];
}

bool ImageSilo::getFloppyEncodingRunning(void)
{
    return floppyEncodingRunning;
}
