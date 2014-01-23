#ifndef DATAMEDIA_H
#define DATAMEDIA_H

#include <stdio.h>
#include "../datatypes.h"
#include "imedia.h"

class ImageFileMedia: public IMedia
{
public:
    ImageFileMedia();
    ~ImageFileMedia();

    virtual bool iopen(char *path, bool createIfNotExists);
    virtual void iclose(void);

    virtual bool isInit(void);
    virtual bool mediaChanged(void);
    virtual void setMediaChanged(bool changed);
    virtual void getCapacity(DWORD &bytes, DWORD &sectors);

    virtual bool readSectors(DWORD sectorNo, DWORD count, BYTE *bfr);
    virtual bool writeSectors(DWORD sectorNo, DWORD count, BYTE *bfr);

private:

    DWORD	BCapacity;			// device capacity in bytes
    DWORD	SCapacity;			// device capacity in sectors

    bool    mediaHasChanged;

    FILE *image;
};

#endif // DATAMEDIA_H
