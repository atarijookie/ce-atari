#ifndef __RW_TASKS_H__
#define __RW_TASKS_H__

#include <arduino.h>

#define RW_BUFFER_SIZE      4096
#define RW_BUFFERS_COUNT    3

#define READER_STOP     0
#define READER_RUN      1

typedef struct {
    uint8_t index;
    uint16_t len;
    uint8_t data[RW_BUFFER_SIZE];
} RWBuffer;

#define bREAD   true
#define bWRITE  false

extern RWBuffer writeBuffers[RW_BUFFERS_COUNT];        // this will hold the RW data

void createTasks(void);
void clearBuffer(bool readNotWrite, int index);

int getEmptyBuffer(bool readNotWrite);

void writerStart(void);
void writerEnd(void);
RWBuffer* getNextEmptyWriteBuffer(void);
void submitBufferForWrite(RWBuffer* buf, uint16_t len);

void readerStart(uint32_t dataLen);
void readerStop(void);
RWBuffer* getNextFullReadBuffer(void);
void markReadBufferAsEmpty(RWBuffer* buf);

#endif
