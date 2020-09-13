#include <stdio.h>
#include <string.h>

#include "sdmedia.h"
#include "scsi_defs.h"
#include "../debug.h"
#include "../chipinterface_v3/sdthread.h"

SdMedia::SdMedia()
{
    capacityInSectors   = 0;
    mediaChangedFlag    = false;
    senseKey            = SCSI_E_NoSense;
}

SdMedia::~SdMedia()
{
}

bool SdMedia::iopen(const char *path, bool createIfNotExists)
{
    return true;
}

void SdMedia::iclose(void)
{
}

bool SdMedia::isInit(void)
{
    // get card info from the other thread
    sdthread_getCardInfo(isInitialized, mediaChangedFlag, capacityInBytes, capacityInSectors);

    return isInitialized;
}

bool SdMedia::mediaChanged(void)
{
    // get card info from the other thread
    sdthread_getCardInfo(isInitialized, mediaChangedFlag, capacityInBytes, capacityInSectors);

    return mediaChangedFlag;
}

void SdMedia::setMediaChanged(bool changed)
{
    mediaChangedFlag = changed;
}

void SdMedia::getCapacity(int64_t &bytes, int64_t &sectors)
{
    // get card info from the other thread
    sdthread_getCardInfo(isInitialized, mediaChangedFlag, capacityInBytes, capacityInSectors);

    bytes   = capacityInBytes;
    sectors = capacityInSectors;
}

DWORD SdMedia::maxSectorsForSmallReadWrite(void)
{
    return 1;   // only for 1 sector transfers do a single transfer from / to SD and single transfer from / to ST, otherwise go with the larger tranfer mode, which will interleave SD and ST transfers
}

DWORD SdMedia::maxSectorsForSingleReadWrite(void)
{
    return 1;   // while one sector is being transfered from / to ST, another one is being transfered from / to SD to RPi
}

bool SdMedia::readSectors(int64_t sectorNo, DWORD count, BYTE *bfr)
{
    if(count > 1) {     // if requested count is greater than 1, fail
        return false;
    }

    // we ignore the sectorNo and count, as that has been sent to sdThread in startBackgroundTransfer()

    return sdthread_sectorDataGet(bfr);
}

bool SdMedia::writeSectors(int64_t sectorNo, DWORD count, BYTE *bfr)
{
    if(count > 1) {     // if requested count is greater than 1, fail
        return false;
    }

    // we ignore the sectorNo and count, as that has been sent to sdThread in startBackgroundTransfer()

    return sdthread_sectorDataPut(bfr);
}

// let media know the whole transfer params, so it can do some background pre-read to speed up the transfer
void SdMedia::startBackgroundTransfer(bool readNotWrite, int64_t sectorNo, DWORD count)
{
    sdthread_startBackgroundTransfer(readNotWrite, sectorNo, count);
}

// wait until the background transfer finishes and get the final success / failure
bool SdMedia::waitForBackgroundTransferFinish(void)
{
    return sdthread_waitUntilAllDataWritten();
}
