// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef FLOPPYIMAGE_H
#define FLOPPYIMAGE_H

#include <stdio.h>

#include <string>

#include "../global.h"
#include <stdint.h>

class FloppyImage
{
public:
    FloppyImage();
    virtual ~FloppyImage();

    bool isLoaded(void);
    const char *getFileName(void);
    bool gotUnsavedChanges(void);                   // return if we should call save before clear()
    uint32_t getLastWriteTime(void);                   // return time when last write happened

    virtual bool open(const char *fileName);        // open filename for reading
    virtual void close(void);                       // close file handle if open

    virtual bool save(void) = 0;                    // save data into currentFileName
    virtual void clear(void);                       // clear the data in memory
    virtual bool getParams(int &tracks, int &sides, int &sectorsPerTrack);

    virtual bool readSector(int track, int side, int sectorNo, uint8_t *buffer);
    virtual bool writeSector(int track, int side, int sectorNo, uint8_t *buffer);
    virtual bool readNotWriteSector(bool readNotWrite, int track, int side, int sectorNo, uint8_t *buffer);

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
        uint8_t *data;
    } image;

    FILE *fajl;

    std::string currentFileName;
    bool loadedFlag;

    int  sectorsWritten;        // count of sectors that have been written to image in memory and not written yet - cleared by save()
    uint32_t lastWriteTime;        // when did the last write occure (to avoid saving to disk too often)
};

#endif // FLOPPYIMAGE_H
