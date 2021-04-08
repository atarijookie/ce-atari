#ifndef _CYCLICBUFF_H_
#define _CYCLICBUFF_H_

#define CYCLIC_BUF_SIZE     2048
#define CYCLIC_BUF_MASK     0x7FF

class CyclicBuff {
public:
    BYTE buf[CYCLIC_BUF_SIZE];
    int  count;
    int  addPos;
    int  getPos;

    void init           (void);
    void add            (BYTE val);
    BYTE get            (void);
    BYTE peek           (void);
    BYTE peekWithOffset (int offset);
};

#endif

