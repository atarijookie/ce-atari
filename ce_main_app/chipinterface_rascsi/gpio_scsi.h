#ifndef __GPIO_SCSI_H__
#define __GPIO_SCSI_H__

#include <stdint.h>
#include "gpio_rascsi.h"

#define DIR_SEND    1
#define DIR_RECV    0

#define IS_TIMEOUT_CACHED_COUNT     1000

#define SCSI_PHASE_BUSFREE      0
#define SCSI_PHASE_SELECTION    1
#define SCSI_PHASE_COMMAND      2
#define SCSI_PHASE_DATAIN       3
#define SCSI_PHASE_DATAOUT      4
#define SCSI_PHASE_STATUS       5
#define SCSI_PHASE_MSGIN        6
#define SCSI_PHASE_MSGOUT       7

class GpioScsi
{
public: 
    GpioScsi();
    ~GpioScsi();

    void init(uint8_t hddEnabledIDs, uint8_t sdCardId);     // init all pins
    void initPins(void);
    void setConfig(uint8_t hddEnabledIDs, uint8_t sdCardId);    // store config needed for this to work
    uint8_t getXilinxByte(void);

    bool getCmd(uint8_t* cmd);      // try to get command, if one is waiting
    void startTransfer(uint8_t sendNotRecv, uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus);
    bool sendBlock(uint8_t *pData, uint32_t dataCount);
    bool recvBlock(uint8_t *pData, uint32_t dataCount);
    bool sendStatus(uint8_t scsiStatus);
    void reset(void);

private:
    uint8_t hddEnabledIDs;
    uint8_t sdCardId;

    uint8_t sendNotRecv;
    uint32_t totalDataCount;
    uint8_t scsiStatus;
    bool withStatus;

    uint32_t timeoutTime;       // timestamp after which the current operation will be considered as timeout
    int timeoutCount;           // count of calls to isTimeout() which were just using the cached value of isTimeout

    void setBsy(bool bsy);
    void setPhaseBits(uint8_t sendNotRecv, bool cmdNotData, bool MSG);
    void setDataDirection(uint8_t sendNotRecv);

    bool waitForTwoPinLevels(int pin1, int level1, int pin2, int level2);
    bool waitForPinLevel(int pin, int level);
    bool waitForAckLevel(int level);
    uint8_t recvByte(void);
    bool sendByte(uint8_t data);
    void setPhase(int phase);
    const char* getPhaseStr(int phase);

    uint8_t dataIn(void);
    void dataOut(uint8_t data);

    void timeoutStart(uint32_t durationMs);
    bool isTimeout(void);

    uint8_t parityTable[256];
    void generateParityTable(void);
    uint8_t parityOfByte(uint8_t val);
};

#endif
