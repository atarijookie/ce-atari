#include "WiFi.h"
#include <Preferences.h>

#include "defs.h"
#include "connection.h"
#include "utils.h"
#include "rw_tasks.h"

extern NetworkClient clientHdd;
extern NetworkClient clientFdd;
extern NetworkClient clientIkbd;

TaskHandle_t xHandleWriter;
QueueHandle_t writeQ;

RWBuffer writeBuffers[RW_BUFFERS_COUNT];        // this will hold the RW data

// clear buffer identified by index
void clearBuffer(int index)
{
    if(index >= RW_BUFFERS_COUNT) {
        return;
    }

    writeBuffers[index].used = false;
    writeBuffers[index].len = 0;
}

// try to get empty buffer or fail on timeout
int getEmptyBuffer(void)
{
    while(!hasTimedOut)     // try while not timeout
    {
        for(int i=0; i<RW_BUFFERS_COUNT; i++) {     // go through buffers, if found one that's not used, return index
            if(!writeBuffers[i].used) {
                return i;
            }
        }
    }

    // return -1 when no empty buffer could be found
    return -1;
}

void submitBufferForWrite(uint8_t index, uint16_t dataLen)
{
    if(index >= RW_BUFFERS_COUNT) {
        return;
    }

    uint32_t now = millis();
    uint32_t timeToTimeout = (now < timerEndMillis) ? (timerEndMillis - now) : 0;       // get how much time we have to the timeout

    TickType_t xDelay = timeToTimeout / portTICK_PERIOD_MS;     // convert ms to freeRTOS ticks

    writeBuffers[index].used = true;                    // buffer is now in use
    writeBuffers[index].len = dataLen;                  // this much of data is in the buffer
    xQueueSendToBack(writeQ, (void *) &index, xDelay);  // send buffer index to queue
}

void taskWriter(void * pvParameters)
{
    while(true)
    {
        uint8_t bfrIndex;
        xQueueReceive(writeQ, (void *) &bfrIndex, portMAX_DELAY);   // wait indefinitelly for writeQ to have item for this task

        if(bfrIndex >= RW_BUFFERS_COUNT) {      // index out of bounds? ignore it
            continue;
        }

        if(!clientHdd.connected()) {            // not connected? clear buffer and don't send it
            clearBuffer(bfrIndex);
            continue;
        }

        // send this buffer and clear it
        clientHdd.write(writeBuffers[bfrIndex].data, writeBuffers[bfrIndex].len);
        clearBuffer(bfrIndex);
    }
}

void createTasks(void)
{
    // clear buffers before using
    for(int i=0; i<RW_BUFFERS_COUNT; i++) {
        clearBuffer(i);
    }

    // create queues
    writeQ = xQueueCreate(RW_BUFFERS_COUNT, sizeof(uint8_t));

    // create tasks
    BaseType_t xReturned;
    xReturned = xTaskCreate(taskWriter, "taskWriter", 8192, (void *) NULL, /*priority*/ 2, &xHandleWriter);
}
