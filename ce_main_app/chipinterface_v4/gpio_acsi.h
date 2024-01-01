#ifndef __GPIO_ACSI_H__
#define __GPIO_ACSI_H__

#include <stdint.h>
#include "gpio4.h"

class GpioAcsi
{
public: 
    GpioAcsi();
    ~GpioAcsi();

    void init(void);                // init all pins
    void reset(void);               // reset pins that might get stuck after failed transfer
    void setConfig(uint8_t hddEnabledIDs, uint8_t sdCardId);    // store config needed for this to work
    uint8_t getXilinxByte(void);

    bool getCmd(uint8_t* cmd);      // try to get command, if one is waiting
    void startTransfer(uint8_t sendNotRecv, uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus);
    bool sendBlock(uint8_t *pData, uint32_t dataCount);
    bool recvBlock(uint8_t *pData, uint32_t dataCount);
    bool sendStatus(uint8_t scsiStatus);

private:
    uint8_t hddEnabledIDs;
    uint8_t sdCardId;

    uint8_t sendNotRecv;
    uint32_t totalDataCount;
    uint8_t scsiStatus;
    bool withStatus;

    uint8_t sendNotRecvNow;
    uint32_t timeoutTime;

    void setDataDirection(uint8_t sendNotRecv);

    uint8_t getCmdByte(void);
    uint8_t dataIn(void);
    void dataOut(uint8_t data);
    uint8_t getCmdLengthFromCmdBytesAcsi(uint8_t* cmd);

    void timeoutStart(uint32_t durationMs);
    bool isTimeout(void);
    bool waitForEOT(void);
    void waitForEOTlevel(int level);
};

#endif
