#include <stdio.h>
#include "../datatypes.h"
#include "imedia.h"

// what will be the maximum value where small transfer is used (single transfer to/from 
// media + single transfer to/from ST) and where the larger transfer is used above that
DWORD IMedia::maxSectorsForSmallReadWrite(void)
{
    return MAXIMUM_SECTOR_COUNT_LANGE;
}

// what is the maximum sector count we can use on readSectors() and writeSectors()
DWORD IMedia::maxSectorsForSingleReadWrite(void)
{
    return MAXIMUM_SECTOR_COUNT_LANGE;
}

// let media know the whole transfer params, so it can do some background pre-read to speed up the transfer
void IMedia::startBackgroundTransfer(bool readNotWrite, int64_t sectorNo, DWORD count)
{
    // override this method in child class if the media type supports background transfer
}

// wait until all the data in the background transfer are finished moving around
bool IMedia::waitForBackgroundTransferFinish(void)
{
    // override this method in child class if the media type supports background transfer

    return true;    // pretend success if not overriden
}
