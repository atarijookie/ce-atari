#include <stdio.h>
#include <string.h>

#include "sdmedia.h"
#include "scsi_defs.h"
#include "../debug.h"

SdMedia::SdMedia()
{
	capacityInSectors	= 0;
	mediaChangedFlag	= false;
	senseKey			= SCSI_E_NoSense;
}

SdMedia::~SdMedia()
{
}

void SdMedia::setCurrentCapacity(DWORD sectors)
{
	if(capacityInSectors != sectors) {				// if the number of sectors changed, then media changed
		capacityInSectors	= sectors;
		mediaChangedFlag	= true;
	}
}

bool SdMedia::iopen(char *path, bool createIfNotExists)
{
    return true;
}

void SdMedia::iclose(void)
{
}

bool SdMedia::isInit(void)
{
    if(capacityInSectors != 0) {					// got card capacity? It's initialized...
		return true;
	}
	
	return false;
}

bool SdMedia::mediaChanged(void)
{
    return mediaChangedFlag;
}

void SdMedia::setMediaChanged(bool changed)
{
	mediaChangedFlag = changed;
}

void SdMedia::getCapacity(int64_t &bytes, int64_t &sectors)
{
    bytes   = capacityInSectors << 9;
    sectors = capacityInSectors;
}

bool SdMedia::readSectors(int64_t sectorNo, DWORD count, BYTE *bfr)
{
	// you should never call this, and if you do, you will fail
    return false;
}

bool SdMedia::writeSectors(int64_t sectorNo, DWORD count, BYTE *bfr)
{
	// you should never call this, and if you do, you will fail
    return false;
}
