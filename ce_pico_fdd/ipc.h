#ifndef __IPC_H__
#define __IPC_H__

#include <cstdint>

enum IpcCommand {
  CMD_STORE_TRACK,
  CMD_SEND_WRITTEN_SECTOR
};

struct __attribute__((packed)) IPCbuffer
{
    uint8_t index;
    bool free;
    IpcCommand command;
    uint32_t length;

    uint8_t trackNo;
    uint8_t sideNo;
    uint8_t sectorNo;

    uint32_t maxDataSize;
    uint8_t *data;
};

void ipcInit(void);
IPCbuffer* ipcGetFreeBuffer(int coreNo, uint32_t maxWaitMs);
IPCbuffer* ipcGetBufferFromFifo(int coreNo);
void ipcPutBufferToFifo(int coreNo, uint8_t bfrIndex);
void ipcSetBufferAndPutToFifo(IPCbuffer* bfr, int coreNo, IpcCommand command, uint32_t lengthToStore);

#define IPC_BUFFER_LARGE_COUNT    10
#define IPC_BUFFER_SMALL_COUNT    16

#endif /* __IPC_H__ */
