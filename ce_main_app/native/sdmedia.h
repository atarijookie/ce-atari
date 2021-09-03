#ifndef _SDMEDIA_H
#define _SDMEDIA_H

#include <stdio.h>
#include <stdint.h>
#include "imedia.h"

class SdMedia: public IMedia
{
public:
    SdMedia();
    virtual ~SdMedia();

    virtual bool iopen(const char *path, bool createIfNotExists);
    virtual void iclose(void);

    virtual bool isInit(void);
    virtual bool mediaChanged(void);
    virtual void setMediaChanged(bool changed);
    virtual void getCapacity(int64_t &bytes, int64_t &sectors);

    virtual bool readSectors(int64_t sectorNo, uint32_t count, uint8_t *bfr);
    virtual bool writeSectors(int64_t sectorNo, uint32_t count, uint8_t *bfr);

    virtual uint32_t maxSectorsForSmallReadWrite(void);    // what will be the maximum value where small transfer is used (single transfer to/from media + single transfer to/from ST) and where the larger transfer is used above that
    virtual uint32_t maxSectorsForSingleReadWrite(void);   // what is the maximum sector count we can use on readSectors() and writeSectors()
private:
    bool    isInitialized;
    bool    mediaChangedFlag;
    int64_t capacityInSectors;
    int64_t capacityInBytes;
    uint8_t    senseKey;
};

#endif // _SDMEDIA_H
