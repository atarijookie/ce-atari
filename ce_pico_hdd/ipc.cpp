#include <arduino.h>
#include "ipc.h"

IPCbuffer buffersForCore0[IPC_BUFFER_COUNT];
IPCbuffer buffersForCore1[IPC_BUFFER_COUNT];

queue_t fifoToCore0;
queue_t fifoToCore1;

void ipcInit(void)
{
    for(int i=0; i<IPC_BUFFER_COUNT; i++) {
        buffersForCore0[i].free = true;
        buffersForCore0[i].index = i;

        buffersForCore1[i].free = true;
        buffersForCore1[i].index = i;
    }

    queue_init(&fifoToCore0, 1, IPC_BUFFER_COUNT);
    queue_init(&fifoToCore1, 1, IPC_BUFFER_COUNT);
}

IPCbuffer* ipcGetFreeBuffer(int coreNo, uint32_t maxWaitMs)
{
    IPCbuffer* bfr = (coreNo == 0) ? buffersForCore0 : buffersForCore1;
    uint32_t start = millis();

    while(true)
    {
        for(int i=0; i<IPC_BUFFER_COUNT; i++) {
            if(bfr[i].free) {
                bfr[i].free = false;        // immediatelly not free
                return &bfr[i];
            }
        }

        uint32_t now = millis();
        if((now - start) > maxWaitMs) {
            return NULL;
        }
    }
}

IPCbuffer* ipcGetBufferFromFifo(int coreNo)
{
    IPCbuffer* bfrs = (coreNo == 0) ? buffersForCore0 : buffersForCore1;
    queue_t* fifo = (coreNo == 0) ? &fifoToCore0 : &fifoToCore1;

    if(queue_is_empty(fifo)) {  // nothing in FIFO, return NULL
        return NULL;
    }

    uint8_t bfrIndex;

    if (queue_try_remove(fifo, &bfrIndex)) {    // succeeded getting value?
        if(bfrIndex >= IPC_BUFFER_COUNT) {      // index too big? fail
            return NULL;
        }

        return &bfrs[bfrIndex];
    }

    return NULL;
}

void ipcPutBufferToFifo(int coreNo, uint8_t bfrIndex)
{
    queue_t* fifo = (coreNo == 0) ? &fifoToCore0 : &fifoToCore1;
    queue_try_add(fifo, (const void*) &bfrIndex);
}

void ipcSetBufferAndPutToFifo(IPCbuffer* bfr, int coreNo, uint8_t command, uint32_t lengthToStore, uint8_t* data, uint32_t lengthOfData)
{
    bfr->free = false;
    bfr->command = command;
    bfr->length = lengthToStore;

    if(data != NULL && lengthOfData > 0) {
        memcpy(bfr->data, data, MIN(lengthOfData, IPC_BUFFER_SIZE));
    }

    ipcPutBufferToFifo(coreNo, bfr->index);
}
