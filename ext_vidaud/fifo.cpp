#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stdint.h>
#include "fifo.h"
#include "utils.h"

Fifo::Fifo(void)
{
    count = 0;
    addPos = 0;
    getPos = 0;
}

void Fifo::add(uint8_t val)
{
    if(freeBytes() == 0) {          // buffer full? quit
        return;
    }
    count++;                        // update count

    buf[addPos] = val;              // store data

    addPos++;                       // update 'add' position
    addPos = addPos & CYCLIC_BUF_MASK;
}

uint8_t Fifo::get(void)
{
    if(usedBytes() == 0) {          // buffer empty? quit
        return 0;
    }
    count--;                        // update count

    uint8_t val = buf[getPos];      // get data

    getPos++;                       // update 'get' position
    getPos = getPos & CYCLIC_BUF_MASK;

    return val;
}

uint32_t Fifo::usedBytes(void)
{
    return count;
}

uint32_t Fifo::freeBytes(void)
{
    return (CYCLIC_BUF_SIZE - count);
}

void Fifo::addBfr(uint8_t* bfr, uint32_t size)
{
    uint32_t freeCnt = freeBytes();
    if(freeCnt == 0) {              // buffer full? quit
        return;
    }

    uint32_t storeCnt = MIN(freeCnt, size); // pick smaller if cannot fit into free space

    if((addPos + storeCnt) >= CYCLIC_BUF_SIZE) {        // if storing near the end and have to store the rest at the start
        uint32_t cntToEnd = CYCLIC_BUF_SIZE - addPos;   // how many we can store until the end
        uint32_t cntAtStart = storeCnt - cntToEnd;      // how many we will then store at the start

        memcpy(&buf[addPos], bfr,            cntToEnd);     // first part at the end
        memcpy(&buf[0],      bfr + cntToEnd, cntAtStart);   // second part at the start
        addPos = cntAtStart;                                // next time store after the part at the start
        count += storeCnt;                                  // we've added whole storeCount
    } else {    // storing of data will not go beyond the end
        memcpy(&buf[addPos], bfr, storeCnt);
        addPos += storeCnt;
        count += storeCnt;
    }
}

void Fifo::getBfr(uint8_t* bfr, uint32_t size)
{
    uint32_t usedCnt = usedBytes();

    if(usedCnt == 0) {              // buffer empty? quit
        return;
    }

    uint32_t getCnt = MIN(usedCnt, size);               // pick smaller if cannot get all the requested bytes

    if((getPos + getCnt) >= CYCLIC_BUF_SIZE) {          // if getting near the end and have to get the rest at the start
        uint32_t cntToEnd = CYCLIC_BUF_SIZE - getPos;   // how many we can get until the end
        uint32_t cntAtStart = getCnt - cntToEnd;        // how many we will then get at the start

        memcpy(bfr,            &buf[getPos], cntToEnd);     // first part at the end
        memcpy(bfr + cntToEnd, &buf[0],      cntAtStart);   // second part at the start
        getPos = cntAtStart;                                // next time get after the part at the start
        count -= getCnt;                                    // we've retrieved whole getCount
    } else {    // getting of data will not go beyond the end
        memcpy(bfr, &buf[getPos], getCnt);
        getPos -= getCnt;
        count -= getCnt;
    }
}

