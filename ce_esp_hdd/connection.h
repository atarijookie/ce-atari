#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include <arduino.h>

#define SOCK_HDD    0
#define SOCK_FDD    1

void connectToHost(void);
void handleIncommingData(void);
uint8_t sendBufferToHost(uint8_t whichSock, uint8_t *bfr, uint32_t txCount);

#define SYNC_TAG_HDD    0xc050d1c5

typedef struct {
    uint32_t syncTag;   //  0..3: 0xc050d1c5 [COSmODICS] (4 bytes)
    uint16_t cmdCode;   //  4..5: cmd code (2 bytes)
    uint32_t len;       //  6..9: data len (4 bytes)
} THeader;

#endif
