#include <stdio.h>
#include <string.h>

#include "imagefilemedia.h"
#include "../debug.h"

ImageFileMedia::ImageFileMedia()
{
    BCapacity       = 0;
    SCapacity       = 0;
    mediaHasChanged = 0;
    image           = NULL;
}

ImageFileMedia::~ImageFileMedia()
{
    iclose();
}

bool ImageFileMedia::iopen(char *path, bool createIfNotExists)
{
    bool imageWasCreated = false;
    mediaHasChanged = false;

    image = fopen(path, "rb+");                  // try to open existing file

    if(image == NULL && createIfNotExists) {     // failed to open existing file and should create one?
        image = fopen(path, "wb+");
        imageWasCreated = true;
    }

    if(image == NULL) {
        Debug::out(LOG_ERROR, "ImageFileMedia - failed to open %s", path);
        return false;
    }

    if(imageWasCreated) {               // if the image was just created, create empty image file
        BYTE bfr[512];
        memset(bfr, 0, 512);

        for(int i=0; i<2048; i++) {     // create 1 MB image
            fwrite(bfr, 1, 512, image);
        }

        fflush(image);
    }

    fseek(image, 0, SEEK_END);          // move to the end of file
    DWORD cap = ftell(image);           // get the position in file (offset from start)

    BCapacity = cap;                    // capacity in bytes
    SCapacity = cap / 512;              // capacity in sectors

    mediaHasChanged = false;

    Debug::out(LOG_INFO, "ImageFileMedia - open succeeded, capacity: %d, was created: %d", BCapacity, (int) imageWasCreated);

    return true;
}

void ImageFileMedia::iclose(void)
{
    if(image) {
        fclose(image);
    }

    BCapacity       = 0;
    SCapacity       = 0;
    mediaHasChanged = 0;
    image           = NULL;
}

bool ImageFileMedia::isInit(void)
{
    if(image) {
        return true;
    }

    return false;
}

bool ImageFileMedia::mediaChanged(void)
{
    return mediaHasChanged;
}

void ImageFileMedia::setMediaChanged(bool changed)
{
    mediaHasChanged = changed;
}

void ImageFileMedia::getCapacity(DWORD &bytes, DWORD &sectors)
{
    bytes   = BCapacity;
    sectors = SCapacity;
}

bool ImageFileMedia::readSectors(DWORD sectorNo, DWORD count, BYTE *bfr)
{
    if(!isInit()) {                             // if not initialized, failed
        return false;
    }

    DWORD pos = sectorNo * 512;                 // convert sector # to offset in image file
    DWORD res = fseek(image, pos, SEEK_SET);      // move to the end of file

    if(res != 0) {                              // failed to fseek?
        return false;
    }

    DWORD byteCount = count * 512;
    res = fread(bfr, 1, byteCount, image);      // read

    if(res != byteCount) {                      // not all data was read? fail
        return false;
    }

    return true;
}

bool ImageFileMedia::writeSectors(DWORD sectorNo, DWORD count, BYTE *bfr)
{
    if(!isInit()) {                             // if not initialized, failed
        return false;
    }

    DWORD pos = sectorNo * 512;                 // convert sector # to offset in image file
    DWORD res = fseek(image, pos, SEEK_SET);    // move to the end of file

    if(res != 0) {                              // failed to fseek?
        return false;
    }

    DWORD byteCount = count * 512;
    res = fwrite(bfr, 1, byteCount, image);     // write

    if(res != byteCount) {                      // not all data was written? fail
        return false;
    }

    return true;
}
