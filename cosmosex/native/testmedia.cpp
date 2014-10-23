#include <stdio.h>
#include <string.h>

#include "testmedia.h"
#include "../debug.h"

TestMedia::TestMedia()
{

}

TestMedia::~TestMedia()
{
    iclose();
}

bool TestMedia::iopen(char *path, bool createIfNotExists)
{
    return true;
}

void TestMedia::iclose(void)
{
}

bool TestMedia::isInit(void)
{
    return true;
}

bool TestMedia::mediaChanged(void)
{
    return false;
}

void TestMedia::setMediaChanged(bool changed)
{

}

void TestMedia::getCapacity(int64_t &bytes, int64_t &sectors)
{
    bytes   = 512;
    sectors = 1;
}

bool TestMedia::readSectors(int64_t sectorNo, DWORD count, BYTE *bfr)
{
	DWORD i;
    DWORD byteCount = count * 512;
	
	for(i=0; i<byteCount; i++) {				// fill buffer with counter
		bfr[i] = i;
	}	
	
    return true;
}

bool TestMedia::writeSectors(int64_t sectorNo, DWORD count, BYTE *bfr)
{

    return true;
}
