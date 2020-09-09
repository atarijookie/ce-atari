#ifndef IMEDIA_H
#define IMEDIA_H

#include <stdio.h>
#include "../datatypes.h"

// The following value will be used as sector limit for most of the large read / write operations. (1 MB in sectors)
// Some data media (e.g. SD media) might prefer smaller size than this.
// Make sure no media returns anything larger than this, becuase it will be used as buffer size in scsi.h (for the largest possible buffer).
#define MAXIMUM_SECTOR_COUNT_LANGE  2048

class IMedia
{
public:
    virtual ~IMedia()   { };

    virtual bool iopen(const char *path, bool createIfNotExists) = 0;
    virtual void iclose(void) = 0;

    virtual bool isInit(void) = 0;
    virtual bool mediaChanged(void) = 0;
    virtual void setMediaChanged(bool changed) = 0;
    virtual void getCapacity(int64_t &bytes, int64_t &sectors) = 0;

    virtual bool readSectors(int64_t sectorNo, DWORD count, BYTE *bfr) = 0;
    virtual bool writeSectors(int64_t sectorNo, DWORD count, BYTE *bfr) = 0;

    // the following methods will be the same for most children of IMedia, but we need it different for SdMedia.
    virtual DWORD maxSectorsForSmallReadWrite(void);             // what will be the maximum value where small transfer is used (single transfer to/from media + single transfer to/from ST) and where the larger transfer is used above that
    virtual DWORD maxSectorsForSingleReadWrite(void);            // what is the maximum sector count we can use on readSectors() and writeSectors()
    virtual void  setSectorCountForThisTransfer(DWORD sectorCount);  // store sector count for this transfer, so media can do some background pre-read to speed up the transfer

private:
    DWORD sectorCountForThisTransfer;
};

#endif // IMEDIA_H
