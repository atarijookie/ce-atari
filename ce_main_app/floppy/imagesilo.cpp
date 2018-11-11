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

extern pthread_mutex_t floppyEncoderMutex;
extern pthread_cond_t  floppyEncoderShouldWork;
extern volatile bool   floppyEncodingRunning;

//-------------------------------
// silo slots are global objects now, as CCoreThread needs to stream from them and
// encoder thread needs to check all of them for tracks that need (re)encoding.
// Access is protected by floppyEncoderMutex.
// Individual tracks in slot can be streamed without using mutex when: slot.encImage.tracks[index].isReady
// (that means we're not encoding that track at that moment).

extern volatile int currentSlot;

extern SiloSlot slots[SLOT_COUNT];
//-------------------------------

SiloSlotSimple  ImageSilo::floppyImages[3];
int ImageSilo::floppyImageSelected = EMPTY_IMAGE_SLOT;

extern TFlags flags;

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
        floppyEncoder_addEncodeWholeImageRequest(EMPTY_IMAGE_SLOT, EMPTY_IMAGE_PATH);
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
    floppyEncoder_addEncodeWholeImageRequest(positionIndex, hostDestPath.c_str());

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
        floppyEncoder_addReencodeTrackRequest(currentSlot, track, side);              // ask for reencoding

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
