#ifndef DATAMEDIA_H
#define DATAMEDIA_H

#include <stdio.h>
#include "../datatypes.h"

class DataMedia
{
public:
    DataMedia();
    ~DataMedia();

    bool open(char *path, bool createIfNotExists);
    void close(void);

    bool isInit(void);
    bool mediaChanged(void);
    void setMediaChanged(bool changed);
    void getCapacity(DWORD &bytes, DWORD &sectors);

    bool readSectors(DWORD sectorNo, DWORD count, BYTE *bfr);
    bool writeSectors(DWORD sectorNo, DWORD count, BYTE *bfr);

private:

    DWORD	BCapacity;			// device capacity in bytes
    DWORD	SCapacity;			// device capacity in sectors

    bool    mediaHasChanged;

    FILE *image;
};

#endif // DATAMEDIA_H
