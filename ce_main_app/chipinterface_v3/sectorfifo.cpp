#include <string.h>

#include "sectorfifo.h"

SectorFIFO::SectorFIFO(void)
{
    clear();
}

void SectorFIFO::clear(void)            // empty whole FIFO
{
    storedCount = 0;
    writeIndex = 0;
    readIndex = 0;
}

bool SectorFIFO::isFull(void)           // true when FULL  - shouldn't write
{
    return (storedCount >= SECTOR_FIFO_SIZE);
}

bool SectorFIFO::isEmpty(void)          // true when EMPTY - shouldn't read
{
    return (storedCount == 0);
}

void SectorFIFO::write(BYTE *data)      // add 512 bytes to FIFO
{
    if(isFull()) {                      // when FULL - don't write
        return;
    }

    int startIndex = writeIndex * 512;          // calculate starting index in whole buffer for this sector write index
    memcpy(&fifoData[startIndex], data, 512);   // copy in data

    storedCount++;                              // increment stored count

    writeIndex++;                               // advance write index

    if(writeIndex >= SECTOR_FIFO_SIZE) {        // if index reached end, restart it
        writeIndex = 0;
    }
}

void SectorFIFO::read(BYTE *data)               // get 512 bytes from FIFO
{
    if(isEmpty()) {                             // when EMPTY - don't read
        return;
    }

    int startIndex = readIndex * 512;           // calculate starting index in whole buffer for this sector read index
    memcpy(data, &fifoData[startIndex], 512);   // copy out data

    storedCount--;                              // decrement stored count

    readIndex++;                                // advance read index

    if(readIndex >= SECTOR_FIFO_SIZE) {         // if index reached end, restart it
        readIndex = 0;
    }
}
