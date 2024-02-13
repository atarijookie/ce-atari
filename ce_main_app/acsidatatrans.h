#ifndef ACSIDATATRANS_H
#define ACSIDATATRANS_H

#include "chipinterface.h"

// data direction after command processing
#define DATA_DIRECTION_UNKNOWN      0
#define DATA_DIRECTION_READ         1
#define DATA_DIRECTION_WRITE        2

#define ACSI_CMD_SIZE               14

#define ACSI_BUFFER_SIZE            (1024*1024)

#define ACSI_MAX_TRANSFER_SIZE_BYTES    (254 * 512)

#include "retrymodule.h"

class AcsiDataTrans
{
public:
    AcsiDataTrans();
    virtual ~AcsiDataTrans();

    virtual void setCommunicationObject(ChipInterface *comIn);
    virtual void setRetryObject(RetryModule *retryModule);

    //----------------
    // following functions are for convenient gathering of data in handling functions, but work only up to ACSI_BUFFER_SIZE which is currently 1 MB

    virtual void clear(bool clearAlsoDataDirection=true);

    virtual void setStatus(uint8_t stat);

    virtual void addDataByte(uint8_t val);
    virtual void addDataWord(uint16_t val);
    virtual void addDataDword(uint32_t val);
    
    virtual void addZerosUntilSize(uint32_t finalBufferCnt);

    virtual void addDataBfr(const void *data, uint32_t cnt, bool padToMul16);

    virtual void addDataCString(const char *data, bool padToMul16); // including null terminator
    virtual void padDataToMul16(void);

    virtual void dumpDataOnce(void);

    virtual uint32_t getCount(void);

    //----------------
    // for sending data from config stream
    virtual void sendDataToFd(int fd);

    //----------------
    // READ/WRITE functions for smaller data
    virtual bool recvData(uint8_t *data, uint32_t cnt);
    virtual void sendDataAndStatus(bool fromRetryModule = false);       // by default it's not a retry

    //----------------
    // following functions are used for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above
    
    virtual bool sendData_start         (uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus);
    virtual bool sendData_transferBlock (uint8_t *pData, uint32_t dataCount);

    virtual bool recvData_start         (uint32_t totalDataCount);
    virtual bool recvData_transferBlock (uint8_t *pData, uint32_t dataCount);

    virtual void sendStatusToHans       (uint8_t statusByte);

protected:
    uint8_t    *buffer;
    uint32_t   count;
    uint8_t    status;

    bool    statusWasSet;
    int     dataDirection;

    ChipInterface *com;
    RetryModule *retryMod;

    uint8_t    *recvBuffer;

    bool    dumpNextData;
};

#endif // ACSIDATATRANS_H
