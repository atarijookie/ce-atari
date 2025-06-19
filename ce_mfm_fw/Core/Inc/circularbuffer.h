#ifndef CIRCULARBUFFER_H_
#define CIRCULARBUFFER_H_

#include <stdint.h>

#define CIRCBUFFER_SIZE        128

typedef struct {
    volatile uint8_t count;    // count of data stored

    volatile uint8_t *pAdd;    // pointer where data will be stored
    volatile uint8_t *pGet;    // pointer from where data will be get

    volatile uint8_t data[CIRCBUFFER_SIZE];
} TCircBuffer;

void circularInit(TCircBuffer *cb);
void cicrularAdd(TCircBuffer *cb, uint8_t val);
uint8_t cicrularGet(TCircBuffer *cb);

#endif
