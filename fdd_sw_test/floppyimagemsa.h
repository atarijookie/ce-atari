#ifndef FLOPPYIMAGEMSA_H
#define FLOPPYIMAGEMSA_H

#include <stdio.h>
#include "ifloppyimage.h"

#include "datatypes.h"

class FloppyImageMsa: public IFloppyImage
{
public:
    FloppyImageMsa();
    ~FloppyImageMsa();

    virtual bool open(char *fileName);
    virtual bool isOpen(void);
    virtual void close();
    virtual bool getParams(int &tracks, int &sides, int &sectorsPerTrack);
    virtual bool readSector(int track, int side, int sectorNo, BYTE *buffer);

private:
    bool openFlag;

    struct {
        int tracksNo;
        int sidesNo;
        int sectorsPerTrack;

        bool isInit;
    } params;

    void SWAPBYTES(WORD &w);
    void getTrackStartOffsets(void);
    bool loadImageIntoMemory(void);

    struct {
        int size;
        BYTE *data;
    } image;

    DWORD trackOffset[200];
    FILE *fajl;
};

#endif // FLOPPYIMAGEST_H
