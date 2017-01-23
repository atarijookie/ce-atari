// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <string.h>
#include <stdlib.h>

#include "floppyimage.h"
#include "../debug.h"

FloppyImage::FloppyImage()
{
	openFlag        = false;
    params.isInit   = false;

    image.data = NULL;
    image.size = 0;
}

FloppyImage::~FloppyImage()
{
	close();
}

bool FloppyImage::isOpen(void)
{
	return openFlag;
}

void FloppyImage::close()
{
    if(!openFlag) {                     // not open? nothing to do
        return;
    }

    fclose(fajl);
    fajl = NULL;
    openFlag = false;
    params.isInit = false;

    if(image.data != NULL) {
        free(image.data);
        image.data = NULL;
        image.size = 0;
    }
}

bool FloppyImage::getParams(int &tracks, int &sides, int &sectorsPerTrack)
{
    if(!openFlag) {
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
    if(!openFlag) {                                             // not open?
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
    close();

    currentFileName = fileName;

    fajl = fopen(fileName, "rb");

    if(fajl == NULL) {
        Debug::out(LOG_ERROR, "Failed to open image file: %s", fileName);
        openFlag = false;
    } else {
        openFlag = true;
    }
    return openFlag;
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
