#include "fifo.h"

#define MUTEX_LOCKED    1
#define MUTEX_OPEN      0
volatile uint8_t fifoMutex = MUTEX_OPEN;

/*
This is fifo for video frames. It stores pointer to new palette and pointer to new screen data.
fifoInit() and fifoAdd() are called from main thread, fifoGet() is called from the interrupt.
While fifoInit() and fifoAdd() are being called, the fifoGet() will return zeros in interrupt (but doesn't wait).
Also, fifoGet() will return zeros when the fifo is empty.
*/

void fifoInit(Fifo* fifo)
{
    uint8_t i;

    fifoMutex = MUTEX_LOCKED;

    fifo->count = 0;              // buffer now empty

    fifo->indexAdd = 0;
    fifo->indexGet = 0;

    // fill data with zeros
    for(i=0; i<FIFO_SIZE; i++) {
        fifo->data1[i] = 0;
        fifo->data2[i] = 0;
    }

    fifoMutex = MUTEX_OPEN;
}

void fifoAdd(Fifo* fifo, uint32_t val1, uint32_t val2)
{
    if(fifo->count >= FIFO_SIZE) {  // fifo full? quit
        return;
    }

    fifoMutex = MUTEX_LOCKED;

    fifo->count++;

    // store data at the right position
    fifo->data1[fifo->indexAdd] = val1;
    fifo->data2[fifo->indexAdd] = val2;

    fifo->indexAdd++;

    if(fifo->indexAdd >= FIFO_SIZE) {    // if reached end of buffer, go to start
        fifo->indexAdd = 0;
    }

    fifoMutex = MUTEX_OPEN;
}

void fifoGet(Fifo* fifo, uint32_t* val1, uint32_t* val2)
{
    if(fifo->count == 0 || fifoMutex == MUTEX_LOCKED) {  // fifo empty or mutex locked? return zeros and quit
        *val1 = 0;
        *val2 = 0;
        return;
    }

    fifo->count--;

    // get data from fifo
    *val1 = fifo->data1[fifo->indexGet];
    *val2 = fifo->data2[fifo->indexGet];

    fifo->indexGet++;

    if(fifo->indexGet >= FIFO_SIZE) {    // if reached end of buffer, go to start
        fifo->indexGet = 0;
    }
}
