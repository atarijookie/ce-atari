#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "datatypes.h"
#include "cyclicbuff.h"
#include "debug.h"

void CyclicBuff::init(void)
{
    count    = 0;
    addPos   = 0;
    getPos   = 0;
    size     = CYCLIC_BUF_SIZE;
}

void CyclicBuff::add(BYTE val)
{
    if(count >= CYCLIC_BUF_SIZE) {  // buffer full? quit
        return;
    }
    count++;                        // update count

    buf[addPos] = val;              // store data

    addPos++;                       // update 'add' position
    addPos = addPos & CYCLIC_BUF_MASK;
}

BYTE CyclicBuff::get(void)
{
    if(count == 0) {                // buffer empty? quit
        return 0;
    }
    count--;                        // update count

    BYTE val = buf[getPos];         // get data

    getPos++;                       // update 'get' position
    getPos = getPos & CYCLIC_BUF_MASK;

    return val;
}

BYTE CyclicBuff::peek(void)
{
    if(count == 0) {                // buffer empty? 
        return 0;
    }

    BYTE val = buf[getPos];         // just get the data
    return val;
}

BYTE CyclicBuff::peekWithOffset(int offset)
{
    if(offset >= count) {                                           // not enough data in buffer? quit
        return 0;
    }

    int getPosWithOffet = (getPos + offset) & CYCLIC_BUF_MASK;      // calculate get position

    BYTE val = buf[getPosWithOffet];                                // get data
    return val;
}

