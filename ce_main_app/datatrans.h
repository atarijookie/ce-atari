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

    //----------------
    // function for checking if the specified ATN is raised and if so, then get command bytes
    virtual bool waitForATN(int whichSpiCs, BYTE *inBuf) = 0;

    // function for sending / receiving data from/to lower levels of communication (e.g. to SPI)
    virtual void txRx(int whichSpiCs, int count, BYTE *sendBuffer, BYTE *receiveBufer) = 0;

    // returns how many data there is still to be transfered
    virtual WORD getRemainingLength(void) = 0;
    //----------------
    // following functions are for convenient gathering of data in handling functions, but work only up to ACSI_BUFFER_SIZE which is currently 1 MB

    virtual void clear(bool clearAlsoDataDirection=true) = 0;

    virtual void setStatus(BYTE stat) = 0;

    virtual void addDataByte(BYTE val) = 0;
    virtual void addDataWord(WORD val) = 0;
    virtual void addDataDword(DWORD val) = 0;
    
    virtual void addZerosUntilSize(DWORD finalBufferCnt) = 0;

    virtual void addDataBfr(const void *data, DWORD cnt, bool padToMul16) = 0;

    virtual void addDataCString(const char *data, bool padToMul16) = 0;  // including null terminator

    virtual void padDataToMul16(void) = 0;

    virtual bool recvData(BYTE *data, DWORD cnt) = 0;
    virtual void sendDataAndStatus(bool fromRetryModule = false) = 0;       // by default it's not a retry
    virtual void sendDataToFd(int fd) = 0;

    virtual void dumpDataOnce(void) = 0;

    virtual DWORD getCount(void) = 0;

    //----------------
    // following functions are used for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above
    
    virtual bool sendData_start         (DWORD totalDataCount, BYTE scsiStatus, bool withStatus) = 0;
    virtual bool sendData_transferBlock (BYTE *pData, DWORD dataCount) = 0;

    virtual bool recvData_start         (DWORD totalDataCount) = 0;
    virtual bool recvData_transferBlock (BYTE *pData, DWORD dataCount) = 0;

    virtual void sendStatusToHans       (BYTE statusByte) = 0;

    RetryModule *retryMod;
};

#endif // DATATRANS_H
