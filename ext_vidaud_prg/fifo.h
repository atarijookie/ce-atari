#ifndef __FIFO_H__
#define __FIFO_H__

#include <stdint.h>

#define FIFO_SIZE   20

typedef struct {
    volatile uint8_t count;    // count of data stored
    volatile uint8_t indexAdd;
    volatile uint8_t indexGet;
    volatile uint32_t data1[FIFO_SIZE];
    volatile uint32_t data2[FIFO_SIZE];
} Fifo;

void fifoInit(Fifo* fifo);
void fifoAdd(Fifo* fifo, uint32_t val1, uint32_t val2);
void fifoGet(Fifo* fifo, uint32_t* val1, uint32_t* val2);

#endif
