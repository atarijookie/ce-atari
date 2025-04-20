#ifndef __FINDSTORAGE_H__
#define __FINDSTORAGE_H__

#include <stdint.h>

class TFindStorage {
public:
    TFindStorage();
    ~TFindStorage();

    int getSize(void);
    void clear(void);
    void copyDataFromOther(TFindStorage *other);

    uint32_t dta;

    uint8_t *buffer;
    uint16_t count;             // count of items found

    uint16_t maxCount;          // maximum count of items that this buffer can hold
};

#endif
