#ifndef ACSIDATATRANS_H
#define ACSIDATATRANS_H

#include "chipinterface.h"
#include "datatypes.h"

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
    ~AcsiDataTrans();

    void setCommunicationObject(ChipInterface *comIn);
    void setRetryObject(RetryModule *retryModule);

    //----------------
    // following functions are for convenient gathering of data in handling functions, but work only up to ACSI_BUFFER_SIZE which is currently 1 MB

    void clear(bool clearAlsoDataDirection=true);

    void setStatus(BYTE stat);

    void addDataByte(BYTE val);
    void addDataWord(WORD val);
    void addDataDword(DWORD val);
    
    void addZerosUntilSize(DWORD finalBufferCnt);

    void addDataBfr(const void *data, DWORD cnt, bool padToMul16);

    void addDataCString(const char *data, bool padToMul16); // including null terminator
    void padDataToMul16(void);

    void dumpDataOnce(void);

    DWORD getCount(void);

    //----------------
    // for sending data from config stream
    void sendDataToFd(int fd);

    //----------------
    // READ/WRITE functions for smaller data
    bool recvData(BYTE *data, DWORD cnt);
    void sendDataAndStatus(bool fromRetryModule = false);       // by default it's not a retry

    //----------------
    // following functions are used for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above
    
    bool sendData_start         (DWORD totalDataCount, BYTE scsiStatus, bool withStatus);
    bool sendData_transferBlock (BYTE *pData, DWORD dataCount);

    bool recvData_start         (DWORD totalDataCount);
    bool recvData_transferBlock (BYTE *pData, DWORD dataCount);

    void sendStatusToHans       (BYTE statusByte);

private:
    BYTE    *buffer;
    DWORD   count;
    BYTE    status;

    bool    statusWasSet;
    int     dataDirection;

    ChipInterface *com;
    RetryModule *retryMod;

    BYTE    *recvBuffer;

    bool    dumpNextData;
};

#endif // ACSIDATATRANS_H
