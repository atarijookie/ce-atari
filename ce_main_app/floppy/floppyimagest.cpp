// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <string.h>
#include "floppyimagest.h"
#include "../debug.h"

bool FloppyImageSt::open(const char *fileName)
{
    loadedFlag = false;

    if(!FloppyImage::open(fileName))
        return false;

    bool res = loadImageIntoMemory();   // load the whole image in memory to avoid later disk access
    close();                            // close the file, all the needed data is now in memory

    if(!res) {                          // if failed to load image, quit and fail
        clear();
        return false;
    }

    calcParams();                       // calculate the params of this floppy

    loadedFlag = true;

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

bool FloppyImageSt::save(void)
{
    sectorsWritten = 0;                 // clear unwritten sectors counter

    if(!isLoaded()) {                   // nothing in memory? fail
        return false;
    }

    FILE *f = fopen(currentFileName.c_str(), "wb");    // open

    if(!f) {                            // failed?
        return false;
    }

    int res = fwrite(image.data, 1, image.size, f); // write

    fclose(f);                          // close
    return (res == image.size);         // success if written as much as should
}

bool FloppyImageSt::createNewImage(std::string pathAndFile)
{
    // open the file
    FILE *f = fopen(pathAndFile.c_str(), "wb");

    if(!f) {                                            // failed to open file?
        Debug::out(LOG_ERROR, "FloppySetup::newImage - failed to open file %s", pathAndFile.c_str());
        return false;
    }

    // create default boot sector (copied from blank .st image created in Steem)
    BYTE sect0start[]   = {0xeb, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0xc8, 0x82, 0x75, 0x00, 0x02, 0x02, 0x01, 0x00,
                           0x02, 0x70, 0x00, 0xa0, 0x05, 0xf9, 0x05, 0x00,
                           0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
    BYTE sect0end[]     = {0x00, 0x97, 0xc7};

    BYTE bfr[512];
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
