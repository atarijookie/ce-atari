#ifndef FLOPPYIMAGEMSA_H
#define FLOPPYIMAGEMSA_H

#include <stdio.h>
#include "ifloppyimage.h"

#include "../datatypes.h"

class FloppyImageMsa: public IFloppyImage
{
public:
    FloppyImageMsa();
    virtual ~FloppyImageMsa();

    virtual bool open(const char *fileName);
    virtual bool isOpen(void);
    virtual void close();
    virtual bool getParams(int &tracks, int &sides, int &sectorsPerTrack);
    virtual bool readSector(int track, int side, int sectorNo, BYTE *buffer);
    virtual char *getFileName(void);

private:
    char currentFileName[512];

    bool openFlag;

    struct {
        int tracksNo;
        int sidesNo;
        int sectorsPerTrack;

        bool isInit;
    } params;

    bool loadImageIntoMemory(void);

    struct {
        int size;
        BYTE *data;
    } image;

    FILE *fajl;
};

#endif // FLOPPYIMAGEST_H
