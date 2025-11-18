#include <Ethernet.h>

#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "rw_tasks.h"

extern EthernetClient clientHdd;
extern EthernetClient clientFdd;
extern EthernetClient clientIkbd;

// TaskHandle_t xHandleWriter, xHandleReader;
// QueueHandle_t writeQfull, writeQempty;
// QueueHandle_t readQfull, readQempty;

RWBuffer writeBuffers[RW_BUFFERS_COUNT];        // this will hold the W data
RWBuffer readBuffers[RW_BUFFERS_COUNT];         // this will hold the R data

volatile uint8_t readerState = READER_STOP;     // start with reader not running
volatile uint32_t readDataLen;

// ===============================================================================================================================================
// HELPERS FOR READER AND WRITER

// clear buffer identified by index
void clearBuffer(bool readNotWrite, int index)
{
    if(index >= RW_BUFFERS_COUNT) {
        return;
    }

    RWBuffer* buf = readNotWrite ? &readBuffers[index] : &writeBuffers[index];

    buf->index = index;
    buf->len = 0;
}

// Get how much time we have from now to time the timeout will happen, and convert to ticks for freeRTOS functions
// TickType_t getXDelayToTimeout(void)
// {
//     uint32_t now = millis();
//     uint32_t timeToTimeout = (now < timerEndMillis) ? (timerEndMillis - now) : 0;       // get how much time we have to the timeout

//     TickType_t xDelay = timeToTimeout / portTICK_PERIOD_MS;     // convert ms to freeRTOS ticks
//     return xDelay;
// }

// ===============================================================================================================================================
// TASK WRITER PART

void writerStart(void)
{
    // empty the queues
    // xQueueReset(writeQfull);
    // xQueueReset(writeQempty);

    // // clear buffers before using, put them in the empty Q
    // for(uint8_t index=0; index<RW_BUFFERS_COUNT; index++)
    // {
    //     clearBuffer(bWRITE, index);
    //     xQueueSendToBack(writeQempty, (void *) &index, portMAX_DELAY);
    // }
}

void writerEnd(void)
{
    // xQueueReset(writeQfull);
    // xQueueReset(writeQempty);
}

// get next empty buffer for write, or NULL on timeout
RWBuffer* getNextEmptyWriteBuffer(void)
{
    // uint8_t idx;
    // TickType_t xDelay = getXDelayToTimeout();       // how much delay there is to timeout
    // BaseType_t res = xQueueReceive(writeQempty, (void *) &idx, xDelay);   // try to get index from queue

    // RWBuffer* buf = (res ? &writeBuffers[idx] : NULL);        // on succes return pointer, in fail return NULL
    // return buf;
    return NULL;
}

void submitBufferForWrite(RWBuffer* buf, uint16_t len)
{
    // buf->len = len;     // store length
    // TickType_t xDelay = getXDelayToTimeout();           // how much delay there is to timeout
    // xQueueSendToBack(writeQfull, (void *) &buf->index, xDelay); // send buffer index to queue
}

// taskWriter waits for buffer index in the writeQfull, then sends buffer with that index to TCP connection.
// To indicate that the buffer is free after sending, its index is placed in writeQempty.
void taskWriter(void* pvParameters)
{
    // while(true)
    // {
    //     uint8_t bfrIndex;
    //     xQueueReceive(writeQfull, (void *) &bfrIndex, portMAX_DELAY);   // wait indefinitelly for writeQ to have item for this task

    //     if(bfrIndex >= RW_BUFFERS_COUNT) {      // index out of bounds? ignore it
    //         continue;
    //     }

    //     if(clientHdd.connected()) {     // if connected, send the buffer
    //         clientHdd.write(writeBuffers[bfrIndex].data, writeBuffers[bfrIndex].len);
    //     }

    //     xQueueSendToBack(writeQempty, (void *) &bfrIndex, portMAX_DELAY);
    // }
}

// ===============================================================================================================================================
// TASK READER PART

// start data reader and make it read and send this much of data
void readerStart(uint32_t dataLen)
{
    // readDataLen = dataLen;          // set the desired read length
    // readerState = READER_RUN;       // put the reader in RUN state

    // // empty queues if anything was in them
    // xQueueReset(readQfull);
    // xQueueReset(readQempty);

    // // all buffers in the empty Q
    // for(uint8_t i=0; i<RW_BUFFERS_COUNT; i++)
    // {
    //     xQueueSendToBack(readQempty, (void *) &i, portMAX_DELAY);
    // }
}

