#ifndef IMEDIA_H
#define IMEDIA_H

#include <stdio.h>
#include "../datatypes.h"

class IMedia
{
public:
	virtual ~IMedia()	{ };

    virtual bool iopen(char *path, bool createIfNotExists) = 0;
    virtual void iclose(void) = 0;

    virtual bool isInit(void) = 0;
    virtual bool mediaChanged(void) = 0;
    virtual void setMediaChanged(bool changed) = 0;
    virtual void getCapacity(int64_t &bytes, int64_t &sectors) = 0;

    virtual bool readSectors(int64_t sectorNo, DWORD count, BYTE *bfr) = 0;
    virtual bool writeSectors(int64_t sectorNo, DWORD count, BYTE *bfr) = 0;
};

#endif // IMEDIA_H
