// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <string.h>
#include <stdlib.h>

#include "floppyimage.h"
#include "../debug.h"

FloppyImage::FloppyImage()
{
	loadedFlag      = false;
    params.isInit   = false;

    image.data = NULL;
    image.size = 0;
}

FloppyImage::~FloppyImage()
{
	clear();
}

bool FloppyImage::isLoaded(void)
{
	return loadedFlag;
}

void FloppyImage::close(void)               // just close the file handle if open
{
    if(fajl) {                              // if file still open, close it
        fclose(fajl);
        fajl = NULL;
    }
}

void FloppyImage::clear(void)           // close and clear / free memory
{
    close();                            // if file still open, close it

    if(!loadedFlag) {                   // not loaded? nothing to do
        return;
    }

    loadedFlag = false;
    params.isInit = false;

    if(image.data != NULL) {
        free(image.data);
        image.data = NULL;
        image.size = 0;
    }
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

bool FloppyImage::readSector(int track, int side, int sectorNo, BYTE *buffer)
{
    if(!loadedFlag) {   // not loaded?
        return false;
    }

    if(sectorNo < 1 || sectorNo > params.sectorsPerTrack) {     // sector # out of range?
        return false;
    }

    int offset   = track    * (params.sidesNo * params.sectorsPerTrack);    // move to the right track
    offset      += side     * params.sectorsPerTrack;                       // then move to the right side
    offset      += (sectorNo - 1);                                          // and move a little to the right sector
    offset       = offset * 512;                                            // calculate ofsset in bytes

    memcpy(buffer, &image.data[offset], 512);
    return true;
}

const char *FloppyImage::getFileName(void)
{
    return currentFileName.c_str();
}

bool FloppyImage::open(const char *fileName)
{
    clear();

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
    if(image.data != NULL) {
        free(image.data);
        image.data = NULL;
        image.size = 0;
    }

    fseek(fajl, 0, SEEK_END);           // move to the end of file
    int cnt = ftell(fajl);              // get the file size
    fseek(fajl, 0, SEEK_SET);           // move to the start of file

    image.data = (BYTE *)malloc(cnt);
    int res = fread(image.data, 1, cnt, fajl);
    image.size = cnt;

    if(res != cnt) {
        free(image.data);
        image.data = NULL;
        image.size = 0;
        return false;
    }

    return true;
}
