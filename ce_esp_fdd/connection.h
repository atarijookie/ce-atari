#ifndef __CONNECTION_H__
#define __CONNECTION_H__

#include <arduino.h>

void showRunningStateOnDisplay(void);

void connectToHost(void);
void handleIncommingData(void);

bool sendDataToHost(uint8_t *bfr, uint32_t dataSizeBytes);
bool sendHeaderAndDataToHost(uint8_t *bfr, uint32_t dataSizeBytes);

#define SYNC_TAG_FDD    0xc050f108

typedef struct {
    uint32_t syncTag;   //  0..3: 0xc050f108 [COSmOFLOppy] (4 bytes)
    uint16_t cmdCode;   //  4..5: cmd code (2 bytes)
    uint32_t len;       //  6..9: data len (4 bytes)
} THeader;

#endif
