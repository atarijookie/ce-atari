#ifndef _IMAGEFILEMEDIA_H_
#define _IMAGEFILEMEDIA_H_

#include <stdio.h>
#include "../datatypes.h"
#include "imedia.h"

class ImageFileMedia: public IMedia
{
public:
    ImageFileMedia();
    virtual ~ImageFileMedia();

    virtual bool iopen(const char *path, bool createIfNotExists);
    virtual void iclose(void);

    virtual bool isInit(void);
    virtual bool mediaChanged(void);
    virtual void setMediaChanged(bool changed);
    virtual void getCapacity(int64_t &bytes, int64_t &sectors);

    virtual bool readSectors(int64_t sectorNo, DWORD count, BYTE *bfr);
    virtual bool writeSectors(int64_t sectorNo, DWORD count, BYTE *bfr);

private:

    int64_t	BCapacity;			// device capacity in bytes
    int64_t	SCapacity;			// device capacity in sectors

    bool    mediaHasChanged;

    FILE *image;
};

#endif // _IMAGEFILEMEDIA_H_
