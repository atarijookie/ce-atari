#ifndef __IPC_H__
#define __IPC_H__

#include <cstdint>

#define IPC_BUFFER_SIZE     512

struct __attribute__((packed)) IPCbuffer
{
    uint8_t index;
    bool free;
    uint8_t command;
    uint32_t length;
    uint8_t statusByte;
    uint8_t data[IPC_BUFFER_SIZE];
};

void ipcInit(void);
IPCbuffer* ipcGetFreeBuffer(int coreNo, uint32_t maxWaitMs);
IPCbuffer* ipcGetBufferFromFifo(int coreNo);
void ipcPutBufferToFifo(int coreNo, uint8_t bfrIndex);
void ipcSetBufferAndPutToFifo(IPCbuffer* bfr, int coreNo, uint8_t command, uint32_t lengthToStore, uint8_t* data, uint32_t lengthOfData);

#define IPC_BUFFER_COUNT    16

#endif /* __IPC_H__ */
