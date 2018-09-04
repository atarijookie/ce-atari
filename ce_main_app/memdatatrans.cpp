#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "debug.h"
#include "utils.h"
#include "gpio.h"
#include "memdatatrans.h"
#include "native/scsi_defs.h"

MemDataTrans::MemDataTrans()
{
}

MemDataTrans::~MemDataTrans()
{
}

void MemDataTrans::configureHw(void)
{
}

WORD MemDataTrans::getRemainingLength(void)
{
    return 0;       // shouldn't be used
}

bool MemDataTrans::waitForATN(int whichSpiCs, BYTE *inBuf)
{
    return false;   // ATN will never come from MemDataTrans
}

void MemDataTrans::txRx(int whichSpiCs, int count, BYTE *sendBuffer, BYTE *receiveBufer)
{
    return;         // no communication, just quit
}

bool MemDataTrans::sendData_start(DWORD totalDataCount, BYTE scsiStatus, bool withStatus)
{
    return true;
}

bool MemDataTrans::sendData_transferBlock(BYTE *pData, DWORD dataCount)
{
    return true;
}

bool MemDataTrans::recvData_start(DWORD totalDataCount)
{
    return true;
}

bool MemDataTrans::recvData_transferBlock(BYTE *pData, DWORD dataCount)
{
    return true;
}

void MemDataTrans::sendStatusToHans(BYTE statusByte)
{
}
