#include <stdio.h>
#include <string.h>

#include "nomedia.h"
#include "../debug.h"

NoMedia::NoMedia()
{
}

NoMedia::~NoMedia()
{
}

bool NoMedia::iopen(char *path, bool createIfNotExists)
{
    return true;
}

void NoMedia::iclose(void)
{
}

bool NoMedia::isInit(void)
{
    return true;
}

bool NoMedia::mediaChanged(void)
{
    return false;
}

void NoMedia::setMediaChanged(bool changed)
{

}

void NoMedia::getCapacity(DWORD &bytes, DWORD &sectors)
{
    bytes   = 0;
    sectors = 0;
}

bool NoMedia::readSectors(DWORD sectorNo, DWORD count, BYTE *bfr)
{
    return true;
}

bool NoMedia::writeSectors(DWORD sectorNo, DWORD count, BYTE *bfr)
{
    return true;
}
