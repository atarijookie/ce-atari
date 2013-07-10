#include "floppyimagest.h"

FloppyImageSt::FloppyImageSt()
{
    openFlag        = false;
    params.isInit   = false;

    image.data = NULL;
    image.size = 0;
}

FloppyImageSt::~FloppyImageSt()
{
    close();
}

bool FloppyImageSt::open(char *fileName)
{
    close();

    fajl = fopen(fileName, "rb");

    if(fajl == NULL) {
        printf("Failed to open image file: %s\n", fileName);
        openFlag = false;
        return false;
    }

    openFlag = true;
    if(!loadImageIntoMemory()) {        // load the whole image in memory to avoid later disk access
        close();
        return false;
    }

    calcParams();                       // calculate the params of this floppy

    printf("ST Image opened: %s\n", fileName);
    printf("ST Image params - %d tracks, %d sides, %d sectors per track\n", params.tracksNo, params.sidesNo, params.sectorsPerTrack);

    return true;
}

bool FloppyImageSt::isOpen(void)
{
    return openFlag;                    // just return status
}

void FloppyImageSt::close()
{
    if(!openFlag) {                     // not open? nothing to do
        return;
    }

    fclose(fajl);
    fajl = NULL;
    openFlag = false;
    params.isInit = false;

    if(image.data != NULL) {
        delete []image.data;

        image.data = NULL;
        image.size = 0;
    }
}

bool FloppyImageSt::loadImageIntoMemory(void)
{
    if(image.data != NULL) {
        delete []image.data;
        image.data = NULL;
        image.size = 0;
    }

    fseek(fajl, 0, SEEK_END);           // move to the end of file
    int cnt = ftell(fajl);              // get the file size

    fseek(fajl, 0, SEEK_SET);           // move to the start of file

    image.data = new BYTE[cnt];
    int res = fread(image.data, 1, cnt, fajl);
    image.size = cnt;

    if(res != cnt) {
        return false;
    }

    return true;
}

bool FloppyImageSt::getParams(int &tracks, int &sides, int &sectorsPerTrack)
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

bool FloppyImageSt::readSector(int track, int side, int sectorNo, BYTE *buffer)
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

bool FloppyImageSt::calcParams(void)
{
    params.isInit = false;

    int cnt = image.size / 512;         // convert count of bytes to count of sectors

    if(cnt == 1440) {                   // if the size seems to be standard 80/2/9
        params.tracksNo         = 80;
        params.sidesNo          = 2;
        params.sectorsPerTrack  = 9;

        params.isInit           = true;
        return true;
    }

    if(cnt < 750) {                     // too little sectors?
        params.sidesNo = 1;             // single sided
    } else {
        params.sidesNo = 2;             // double sided
    }
    int sides = params.sidesNo;

    // now try to guess the right combination of track count and sectors per track count
    // note: 77,2,12 and 84,2,11 have the same sector count, also 78,2,14 and 84,2,13
    for(int track=75; track<86; track++) {
        for(int spt=5; spt<16; spt++) {
            int sectors = sides * track * spt;

            if(sectors == cnt) {
                params.tracksNo         = track;
                params.sectorsPerTrack  = spt;

                params.isInit           = true;
                return true;
            }
        }
    }

    printf("Couldn't guess the floppy params :(");
    return false;
}
