#ifndef _SECTORFIFO_H_
#define _SECTORFIFO_H_

#include "../datatypes.h"

#define SECTOR_FIFO_SIZE    8

class SectorFIFO {
public:
    SectorFIFO(void);           // constructor

    void clear(void);           // empty whole FIFO

    bool isFull(void);          // true when FULL  - shouldn't write
    bool isEmpty(void);         // true when EMPTY - shouldn't read

    void write(BYTE *data);     // add 512 bytes to   FIFO
    void read(BYTE *data);      // get 512 bytes from FIFO

private:
    int storedCount;
    int writeIndex;
    int readIndex;

    BYTE fifoData[SECTOR_FIFO_SIZE * 512];
};

#endif // _SECTORFIFO_H_
