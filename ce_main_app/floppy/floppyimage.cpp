// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <string.h>
#include <stdlib.h>

#include "floppyimage.h"
#include "../debug.h"
#include "../utils.h"

FloppyImage::FloppyImage()
{
	loadedFlag      = false;
    params.isInit   = false;

    image.data = NULL;
    image.size = 0;

    sectorsWritten = 0;                     // nothing written yet
    lastWriteTime = Utils::getCurrentMs();  // pretend that writeSector() just happened
}

FloppyImage::~FloppyImage()
{
    close();                            // if file still open, close it
	clear();                            // save if needed, free memory
}

bool FloppyImage::isLoaded(void)
{
	return loadedFlag;
}

void FloppyImage::close(void)           // just close the file handle if open
{
    if(fajl) {                          // if file still open, close it
        fclose(fajl);
        fajl = NULL;
    }
}

void FloppyImage::clear(void)           // save file if needed, free the memory
{
    if(gotUnsavedChanges()) {           // if something wasn't saved yet, save it
        save();
    }

    loadedFlag = false;
    params.isInit = false;
    sectorsWritten = 0;                 // nothing written

    if(image.data != NULL) {            // free memory if should
        free(image.data);
        image.data = NULL;
        image.size = 0;
    }

    lastWriteTime = Utils::getCurrentMs();  // pretend that writeSector() just happened
}

bool FloppyImage::getParams(int &tracks, int &sides, int &sectorsPerTrack)
{
    if(!loadedFlag) {
        tracks          = 0;
        sides           = 0;
        sectorsPerTrack = 0;

        return false;
    }

    tracks          = params.tracksNo;
    sides           = params.sidesNo;
    sectorsPerTrack = params.sectorsPerTrack;

    return true;
}

bool FloppyImage::gotUnsavedChanges(void)
{
    return (sectorsWritten > 0);    // if something written and not saved, return true
}

bool FloppyImage::readNotWriteSector(bool readNotWrite, int track, int side, int sectorNo, BYTE *buffer)
{
    if(!loadedFlag) {   // not loaded?
        return false;
    }

    if(sectorNo < 1 || sectorNo > params.sectorsPerTrack) {                 // sector # out of range?
        return false;
    }

    int offset   = track    * (params.sidesNo * params.sectorsPerTrack);    // move to the right track
    offset      += side     * params.sectorsPerTrack;                       // then move to the right side
    offset      += (sectorNo - 1);                                          // and move a little to the right sector
    offset       = offset * 512;                                            // calculate ofsset in bytes


    if(readNotWrite) {      // read - from image to buffer
        memcpy(buffer, &image.data[offset], 512);
    } else {                // write - from buffer to image
        sectorsWritten++;                       // one more unwritten sector
        lastWriteTime = Utils::getCurrentMs();  // store when the writeSector() happened
        memcpy(&image.data[offset], buffer, 512);
    }

    return true;
}

DWORD FloppyImage::getLastWriteTime(void)
{
    return lastWriteTime;
}

// convenience function for reading sector
bool FloppyImage::readSector(int track, int side, int sectorNo, BYTE *buffer)
{
    return readNotWriteSector(true, track, side, sectorNo, buffer);
}

// convenience function for writing sector
bool FloppyImage::writeSector(int track, int side, int sectorNo, BYTE *buffer)
{
    return readNotWriteSector(false, track, side, sectorNo, buffer);
}

const char *FloppyImage::getFileName(void)
{
    return currentFileName.c_str();
}

bool FloppyImage::open(const char *fileName)
{
    close();        // close if open
    clear();        // save if needed, free memory

    currentFileName = fileName;

    fajl = fopen(fileName, "rb");

    if(fajl == NULL) {
        Debug::out(LOG_ERROR, "Failed to open image file: %s", fileName);
        return false;
    }

    return true;
}

bool FloppyImage::loadImageIntoMemory(void)
{
    clear();                            // save if needed, free memory

    fseek(fajl, 0, SEEK_END);           // move to the end of file
    int cnt = ftell(fajl);              // get the file size
    fseek(fajl, 0, SEEK_SET);           // move to the start of file

    image.data = (BYTE *)malloc(cnt);
    int res = fread(image.data, 1, cnt, fajl);
    image.size = cnt;

    if(res != cnt) {                    // failed to load? clear and fail
        close();                        // close file if open
        clear();                        // save if needed, free memory
        return false;
    }

    // leave file open - e.g. msa will need more file access, so let it and close it later
    return true;
}
