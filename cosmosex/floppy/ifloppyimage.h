#ifndef FLOPPYIMAGE_H
#define FLOPPYIMAGE_H

#include "../global.h"
#include "../datatypes.h"

class IFloppyImage
{
public:
    virtual bool open(char *fileName) = 0;
    virtual bool isOpen(void) = 0;
    virtual void close() = 0;
    virtual bool getParams(int &tracks, int &sides, int &sectorsPerTrack) = 0;
    virtual bool readSector(int track, int side, int sectorNo, BYTE *buffer) = 0;
};

#endif // FLOPPYIMAGE_H
