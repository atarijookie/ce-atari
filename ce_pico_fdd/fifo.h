#ifndef __FIFO_H__
#define __FIFO_H__

#include "pico/stdlib.h"
#include "hardware/sync.h"
#include <cstdint>
#include <cstring>

class Fifo {

public:
    Fifo(uint8_t* buf, size_t size);

    void getPushChunks(uint32_t* chunk1Addr, size_t* chunk1Size, uint32_t* chunk2Addr, size_t* chunk2Size, size_t len);
    void updateIndicesAfterPush(size_t written);
    void updateIndicesAfterPop(size_t retrieved);

    size_t push(const uint8_t* data, size_t len);               // Add data into FIFO
    size_t pop(uint8_t* out, size_t len, uint32_t timeout_ms, bool justPeek);  // Fetch exactly len bytes, wait up to timeout_ms
    uint8_t popByte(void);

    size_t get_length() const;
    size_t get_free() const;

private:
    uint8_t* buffer;
    size_t capacity;

    size_t head;   // write index
    size_t tail;   // read index
    size_t length; // number of bytes stored

    uint spinlock_id;
    spin_lock_t* lock;
};

#endif
