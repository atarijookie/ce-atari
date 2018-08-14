#ifndef DATATRANS_H
#define DATATRANS_H

#include "conspi.h"
#include "datatypes.h"

#include "retrymodule.h"

class DataTrans
{
public:
    DataTrans();
    virtual ~DataTrans();

    virtual void configureHw(void) = 0;

    //----------------
    // function for checking if the specified ATN is raised and if so, then get command bytes
    virtual bool waitForATN(int whichSpiCs, BYTE *inBuf) = 0;

    // function for sending / receiving data from/to lower levels of communication (e.g. to SPI)
    virtual void txRx(int whichSpiCs, int count, BYTE *sendBuffer, BYTE *receiveBufer) = 0;

    // returns how many data there is still to be transfered
    virtual WORD getRemainingLength(void) = 0;
    //----------------
    // following functions are for convenient gathering of data in handling functions, but work only up to ACSI_BUFFER_SIZE which is currently 1 MB

    void clear(bool clearAlsoDataDirection=true);
    void setStatus(BYTE stat);
    void addDataByte(BYTE val);
    void addDataWord(WORD val);
    void addDataDword(DWORD val);
    void addDataBfr(const void *data, DWORD cnt, bool padToMul16);
    void addDataCString(const char *data, bool padToMul16);  // including null terminator
    void padDataToMul16(void);
    DWORD getCount(void);
    void addZerosUntilSize(DWORD finalBufferCnt);
    void sendDataToFd(int fd);

    void dumpDataOnce(void);    // sets the flag
    void dumpData(void);        // does that dump
    //----------------
    virtual bool recvData(BYTE *data, DWORD cnt) = 0;
    virtual void sendDataAndStatus(bool fromRetryModule = false) = 0;       // by default it's not a retry

    //----------------
    // following functions are used for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above
    
    virtual bool sendData_start         (DWORD totalDataCount, BYTE scsiStatus, bool withStatus) = 0;
    virtual bool sendData_transferBlock (BYTE *pData, DWORD dataCount) = 0;

    virtual bool recvData_start         (DWORD totalDataCount) = 0;
    virtual bool recvData_transferBlock (BYTE *pData, DWORD dataCount) = 0;

    virtual void sendStatusToHans       (BYTE statusByte) = 0;

    RetryModule *retryMod;

private:
    BYTE    *buffer;
    BYTE    *recvBuffer;

    DWORD   count;
    BYTE    status;
    bool    statusWasSet;
    int     dataDirection;

    bool    dumpNextData;
};

#endif // DATATRANS_H
