#ifndef _FIFO_H_
#define _FIFO_H_

#include <stdint.h>

#define CYCLIC_BUF_SIZE     1048576
#define CYCLIC_BUF_MASK     0xFFFFF

class Fifo {
public:
    Fifo(void);
    void clear(void);

    void add(uint8_t val);
    uint8_t get(void);

    uint32_t usedBytes(void);
    uint32_t freeBytes(void);

    void addBfr(uint8_t* bfr, uint32_t size);
    void getBfr(uint8_t* bfr, uint32_t size);

private:
    uint8_t buf[CYCLIC_BUF_SIZE];
    int  count;
    int  addPos;
    int  getPos;
};

#endif

