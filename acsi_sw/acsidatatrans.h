#ifndef ACSIDATATRANS_H
#define ACSIDATATRANS_H

#include "datatypes.h"

class AcsiDataTrans
{
public:
    AcsiDataTrans();
    ~AcsiDataTrans();

    void clear(void);

    void setStatus(BYTE stat);
    void addData(BYTE val);
    void addData(BYTE *data, DWORD cnt);

    bool recvData(BYTE *data, DWORD cnt);
    void sendDataAndStatus(void);

private:
    BYTE    *buffer;
    DWORD   count;
    BYTE    status;

    bool    statusWasSet;
};

#endif // ACSIDATATRANS_H
