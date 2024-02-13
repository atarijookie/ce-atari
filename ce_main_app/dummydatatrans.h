#ifndef DUMMYDATATRANS_H
#define DUMMYDATATRANS_H

#include "chipinterface.h"
#include "retrymodule.h"
#include "acsidatatrans.h"

class DummyDataTrans: public AcsiDataTrans
{
public:
    DummyDataTrans();
    virtual ~DummyDataTrans();

    void copyInDummyData(uint8_t *data, uint32_t cnt);
    uint8_t getStatus(void);

protected:
    virtual bool recvData(uint8_t *data, uint32_t cnt);
    virtual void sendDataAndStatus(bool fromRetryModule = false);
};

#endif // ACSIDATATRANS_H
