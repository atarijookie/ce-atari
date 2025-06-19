#include "circularbuffer.h"

void circularInit(TCircBuffer *cb)
{
    uint16_t i;

    cb->count = 0;              // buffer now empty

    cb->pAdd = &cb->data[0];    // 'add' pointer on start
    cb->pGet = &cb->data[0];    // 'get' pointer on start

    // fill data with zeros
    for(i=0; i<CIRCBUFFER_SIZE; i++) {
        cb->data[i] = 0;
    }
}

void cicrularAdd(TCircBuffer *cb, uint8_t val)
{
    // intentionally removed check of cb->count, as full buffer will fail by not storing (with check) or with overwrite (without check)
    cb->count++;

    // store data at the right position
    *cb->pAdd = val;
    cb->pAdd++;

    if(cb->pAdd >= &cb->data[CIRCBUFFER_SIZE]) {    // if reached end of buffer
        cb->pAdd = &cb->data[0];                    // go to start of buffer
    }
}

uint8_t cicrularGet(TCircBuffer *cb)
{
    uint8_t val;

    // intentionally removed check of cb->count, as that is checked where circularGet() is used
    cb->count--;

    // buffer not empty, get data
    val = *cb->pGet;
    cb->pGet++;

    if(cb->pGet >= &cb->data[CIRCBUFFER_SIZE]) {    // if reached end of buffer
        cb->pGet = &cb->data[0];                    // go to start of buffer
    }

    // return value from buffer
    return val;
}
