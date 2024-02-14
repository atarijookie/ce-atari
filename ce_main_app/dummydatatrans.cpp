#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "debug.h"
#include "utils.h"
#include "dummydatatrans.h"
#include "native/scsi_defs.h"

DummyDataTrans::DummyDataTrans()
{
}

DummyDataTrans::~DummyDataTrans()
{
}

// get data from Hans
bool DummyDataTrans::recvData(uint8_t *data, uint32_t cnt)
{
    memcpy(data, recvBuffer, cnt);
    return true;
}

void DummyDataTrans::copyInDummyData(uint8_t *data, uint32_t cnt)
{
    memcpy(recvBuffer, data, cnt);
}

void DummyDataTrans::sendDataAndStatus(bool fromRetryModule)
{
    Debug::outBfr(buffer, count);
}

uint8_t DummyDataTrans::getStatus(void)
{
    return status;
}

uint8_t* DummyDataTrans::getBuffer(void)
{
    return buffer;
}
