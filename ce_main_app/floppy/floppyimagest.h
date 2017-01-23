// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef FLOPPYIMAGEST_H
#define FLOPPYIMAGEST_H

#include <stdio.h>
#include "ifloppyimage.h"

#include "../datatypes.h"

class FloppyImageSt: public IFloppyImage
{
public:
    FloppyImageSt();
    virtual ~FloppyImageSt();

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

    bool calcParams(void);
    bool loadImageIntoMemory(void);

    FILE *fajl;

    struct {
        int size;
        BYTE *data;
    } image;
};

#endif // FLOPPYIMAGEST_H