// stop the data reader, clear all buffers, return to halted state
void readerStop(void)
{
    // // empty the queues
    // xQueueReset(readQfull);
    // xQueueReset(readQempty);

    // readerState = READER_STOP;  // put the reader in RUN state
    // readDataLen = 0;            // clear size to read
}

// check the queue for next read buffer available to be send to Atari
RWBuffer* getNextFullReadBuffer(void)
{
//     uint8_t idx = 0;
//     TickType_t xDelay = getXDelayToTimeout();       // how much delay there is to timeout
//     BaseType_t res = xQueueReceive(readQfull, (void *) &idx, xDelay);   // try to get index from queue

// #ifdef LOG_MORE
//     Serial.print("getNextFullReadBuffer ");
//     res ? Serial.println(idx) : Serial.println("NULL");
// #endif

//     RWBuffer* buf = (res ? &readBuffers[idx] : NULL);        // on succes return pointer, in fail return NULL
//     return buf;
    return NULL;
}

// once the buffer has been used for transfer, mark it as being free again
void markReadBufferAsEmpty(RWBuffer* buf)
{
    // TickType_t xDelay = getXDelayToTimeout();       // how much delay there is to timeout
    // xQueueSendToBack(readQempty, (void *) &buf->index, xDelay);     // put this index in the empty queue
}

// taskReader blocks until there is an empty buffer index in th readQempty, then if it's connected
// and should be sending data, it reads data into the buffer with that index, and on success
// it places this buffer index into readQfull. On failed read but still running state the index is
// placed back into readQempty.
void taskReader(void* pvParameters)
{
    // while(true)
    // {
    //     // if reader in STOP state, wait indefinitely; but if running then wait just to end of timeout
    //     uint8_t idx;
    //     TickType_t xDelay = (readerState == READER_STOP) ? portMAX_DELAY : getXDelayToTimeout();
    //     BaseType_t res = xQueueReceive(readQempty, (void *) &idx, xDelay);   // try to get index from queue

    //     if(!res)        // failed to get buffer index? wait again
    //     {
    //         continue;
    //     }

    //     // not connected or reader shouldn't run anymore? clear data, stop here
    //     if(!clientHdd.connected() || readerState != READER_RUN)
    //     {
    //         readerStop();
    //         continue;
    //     }

    //     uint32_t readSize = MIN(readDataLen, RW_BUFFER_SIZE);   // read size is rest or only buffer size
    //     int actualSize = clientHdd.read(readBuffers[idx].data, readSize);

    //     if(actualSize > 0 && readerState == READER_RUN)         // something was read and we're still in the run state? store read size, send buffer index to full Q
    //     {
    //         readDataLen -= actualSize;
    //         readBuffers[idx].len = actualSize;

    //         TickType_t xDelay = getXDelayToTimeout();           // how much delay there is to timeout
    //         xQueueSendToBack(readQfull, (void *) &idx, xDelay); // send buffer index to queue
    //     }
    //     else    // if nothing was read and buffer was not used, put it back into empty Q
    //     {
    //         if(readerState == READER_RUN)       // only when still running, put it back to empty Q
    //         {
    //             xQueueSendToBack(readQempty, (void *) &idx, portMAX_DELAY);
    //         }
    //     }
    // }
}

// ===============================================================================================================================================
// creating queues, clear buffers, create tasks

void createTasks(void)
{
    // // clear buffers before using
    // for(int i=0; i<RW_BUFFERS_COUNT; i++)
    // {
    //     clearBuffer(bREAD, i);
    //     clearBuffer(bWRITE, i);
    // }

    // // create queues
    // writeQfull = xQueueCreate(RW_BUFFERS_COUNT, sizeof(uint8_t));
    // writeQempty = xQueueCreate(RW_BUFFERS_COUNT, sizeof(uint8_t));

    // readQfull = xQueueCreate(RW_BUFFERS_COUNT, sizeof(uint8_t));
    // readQempty = xQueueCreate(RW_BUFFERS_COUNT, sizeof(uint8_t));

    // // create tasks
    // BaseType_t xReturned;
    // xReturned = xTaskCreatePinnedToCore(taskWriter, "taskWriter", 8192, (void *) NULL, /*priority*/ 2, &xHandleWriter, 0);
    // xReturned = xTaskCreatePinnedToCore(taskReader, "taskReader", 8192, (void *) NULL, /*priority*/ 2, &xHandleReader, 0);
}
