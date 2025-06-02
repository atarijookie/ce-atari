#ifndef _CYCLICBUFF_H_
#define _CYCLICBUFF_H_

#define CYCLIC_BUF_SIZE     2048
#define CYCLIC_BUF_MASK     0x7FF

class CyclicBuff {
public:
    uint8_t buf[CYCLIC_BUF_SIZE];
    int  count;
    int  addPos;
    int  getPos;
    int  size;

    void init           (void);
    void add            (uint8_t val);
    uint8_t get            (void);
    uint8_t peek           (void);
    uint8_t peekWithOffset (int offset);
};

#endif

