#ifndef __GPIO_ACSI_H__
#define __GPIO_ACSI_H__

#include <stdint.h>
#include "gpio4.h"

#define DIR_SEND    1
#define DIR_RECV    0

#define IS_TIMEOUT_CACHED_COUNT     1000

class GpioAcsi
{
public: 
    GpioAcsi();
    ~GpioAcsi();

    void init(uint8_t hddEnabledIDs, uint8_t sdCardId);     // init all pins
    void reset(void);                                       // reset pins that might get stuck after failed transfer
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

    uint32_t timeoutTime;       // timestamp after which the current operation will be considered as timeout
    int timeoutCount;           // count of calls to isTimeout() which were just using the cached value of isTimeout

    void setDataDirection(uint8_t sendNotRecv);

    uint8_t getCmdByte(void);
    uint8_t dataIn(void);
    void dataOut(uint8_t data);
    uint8_t getCmdLengthFromCmdBytesAcsi(uint8_t* cmd);

    void timeoutStart(uint32_t durationMs);
    bool isTimeout(void);
    bool waitForEOT(void);
    void waitForEOTlevel(int level);
    void resetCmd1st(void);
};

#endif
