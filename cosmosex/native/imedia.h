#ifndef IMEDIA_H
#define IMEDIA_H

#include <stdio.h>
#include "../datatypes.h"

class IMedia
{
public:
    virtual bool open(char *path, bool createIfNotExists) = 0;
    virtual void close(void) = 0;

    virtual bool isInit(void) = 0;
    virtual bool mediaChanged(void) = 0;
    virtual void setMediaChanged(bool changed) = 0;
    virtual void getCapacity(DWORD &bytes, DWORD &sectors) = 0;

    virtual bool readSectors(DWORD sectorNo, DWORD count, BYTE *bfr) = 0;
    virtual bool writeSectors(DWORD sectorNo, DWORD count, BYTE *bfr) = 0;
};

#endif // IMEDIA_H
