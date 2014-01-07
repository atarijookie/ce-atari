#ifndef NOMEDIA_H
#define NOMEDIA_H

#include <stdio.h>
#include "../datatypes.h"
#include "imedia.h"

class NoMedia: public IMedia
{
public:
    NoMedia();
    ~NoMedia();

    virtual bool open(char *path, bool createIfNotExists);
    virtual void close(void);

    virtual bool isInit(void);
    virtual bool mediaChanged(void);
    virtual void setMediaChanged(bool changed);
    virtual void getCapacity(DWORD &bytes, DWORD &sectors);

    virtual bool readSectors(DWORD sectorNo, DWORD count, BYTE *bfr);
    virtual bool writeSectors(DWORD sectorNo, DWORD count, BYTE *bfr);
};

#endif // NOMEDIA_H
