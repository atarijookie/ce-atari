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
#include "imagesilo.h"
#include "floppyencoder.h"

extern pthread_mutex_t floppyEncoderMutex;
extern pthread_cond_t  floppyEncoderShouldWork;

extern SiloSlot slots[SLOT_COUNT];
//-------------------------------

extern TFlags flags;

ImageSilo::ImageSilo()
{
    //-----------
    // create empty track, which can be used when the equested track is out of range (or image doesn't exist)
    emptyTrack = new uint8_t[MFM_STREAM_SIZE];
    memset(emptyTrack, 0, MFM_STREAM_SIZE);

    uint8_t *p = emptyTrack;
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

    loadSettings();
}

ImageSilo::~ImageSilo()
{
    delete []emptyTrack;
}

bool ImageSilo::createNewImage(std::string pathAndFile)
{
    // open the file
    FILE *f = fopen((char *) pathAndFile.c_str(), "wb");

    if(!f) {                                            // failed to open file?
        logFdd(LOG_ERROR, "FloppySetup::newImage - failed to open file %s", (char *) pathAndFile.c_str());
        return false;
    }

    // create default boot sector (copied from blank .st image created in Steem)
    uint8_t sect0start[]   = {0xeb, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x82, 0x75, 0x00, 0x02, 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0xa0, 0x05, 0xf9, 0x05, 0x00, 0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t sect0end[]     = {0x00, 0x97, 0xc7};

    uint8_t bfr[512];
    memset(bfr, 0, 512);

    memcpy(bfr, sect0start, sizeof(sect0start));                        // copy the start of default boot sector to start of buffer
    memcpy(bfr + 512 - sizeof(sect0end), sect0end, sizeof(sect0end));   // copy the end of default boot sector to end of buffer

    fwrite(bfr, 1, 512, f);

    // create the empty rest of the file
    memset(bfr, 0, 512);

    int totalSectorCount = (9*80*2) - 1;                                // calculate the count of sectors on a floppy - 2 sides, 80 tracks, 9 spt, minus the already written boot sector

    for(int i=0; i<totalSectorCount; i++) {
        fwrite(bfr, 1, 512, f);
    }

    fclose(f);
    return true;
}

uint8_t *ImageSilo::getEmptyTrack(void)
{
    return emptyTrack;
}

void ImageSilo::loadSettings(void)
{
    Settings s;

    char key[32];
    for(int slot=0; slot<SLOT_COUNT; slot++) {
        sprintf(key, "FLOPPY_IMAGE_%d", slot);      // create settings key

        const char *img = s.getString(key, "");     // try to read the value

        std::string pathAndFile, path, file;
        pathAndFile = img;

        if(pathAndFile.empty()) {                   // nothing stored? skip it
            // TODO: add() empty image
            continue;
        }

        add(slot, file, pathAndFile);        // add this image
    }
}

void ImageSilo::saveSettings(void)
{

}

void ImageSilo::add(int positionIndex, std::string &filename, std::string &hostPath)
{
    if(positionIndex < 0 || positionIndex >= SLOT_COUNT) {
        return;
    }

    std::string filenameNoExt, ext;
    Utils::splitFilenameFromExt(filename, filenameNoExt, ext);   // create filename without extension (ZIPed image in list might be extracted under different extension)

    // store the info about slot
    slots[positionIndex].imageFile      = filename;         // just file name:                     bla.st
    slots[positionIndex].imageFileNoExt = filenameNoExt;    // just file name without extension:   bla
    slots[positionIndex].hostPath       = hostPath;         // where the file is stored on translated drive (/mnt/sda/gamez/bla.st) or where the image is uploaded from atari (/tmp/bla.st)

    // create and add floppy encode request
    floppyEncoder_addEncodeWholeImageRequest(positionIndex, hostPath.c_str());
}

void ImageSilo::remove(int index)                   // remove image at specified slot
{
    if(index < 0 || index >= SLOT_COUNT) {
        return;
    }

    if(slots[index].imageFile.empty()) {            // no image in this slot? skip the rest
        return;
    }

    clearSlot(index);
    saveSettings();                     // save it to settings
}

void ImageSilo::dumpStringsToBuffer(uint8_t *bfr)      // copy the strings to buffer
{
    memset(bfr, 0, 512);

    for(int i=0; i<3; i++) {
        strncpy((char *) &bfr[(i * 160)     ], slots[i].imageFile.c_str(), 79);
    }
}

void ImageSilo::clearSlot(int index)
{
    slots[index].imageFile.clear();
    slots[index].imageFileNoExt.clear();
    slots[index].hostPath.clear();
}

uint8_t *ImageSilo::getEncodedTrack(int floppySlotIndex, int track, int side, int &bytesInBuffer)
{
    uint8_t *pTrack;

    if(floppySlotIndex < 0 || floppySlotIndex >= SLOT_COUNT) {
        return emptyTrack;
    }

    //logFdd(LOG_DEBUG, "ImageSilo::getEncodedTrack - track: %d, side: %d, currentSlot: %d, isReady: %d", track, side, currentSlot, slots[currentSlot].encImage.encodedTrackIsReady(track, side));
    //uint32_t start = Utils::getCurrentMs();

    if(!slots[floppySlotIndex].encImage.encodedTrackIsReady(track, side)) {     // track not ready?
        floppyEncoder_addReencodeTrackRequest(floppySlotIndex, track, side);    // ask for reencoding

        // wait short while to see if the image gets encoded
        uint32_t endTime = Utils::getEndTime(500);
        bool isReady = false;

        while(Utils::getCurrentMs() < endTime) {    // still should wait?
            isReady = slots[floppySlotIndex].encImage.encodedTrackIsReady(track, side); // check if it's ready

            if(isReady) {   // ready? quit loop
                break;
            }
        }

        if(!isReady) {      // not ready? return empty track
            //logFdd(LOG_DEBUG, "ImageSilo::getEncodedTrack - finishing with isReady: %d after %d ms", isReady, Utils::getCurrentMs() - start);
            return emptyTrack;
        }
    }

    //logFdd(LOG_DEBUG, "ImageSilo::getEncodedTrack - finishing with isReady: %d after %d ms", true, Utils::getCurrentMs() - start);
    
    // is ready? return that track
    pTrack = slots[floppySlotIndex].encImage.getEncodedTrack(track, side, bytesInBuffer);   // get data from current slot
    return pTrack;
}

bool ImageSilo::getParams(int floppySlotIndex, int &tracks, int &sides, int &sectorsPerTrack)
{
    if(floppySlotIndex < 0 || floppySlotIndex >= SLOT_COUNT) {
        tracks = 0;
        sides = 0;
        sectorsPerTrack = 0;
        return false;
    }

    return slots[floppySlotIndex].encImage.getParams(tracks, sides, sectorsPerTrack);
}

std::string ImageSilo::getFileName(int floppySlotIndex)
{
    if(floppySlotIndex < 0 || floppySlotIndex >= SLOT_COUNT) {
        std::string empty;
        return empty;
    }

    return slots[floppySlotIndex].imageFile;
}
