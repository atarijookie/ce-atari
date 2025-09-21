#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include "client_context.h"

void showRunningStateOnDisplay(void);

void connectToHost(void);
void handleIncommingData(void);

bool sendDataToHost(uint8_t *bfr, uint32_t dataSizeBytes);
bool sendHeaderAndDataToHost(uint8_t *bfr, uint32_t dataSizeBytes);

#define SYNC_TAG_FDD    0xc0500fdd

typedef struct {
    uint32_t syncTag;   //  0..3: 0xc0500fdd [COSmOFdd0] (4 bytes)
    uint16_t cmdCode;   //  4..5: cmd code (2 bytes)
    uint32_t len;       //  6..9: data len (4 bytes)
} THeader;

typedef struct {
    struct tcp_pcb *pcb;
    ClientContext *cc;
} TConnection;

size_t conWrite(TConnection* client, const uint8_t *buf, size_t size);
bool connectionAvailable(TConnection* client);
bool isConnected(TConnection* client);
size_t connectionCanReadBytes(TConnection* client);
void connect(TConnection* client, ip_addr_t* addr, uint16_t port);
void stop(TConnection* client);

#endif
