// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef FLOPPYIMAGE_H
#define FLOPPYIMAGE_H

#include <stdio.h>

#include <string>

#include "../global.h"
#include "../datatypes.h"

class FloppyImage
{
public:
    FloppyImage();
    virtual ~FloppyImage();

    bool isOpen(void);
    const char *getFileName(void);

    virtual bool open(const char *fileName);
    virtual bool save(const char *fileName) = 0;
    virtual void close();
    virtual bool getParams(int &tracks, int &sides, int &sectorsPerTrack);
    virtual bool readSector(int track, int side, int sectorNo, BYTE *buffer);

protected:
    virtual bool loadImageIntoMemory(void);

    struct {
        int tracksNo;
        int sidesNo;
        int sectorsPerTrack;
        bool isInit;
    } params;

    struct {
        int size;
        BYTE *data;
    } image;

    FILE *fajl;

private:
    std::string currentFileName;
    bool openFlag;
};

#endif // FLOPPYIMAGE_H
