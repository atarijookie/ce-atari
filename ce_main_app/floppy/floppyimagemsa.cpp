// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <stdlib.h>
#include <string.h>
#include "floppyimagemsa.h"
#include "msa.h"
#include "../debug.h"
#include "../utils.h"

bool FloppyImageMsa::open(const char *fileName)
{
    if(!FloppyImage::open(fileName))
        return false;

    // Read header
    WORD id, spt, sides, trackStart, trackEnd;
    fread(&id,          2,1,fajl);      Utils::SWAPWORD(id);
    fread(&spt,         2,1,fajl);      Utils::SWAPWORD(spt);
    fread(&sides,       2,1,fajl);      Utils::SWAPWORD(sides);
    fread(&trackStart,  2,1,fajl);      Utils::SWAPWORD(trackStart);
    fread(&trackEnd,    2,1,fajl);      Utils::SWAPWORD(trackEnd);

    if(id != 0x0e0f) {          // MSA ID mismatch?
        close();                // close file
        clear();                // clear memory
        return false;
    }
    params.tracksNo         = trackEnd - trackStart + 1;
    params.sidesNo          = sides + 1;
    params.sectorsPerTrack  = spt;
    params.isInit           = true;

    bool res = loadImageIntoMemory();   // load the whole image in memory to avoid later disk access
    close();                            // close file - no need for it to be open

    if(!res) {              // load failed?
        clear();            // clear memory
        return false;
    }

    Debug::out(LOG_DEBUG, "MSA Image opened: %s", fileName);
    Debug::out(LOG_DEBUG, "MSA Image params - %d tracks, %d sides, %d sectors per track", params.tracksNo, params.sidesNo, params.sectorsPerTrack);

    return true;
}

bool FloppyImageMsa::loadImageIntoMemory(void)
{
    if(!FloppyImage::loadImageIntoMemory()) {
        return false;
    }

    long imageSize = 0;
    BYTE *pDiskBuffer = MSA_UnCompress(image.data, &imageSize);

    free(image.data);                           // free the memory which was used for file reading

    image.data = pDiskBuffer;                   // store pointer to buffer with decompressed image and size
    image.size = imageSize;

    if(image.data == NULL) {                    // if the MSA_UnCompress failed, error
        return false;
    }

    return true;
}

bool FloppyImageMsa::save(void)
{
    sectorsWritten = 0;                         // clear unwritten sectors counter
	return MSA_WriteDisk(currentFileName.c_str(), image.data, image.size);
}
