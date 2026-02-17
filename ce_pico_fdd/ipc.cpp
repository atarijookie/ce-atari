#include "pico/util/queue.h"

#include "ipc.h"
#include "defs.h"
#include "utils.h"

IPCbuffer buffersForCore0[IPC_BUFFER_SMALL_COUNT];
IPCbuffer buffersForCore1[IPC_BUFFER_LARGE_COUNT];

uint8_t dataToCore0[IPC_BUFFER_SMALL_COUNT][WRITEBUFFER_SIZE];
uint8_t dataToCore1[IPC_BUFFER_LARGE_COUNT][READTRACKDATA_SIZE_BYTES];

queue_t fifoToCore0;
queue_t fifoToCore1;

void ipcInit(void)
{
    for(int i=0; i<IPC_BUFFER_SMALL_COUNT; i++) {
        buffersForCore0[i].free = true;
        buffersForCore0[i].index = i;
        buffersForCore0[i].maxDataSize = WRITEBUFFER_SIZE;
        buffersForCore0[i].data = dataToCore0[i];
    }

    for(int i=0; i<IPC_BUFFER_LARGE_COUNT; i++) {
        buffersForCore1[i].free = true;
        buffersForCore1[i].index = i;
        buffersForCore1[i].maxDataSize = READTRACKDATA_SIZE_BYTES;
        buffersForCore1[i].data = dataToCore1[i];
    }

    queue_init(&fifoToCore0, 1, IPC_BUFFER_SMALL_COUNT);
    queue_init(&fifoToCore1, 1, IPC_BUFFER_LARGE_COUNT);
}

IPCbuffer* ipcGetFreeBuffer(int coreNo, uint32_t maxWaitMs)
{
    IPCbuffer* bfr = (coreNo == 0) ? buffersForCore0 : buffersForCore1;
    uint32_t start = millis();

    while(true)
    {
        int maxBuffers = (coreNo == 0) ? IPC_BUFFER_SMALL_COUNT : IPC_BUFFER_LARGE_COUNT;

        for(int i=0; i<maxBuffers; i++) {
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

    int maxBuffers = (coreNo == 0) ? IPC_BUFFER_SMALL_COUNT : IPC_BUFFER_LARGE_COUNT;

    if (queue_try_remove(fifo, &bfrIndex)) {    // succeeded getting value?
        if(bfrIndex >= maxBuffers) {      // index too big? fail
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

void ipcSetBufferAndPutToFifo(IPCbuffer* bfr, int coreNo, IpcCommand command, uint32_t lengthToStore)
{
    bfr->free = false;
    bfr->command = command;
    bfr->length = lengthToStore;

    ipcPutBufferToFifo(coreNo, bfr->index);
}
