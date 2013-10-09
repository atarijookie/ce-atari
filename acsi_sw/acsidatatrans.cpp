#include "acsidatatrans.h"
#include "native/scsi_defs.h"

AcsiDataTrans::AcsiDataTrans()
{
    buffer  = new BYTE[1024*1024];       // 1 MB buffer
    count   = 0;
    status  = SCSI_ST_OK;
}

AcsiDataTrans::~AcsiDataTrans()
{
    delete []buffer;
}

void AcsiDataTrans::clear(void)
{
    count   = 0;
    status  = SCSI_ST_OK;
}

void AcsiDataTrans::setStatus(BYTE stat)
{
    status = stat;
}

void AcsiDataTrans::addData(BYTE val)
{
    buffer[count] = val;
    count++;
}

void AcsiDataTrans::sendAll(void)
{

}
