#ifndef ACSIDATATRANS_H
#define ACSIDATATRANS_H

#include "datatypes.h"

class AcsiDataTrans
{
public:
    AcsiDataTrans();
    ~AcsiDataTrans();

    void clear(void);
    void sendAll(void);

    void setStatus(BYTE stat);
    void addData(BYTE val);

private:
    BYTE    *buffer;
    DWORD   count;
    BYTE    status;

};

#endif // ACSIDATATRANS_H
