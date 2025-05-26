#ifndef __RW_TASKS_H__
#define __RW_TASKS_H__

#include <arduino.h>

#define RW_BUFFER_SIZE      4096
#define RW_BUFFERS_COUNT    3

typedef struct {
    bool used;
    uint16_t len;
    uint8_t data[RW_BUFFER_SIZE];
} RWBuffer;

extern RWBuffer writeBuffers[RW_BUFFERS_COUNT];        // this will hold the RW data

void createTasks(void);
void clearBuffer(int index);
int getEmptyBuffer(void);
void submitBufferForWrite(uint8_t index, uint16_t dataLen);

#endif
