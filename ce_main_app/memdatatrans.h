#ifndef MEMDATATRANS_H
#define MEMDATATRANS_H

#include "datatrans.h"
#include "datatypes.h"

class MemDataTrans: public DataTrans
{
public:
    MemDataTrans();
    ~MemDataTrans();

    virtual void configureHw(void);
    //----------------
    // function for checking if the specified ATN is raised and if so, then get command bytes
    virtual bool waitForATN(int whichSpiCs, BYTE *inBuf);

    // function for sending / receiving data from/to lower levels of communication (e.g. to SPI)
    virtual void txRx(int whichSpiCs, int count, BYTE *sendBuffer, BYTE *receiveBufer);

    // returns how many data there is still to be transfered
    virtual WORD getRemainingLength(void);
    //----------------
    // following functions are used for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above

    virtual bool sendData_start         (DWORD totalDataCount, BYTE scsiStatus, bool withStatus);
    virtual bool sendData_transferBlock (BYTE *pData, DWORD dataCount);

    virtual bool recvData_start         (DWORD totalDataCount);
    virtual bool recvData_transferBlock (BYTE *pData, DWORD dataCount);

    virtual void sendStatusToHans       (BYTE statusByte);
};

#endif // MEMDATATRANS_H
