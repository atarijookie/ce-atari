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
    virtual bool waitForATN(int whichSpiCs, BYTE *inBuf);
    virtual void txRx(int whichSpiCs, int count, BYTE *sendBuffer, BYTE *receiveBufer);
    virtual WORD getRemainingLength(void);
    virtual bool sendData_start         (DWORD totalDataCount, BYTE scsiStatus, bool withStatus);
    virtual bool sendData_transferBlock (BYTE *pData, DWORD dataCount);
    virtual bool recvData_start         (DWORD totalDataCount);
    virtual bool recvData_transferBlock (BYTE *pData, DWORD dataCount);
    virtual void sendStatusToHans       (BYTE statusByte);
};

#endif // MEMDATATRANS_H
