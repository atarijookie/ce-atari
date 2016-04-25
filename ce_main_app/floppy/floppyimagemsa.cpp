#include <stdlib.h>
#include <string.h>
#include "floppyimagemsa.h"
#include "msa.h"
#include "../debug.h"
#include "../utils.h"

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
        Debug::out(LOG_ERROR, "Failed to open image file: %s", fileName);
        openFlag = false;
        return false;
    }

    // Read header
    WORD id, spt, sides, trackStart, trackEnd;
    fread(&id,          2,1,fajl);      Utils::SWAPWORD(id);
    fread(&spt,         2,1,fajl);      Utils::SWAPWORD(spt);
    fread(&sides,       2,1,fajl);      Utils::SWAPWORD(sides);
    fread(&trackStart,  2,1,fajl);      Utils::SWAPWORD(trackStart);
    fread(&trackEnd,    2,1,fajl);      Utils::SWAPWORD(trackEnd);

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

    Debug::out(LOG_DEBUG, "MSA Image opened: %s", fileName);
    Debug::out(LOG_DEBUG, "MSA Image params - %d tracks, %d sides, %d sectors per track", params.tracksNo, params.sidesNo, params.sectorsPerTrack);

    return true;
}

bool FloppyImageMsa::loadImageIntoMemory(void)
{
    if(image.data != NULL) {
        free(image.data);
        image.data = NULL;
        image.size = 0;
    }

    fseek(fajl, 0, SEEK_END);                   // move to the end of file
    int cnt = ftell(fajl);                      // get the file size
    fseek(fajl, 0, SEEK_SET);                   // move to the start of file

    image.data = (BYTE *) malloc(cnt);          // allocate memory for file reading
    int res = fread(image.data, 1, cnt, fajl);  // read the file into memory
    image.size = cnt;

    if(res != cnt) {                            // if failed to read all, quit
        free(image.data);
        image.data = NULL;
        image.size = 0;

        return false;
    }

	DWORD imageSize = 0;
    BYTE *pDiskBuffer = MSA_UnCompress(image.data, (long int *) &imageSize);

    free(image.data);                           // free the memory which was used for file reading

    image.data = pDiskBuffer;                   // store pointer to buffer with decompressed image and size
    image.size = imageSize;
    
    if(image.data == NULL) {                    // if the MSA_UnCompress failed, error
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
        free(image.data);
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

    int offset   = track    * (params.sidesNo * params.sectorsPerTrack);    // move to the right track
    offset      += side     * params.sectorsPerTrack;                       // then move to the right side
    offset      += (sectorNo - 1);                                          // and move a little to the right sector
    offset       = offset * 512;                                            // calculate ofsset in bytes

    memcpy(buffer, &image.data[offset], 512);
    return true;
}


