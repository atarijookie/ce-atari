// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <stdio.h>
#include <string.h>

#include <fnmatch.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "findstorage.h"

TFindStorage::TFindStorage()
{
    buffer = new uint8_t[getSize()];
    clear();
}

TFindStorage::~TFindStorage()
{
    delete []buffer;
}

void TFindStorage::clear(void)
{
    maxCount    = getSize() / 23;
    count       = 0;
    dta         = 0;
}

int TFindStorage::getSize(void)
{
    return (1024*1024);
}

void TFindStorage::copyDataFromOther(TFindStorage *other)
{
    dta     = other->dta;
    count   = other->count;
    memcpy(buffer, other->buffer, count * 23);
}
