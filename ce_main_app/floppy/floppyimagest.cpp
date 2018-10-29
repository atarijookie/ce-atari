// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <string.h>
#include "floppyimagest.h"
#include "../debug.h"

bool FloppyImageSt::open(const char *fileName)
{
    if(!FloppyImage::open(fileName))
        return false;

    if(!loadImageIntoMemory()) {        // load the whole image in memory to avoid later disk access
        close();
        return false;
    }

    calcParams();                       // calculate the params of this floppy

    Debug::out(LOG_DEBUG, "ST Image opened: %s", fileName);
    Debug::out(LOG_DEBUG, "ST Image params - %d tracks, %d sides, %d sectors per track", params.tracksNo, params.sidesNo, params.sectorsPerTrack);

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

    Debug::out(LOG_ERROR, "Couldn't guess the floppy params :(");
    return false;
}

bool FloppyImageSt::save(const char *fileName)
{
    //TODO: implement this later
    return true;
}
