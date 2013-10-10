#include <stdio.h>
#include <string.h>

#include "acsidatatrans.h"
#include "native/scsi_defs.h"

AcsiDataTrans::AcsiDataTrans()
{
    buffer          = new BYTE[1024*1024];       // 1 MB buffer
    count           = 0;
    status          = SCSI_ST_OK;
    statusWasSet    = false;
}

AcsiDataTrans::~AcsiDataTrans()
{
    delete []buffer;
}

void AcsiDataTrans::clear(void)
{
    count           = 0;
    status          = SCSI_ST_OK;
    statusWasSet    = false;
}

void AcsiDataTrans::setStatus(BYTE stat)
{
    status          = stat;
    statusWasSet    = true;
}

void AcsiDataTrans::addData(BYTE val)
{
    buffer[count] = val;
    count++;
}

void AcsiDataTrans::addData(BYTE *data, DWORD cnt)
{
    memcpy(&buffer[count], data, cnt);
    count += cnt;
}

// get data from Hans
bool AcsiDataTrans::recvData(BYTE *data, DWORD cnt)
{


    return true;
}

// send all data to Hans, including status
void AcsiDataTrans::sendDataAndStatus(void)
{
    if(count == 0 && !statusWasSet) {       // if no data was added and no status was set, nothing to send then
        return;
    }



}
