#include <string.h>
#include "floppyimagemsa.h"
#include "../debug.h"

FloppyImageMsa::FloppyImageMsa()
{
    openFlag        = false;
    params.isInit   = false;

    image.data = NULL;
    image.size = 0;
}

FloppyImageMsa::~FloppyImageMsa()
{
    close();
}

bool FloppyImageMsa::open(char *fileName)
{
    close();

    fajl = fopen(fileName, "rb");

    if(fajl == NULL) {
        Debug::out("Failed to open image file: %s", fileName);
        openFlag = false;
        return false;
    }

    // Read header
    WORD id, spt, sides, trackStart, trackEnd;
    fread(&id,          2,1,fajl);      SWAPBYTES(id);
    fread(&spt,         2,1,fajl);      SWAPBYTES(spt);
    fread(&sides,       2,1,fajl);      SWAPBYTES(sides);
    fread(&trackStart,  2,1,fajl);      SWAPBYTES(trackStart);
    fread(&trackEnd,    2,1,fajl);      SWAPBYTES(trackEnd);

    if(id != 0x0e0f) {          // MSA ID mismatch?
        close();
        return false;
    }

    params.tracksNo         = trackEnd - trackStart + 1;
    params.sidesNo          = sides + 1;
    params.sectorsPerTrack  = spt;
    params.isInit           = true;

    openFlag = true;
    if(!loadImageIntoMemory()) {        // load the whole image in memory to avoid later disk access
        close();
        return false;
    }

    getTrackStartOffsets();

    Debug::out("MSA Image opened: %s", fileName);
    Debug::out("MSA Image params - %d tracks, %d sides, %d sectors per track", params.tracksNo, params.sidesNo, params.sectorsPerTrack);

    return true;
}

void FloppyImageMsa::getTrackStartOffsets(void)
{
    int pos = 10;

    int totalTracks = params.tracksNo * params.sidesNo;

    for(int t=0; t<totalTracks; t++) {
        trackOffset[t] = pos + 2;

        WORD *tlp = (WORD *) &image.data[pos];
        WORD trackLength = *tlp;
        SWAPBYTES(trackLength);

        pos += trackLength + 2;
    }
}

void FloppyImageMsa::SWAPBYTES(WORD &w)
{
    WORD a,b;

    a = w >> 8;         // get top
    b = w  & 0xff;      // get bottom

    w = (b << 8) | a;   // store swapped
}

bool FloppyImageMsa::loadImageIntoMemory(void)
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

bool FloppyImageMsa::isOpen(void)
{
    return openFlag;                    // just return status
}

void FloppyImageMsa::close()
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

bool FloppyImageMsa::getParams(int &tracks, int &sides, int &sectorsPerTrack)
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

bool FloppyImageMsa::readSector(int track, int side, int sectorNo, BYTE *buffer)
{
    if(!openFlag) {                                             // not open?
        return false;
    }

    if(sectorNo < 1 || sectorNo > params.sectorsPerTrack) {     // sector # out of range?
        return false;
    }

    int trackIndex  = track * params.sidesNo + side;
    int trackOfs    = trackOffset[trackIndex];
    int totalOfs    = trackOfs + ((sectorNo - 1) * 512);

    memcpy(buffer, &image.data[totalOfs], 512);
    return true;
}


