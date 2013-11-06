#ifndef TRANSLATEDDISK_H
#define TRANSLATEDDISK_H

#include "../acsidatatrans.h"
#include "datatypes.h"

class TranslatedDisk
{
public:
    TranslatedDisk(void);
    ~TranslatedDisk();

    void setAcsiDataTrans(AcsiDataTrans *dt);

    void processCommand(BYTE *cmd);

private:
    AcsiDataTrans   *dataTrans;

    BYTE            *dataBuffer;
    BYTE            *dataBuffer2;
};

#endif
