#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "debug.h"
#include "utils.h"
#include "acsidatatrans.h"
#include "native/scsi_defs.h"

#if defined(ONPC_HIGHLEVEL)
    #include "socks.h"
#endif

AcsiDataTrans::AcsiDataTrans()
{
    buffer          = new BYTE[ACSI_BUFFER_SIZE];        // 1 MB buffer
    recvBuffer      = new BYTE[ACSI_BUFFER_SIZE];
    
#if defined(ONPC_HIGHLEVEL)
    bufferRead      = buffer;
    bufferWrite     = recvBuffer;
#endif

    memset(buffer,      0, ACSI_BUFFER_SIZE);            // init buffers to zero
    memset(recvBuffer,  0, ACSI_BUFFER_SIZE);
    
    count           = 0;
    status          = SCSI_ST_OK;
    statusWasSet    = false;
    com             = NULL;
    dataDirection   = DATA_DIRECTION_READ;
    
    dumpNextData    = false;
    
    retryMod        = NULL;
}

AcsiDataTrans::~AcsiDataTrans()
{
    delete []buffer;
    delete []recvBuffer;
}

void AcsiDataTrans::setCommunicationObject(ChipInterface *comIn)
{
    com = comIn;
}

void AcsiDataTrans::setRetryObject(RetryModule *retryModule)
{
    retryMod = retryModule;
}

void AcsiDataTrans::clear(bool clearAlsoDataDirection)
{
    count           = 0;
    status          = SCSI_ST_OK;
    statusWasSet    = false;
    
    if(clearAlsoDataDirection) {
        dataDirection   = DATA_DIRECTION_READ;
    }
    
    dumpNextData    = false;
}

void AcsiDataTrans::setStatus(BYTE stat)
{
    status          = stat;
    statusWasSet    = true;
}

void AcsiDataTrans::addDataByte(BYTE val)
{
    buffer[count] = val;
    count++;
}

void AcsiDataTrans::addDataDword(DWORD val)
{
    buffer[count    ] = (val >> 24) & 0xff;
    buffer[count + 1] = (val >> 16) & 0xff;
    buffer[count + 2] = (val >>  8) & 0xff;
    buffer[count + 3] = (val      ) & 0xff;

    count += 4;
}

void AcsiDataTrans::addDataWord(WORD val)
{
    buffer[count    ] = (val >> 8) & 0xff;
    buffer[count + 1] = (val     ) & 0xff;

    count += 2;
}

void AcsiDataTrans::addDataBfr(const void *data, DWORD cnt, bool padToMul16)
{
    memcpy(&buffer[count], data, cnt);
    count += cnt;

    if(padToMul16) {                    // if should pad to multiple of 16
        padDataToMul16();
    }
}

void AcsiDataTrans::addDataCString(const char *data, bool padToMul16)
{
    // include null terminator in byte count
    addDataBfr(data, strlen(data) + 1, padToMul16);
}

void AcsiDataTrans::padDataToMul16(void)
{
    int mod = count % 16;           // how many we got in the last 1/16th part?
    int pad = 16 - mod;             // how many we need to add to make count % 16 equal to 0?

    if(mod != 0) {                  // if we should pad something
        memset(&buffer[count], 0, pad); // set the padded bytes to zero and add this count
        count += pad;

        // if((count % 512) != 0) {     // if it's not a full sector
        //     pad += 2;                // padding is greater than just to make mod16 == 0, to make the data go into ram
        // }
    }
}

// get data from Hans
bool AcsiDataTrans::recvData(BYTE *data, DWORD cnt)
{
    bool res;

    dataDirection = DATA_DIRECTION_WRITE;                   // let the higher function know that we've done data write -- 130 048 Bytes
    res = recvData_start(cnt);                              // first send the command and tell Hans that we need WRITE data

    if(!res) {                                              // failed to start? 
        return false;
    }

    res = recvData_transferBlock(data, cnt);                // get data from Hans
    return res;
}

void AcsiDataTrans::dumpDataOnce(void)
{
    dumpNextData = true;
}

void AcsiDataTrans::sendDataToFd(int fd)
{
    if(dataDirection == DATA_DIRECTION_WRITE) {
        count = 0;
        return;
    }
    
    if(count == 0) {    // if there's no data to send, send single zero byte
        buffer[0]   = 0;
        count       = 1;
    }
    
    WORD length = count;
    write(fd, &length, 2);      // first word - length of data to be received
    
    write(fd, buffer, count);   // then the data...
    count = 0;
}

// send all data to Hans, including status
void AcsiDataTrans::sendDataAndStatus(bool fromRetryModule)
{
    if(!com) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::sendDataAndStatus -- no communication object, fail!");
        return;
    }

    if(!retryMod) {         // no retry module?
        return;
    }

    if(fromRetryModule) {   // if it's a RETRY, get the copy of data and proceed like it would be from real module
        retryMod->restoreDataAndStatus  (dataDirection, count, buffer, statusWasSet, status);
    } else {                // if it's normal run (not a RETRY), let the retry module do a copy of data
        retryMod->copyDataAndStatus     (dataDirection, count, buffer, statusWasSet, status);
    }

