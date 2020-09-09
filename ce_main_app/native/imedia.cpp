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

// store sector count for this transfer, so media can do some background pre-read to speed up the transfer
void IMedia::setSectorCountForThisTransfer(DWORD sectorCount)
{
    sectorCountForThisTransfer = sectorCount;
}
