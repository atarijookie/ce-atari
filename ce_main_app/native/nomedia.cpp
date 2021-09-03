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

bool NoMedia::iopen(const char *path, bool createIfNotExists)
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

void NoMedia::getCapacity(int64_t &bytes, int64_t &sectors)
{
    bytes   = 0;
    sectors = 0;
}

bool NoMedia::readSectors(int64_t sectorNo, uint32_t count, uint8_t *bfr)
{
    return true;
}

bool NoMedia::writeSectors(int64_t sectorNo, uint32_t count, uint8_t *bfr)
{
    return true;
}
