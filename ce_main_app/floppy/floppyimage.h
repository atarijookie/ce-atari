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

    bool isLoaded(void);
    const char *getFileName(void);

    virtual bool open(const char *fileName);        // open filename for reading
    virtual void close(void);                       // close file handle if open

    virtual bool save(const char *fileName) = 0;    // save data into file
    virtual void clear(void);                       // clear the data in memory
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
    bool loadedFlag;
};

#endif // FLOPPYIMAGE_H
