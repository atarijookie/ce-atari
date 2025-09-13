#include <stdio.h>
#include <cstring>
#include "pico/stdlib.h"
#include "hardware/sync.h"

#include "defs.h"
#include "utils.h"
#include "fifo.h"

Fifo::Fifo(uint8_t* buf, size_t size)
{
    buffer = buf;
    capacity = size;
    head = 0;
    tail = 0;
    length = 0;
    spinlock_id = spin_lock_claim_unused(true);
    lock = spin_lock_instance(spinlock_id);
}

// Add data into FIFO, clip data that cannot fit.
void Fifo::getPushChunks(uint32_t* chunk1Addr, size_t* chunk1Size, uint32_t* chunk2Addr, size_t* chunk2Size, size_t len)
{
    size_t written = 0;

    // First chunk
    *chunk1Size = MIN(len, capacity - head);
    *chunk1Addr = (uint32_t) &buffer[head];

    // 1st chunk is smaller than the whole data that needed to be stored? store rest in other chunk
    if (*chunk1Size < len) {
        *chunk2Size = len - *chunk1Size;
        *chunk2Addr = (uint32_t) &buffer[0];
    } else {    // 1st chunk did fit all the data, no need for 2nd chunk
        *chunk2Size = 0;
        *chunk2Addr = 0;
    }
}

void Fifo::updateIndicesAfterPush(size_t written)
{
    // Update indices under spinlock
    uint32_t save = spin_lock_blocking(lock);
    head = (head + written) % capacity;
    length += written;
    spin_unlock(lock, save);
}

// Add data into FIFO, clip data that cannot fit.
size_t Fifo::push(const uint8_t* data, size_t len)
{
    size_t written = 0;

    // Clip to available space
    size_t space = capacity - get_length();
    len = MIN(len, space);

    // get how big the 1st and 2nd chunk need to be
    uint32_t chunk1Addr, chunk2Addr;
    size_t chunk1Size, chunk2Size;
    getPushChunks(&chunk1Addr, &chunk1Size, &chunk2Addr, &chunk2Size, len);

    // First chunk
    memcpy((void*) chunk1Addr, data, chunk1Size);
    written += chunk1Size;

    // Wraparound chunk
    if (chunk2Size > 0) {
        memcpy((void*) chunk2Addr, data + written, chunk2Size);
        written = len;
    }

    updateIndicesAfterPush(written);
    return written;
}

void Fifo::updateIndicesAfterPop(size_t retrieved)
{
    uint32_t save = spin_lock_blocking(lock);
    tail = (tail + retrieved) % capacity;
    length -= retrieved;
    spin_unlock(lock, save);
}

// Fetch exactly len bytes, wait up to timeout_ms
// This method assumes that the whole requested size fits into the FIFO!
size_t Fifo::pop(uint8_t* out, size_t len, uint32_t timeout_ms, bool justPeek)
{
    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);

    while (true) {
        size_t available = get_length();
        if (available >= len) {
            // Copy data
            size_t first = MIN(len, capacity - tail);
            memcpy(out, &buffer[tail], first);

            if (first < len) {
                memcpy(out + first, &buffer[0], len - first);
            }

            // Update indices under spinlock
            if(!justPeek) {
                updateIndicesAfterPop(len);
            }

            return len;
        }

        // Timeout?
        if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) {
            return 0; // nothing fetched
        }

        sleep_ms(1); // small wait
    }
}

uint8_t Fifo::popByte(void)
{
    if(get_length() <= 0) {
        return 0;
    }

    uint8_t val = buffer[tail];

    // Update indices under spinlock
    uint32_t save = spin_lock_blocking(lock);
    tail = (tail + 1) % capacity;
    length--;
    spin_unlock(lock, save);

    return val;
}

size_t Fifo::get_length() const
{
    // Safe read of length with minimal lock
    uint32_t save = spin_lock_blocking(lock);
    size_t l = length;
    spin_unlock(lock, save);
    return l;
}

size_t Fifo::get_free() const
{
    return capacity - get_length();
}