#if defined(ONPC_HIGHLEVEL) 
    if((sockReadNotWrite == 0 && dataDirection != DATA_DIRECTION_WRITE) || (sockReadNotWrite != 0 && dataDirection == DATA_DIRECTION_WRITE)) {
        Debug::out(LOG_ERROR, "!!!!!!!!! AcsiDataTrans::sendDataAndStatus -- DATA DIRECTION DISCREPANCY !!!!! sockReadNotWrite: %d, dataDirection: %d", sockReadNotWrite, dataDirection);
    }
#endif

    // for DATA write transmit just the status in a different way (on separate ATN)
    if(dataDirection == DATA_DIRECTION_WRITE) {
        sendStatusToHans(status);
        return;
    }

    if(count == 0 && !statusWasSet) {       // if no data was added and no status was set, nothing to send then
        return;
    }
    //---------------------------------------
#if defined(ONPC_HIGHLEVEL)
    if(dataDirection == DATA_DIRECTION_READ) {
        // ACSI READ - send (write) data to other side, and also status
        count = sockByteCount;

        Debug::out(LOG_DEBUG, "sendDataAndStatus: %d bytes status: %02x (%d)", count, status, statusWasSet);

//        Debug::out(LOG_ERROR, "AcsiDataTrans::sendDataAndStatus -- sending %d bytes and status %02x", count, status);
//        Debug::out(LOG_DEBUG, "AcsiDataTrans::sendDataAndStatus -- %02x %02x %02x %02x %02x %02x %02x %02x ", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5], buffer[6], buffer[7]);
    
        BYTE padding = 0xff;
        serverSocket_write(&padding, 1);
        serverSocket_write(buffer, count);
        
        WORD sum = dataChecksum(buffer, count);     // calculate and send checksum
        serverSocket_write((BYTE *) &sum, 2);
        
        serverSocket_write(&status, 1);
        return;
    }
#endif
    //---------------------------------------
    if(dumpNextData) {
        Debug::out(LOG_DEBUG, "sendDataAndStatus: %d bytes", count);
        BYTE *src = buffer;

        WORD dumpCnt = 0;
        
        int lines = count / 32;
        if((count % 32) != 0) {
            lines++;
        }
    
        for(int i=0; i<lines; i++) {
            char bfr[1024];
            char *b = &bfr[0];

            for(int j=0; j<32; j++) {
                int val = (int) *src;
                src++;
                sprintf(b, "%02x ", val);
                b += 3;
                
                dumpCnt++;
                if(dumpCnt >= count) {
                    break;
                }
            }

            Debug::out(LOG_DEBUG, "%s", bfr);
        }
        
        dumpNextData = false;
    }
    //---------------------------------------
    // first send the command
    bool res;

    res = sendData_start(count, status, true);      // try to start the read data transfer, with status

    if(!res) {
        return;
    }

    sendData_transferBlock(buffer, count);    // transfer this block
}

bool AcsiDataTrans::sendData_start(DWORD totalDataCount, BYTE scsiStatus, bool withStatus)
{
    if(!com) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::sendData_start -- no communication object, fail");
        return false;
    }

    return com->hdd_sendData_start(totalDataCount, scsiStatus, withStatus);
}

bool AcsiDataTrans::sendData_transferBlock(BYTE *pData, DWORD dataCount)
{
    bool res = com->hdd_sendData_transferBlock(pData, dataCount);

    if(!res) {                                                  // failed? fail
        clear();                                                // clear all the variables
    }

    return res;
}

bool AcsiDataTrans::recvData_start(DWORD totalDataCount)
{
    if(!com) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::recvData_start() -- no communication object, fail!");
        return false;
    }

    dataDirection = DATA_DIRECTION_WRITE;                           // let the higher function know that we've done data write -- 130 048 Bytes

    return com->hdd_recvData_start(recvBuffer, totalDataCount);
}

bool AcsiDataTrans::recvData_transferBlock(BYTE *pData, DWORD dataCount)
{
    bool res = com->hdd_recvData_transferBlock(pData, dataCount);

    if(!res) {              // failed?
        clear(false);       // clear all the variables
    }

    return res;
}

void AcsiDataTrans::sendStatusToHans(BYTE statusByte)
{
    bool res = com->hdd_sendStatusToHans(statusByte);

    if(!res) {
        clear();            // clear all the variables
    }
}

DWORD AcsiDataTrans::getCount(void)
{
    return count;
}

void AcsiDataTrans::addZerosUntilSize(DWORD finalBufferCnt)
{
    while(count < finalBufferCnt) {         // add zeros until we don't have enough
        addDataByte(0);
    }
}
