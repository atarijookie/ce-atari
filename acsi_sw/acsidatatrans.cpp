#include <stdio.h>
#include <string.h>

#include "acsidatatrans.h"
#include "native/scsi_defs.h"

extern "C" void outDebugString(const char *format, ...);

#define BUFFER_SIZE         (1024*1024)
#define COMMAND_SIZE        8

AcsiDataTrans::AcsiDataTrans()
{
    buffer          = new BYTE[BUFFER_SIZE];       // 1 MB buffer
    recvBuffer      = new BYTE[BUFFER_SIZE];
    count           = 0;
    status          = SCSI_ST_OK;
    statusWasSet    = false;
    com             = NULL;
    prevAtnWord.got = false;
}

AcsiDataTrans::~AcsiDataTrans()
{
    delete []buffer;
    delete []recvBuffer;
}

void AcsiDataTrans::setCommunicationObject(CConUsb *comIn)
{
    com = comIn;
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
    if(!com) {
        outDebugString("AcsiDataTrans::recvData -- no communication object, fail!");
        return false;
    }




    return true;
}

// send all data to Hans, including status
void AcsiDataTrans::sendDataAndStatus(void)
{
    if(!com) {
        outDebugString("AcsiDataTrans::sendDataAndStatus -- no communication object, fail!");
        return;
    }

    if(count == 0 && !statusWasSet) {       // if no data was added and no status was set, nothing to send then
        return;
    }

    // first send the command
    BYTE devCommand[COMMAND_SIZE];
    memset(devCommand, 0, COMMAND_SIZE);

    devCommand[3] = CMD_DATA_READ;                          // store command
    devCommand[4] = count >> 8;                             // store data size
    devCommand[5] = count  & 0xff;
    devCommand[6] = status;                                 // store status

    com->txRx(COMMAND_SIZE, devCommand, recvBuffer);        // transmit this command

    // then send the data
    BYTE *dataNow = buffer;

    BYTE atnBuffer[8];
    int loops = 100;

    while(count > 0) {                                      // while there's something to send
        while(1) {                                          // wait for ATN_READ_MORE_DATA
            getAtnWord(atnBuffer);

            if(atnBuffer[1] == ATN_READ_MORE_DATA) {        // if got the requested ATN, break
                break;
            }

            if(loops == 0) {
                outDebugString("AcsiDataTrans::sendDataAndStatus -- this shouldn't happen!");
                return;
            }
            loops--;
        }

        DWORD cntNow = (count > 512) ? 512 : count;         // max 512 bytes per transfer
        com->txRx(cntNow, dataNow, recvBuffer);             // transmit this buffer

        dataNow += cntNow;                                  // move the data pointer further
    }
}

void AcsiDataTrans::getAtnWord(BYTE *bfr)
{
    if(prevAtnWord.got) {                   // got some previous ATN word? use it
        bfr[0] = prevAtnWord.bytes[0];
        bfr[1] = prevAtnWord.bytes[1];
        prevAtnWord.got = false;

        return;
    }

    // no previous ATN word? read it!
    BYTE outBuff[2];
    memset(outBuff, 0, 2);
    com->txRx(2, outBuff, bfr);
}

void AcsiDataTrans::setAtnWord(BYTE *bfr)
{
    prevAtnWord.bytes[0] = bfr[0];
    prevAtnWord.bytes[1] = bfr[1];
    prevAtnWord.got = true;
}
