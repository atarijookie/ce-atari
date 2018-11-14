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
    bool gotUnsavedChanges(void);                   // return if we should call save before clear()

    virtual bool open(const char *fileName);        // open filename for reading
    virtual void close(void);                       // close file handle if open

    virtual bool save(void) = 0;                    // save data into currentFileName
    virtual void clear(void);                       // clear the data in memory
    virtual bool getParams(int &tracks, int &sides, int &sectorsPerTrack);

    virtual bool readSector(int track, int side, int sectorNo, BYTE *buffer);
    virtual bool writeSector(int track, int side, int sectorNo, BYTE *buffer);
    virtual bool readNotWriteSector(bool readNotWrite, int track, int side, int sectorNo, BYTE *buffer);

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

    std::string currentFileName;
    bool loadedFlag;
    int  sectorsWritten;        // count of sectors that have been written to image in memory and not written yet - cleared by save()
};

#endif // FLOPPYIMAGE_H
