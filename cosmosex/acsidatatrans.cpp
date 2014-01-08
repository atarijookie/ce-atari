#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "gpio.h"
#include "acsidatatrans.h"
#include "native/scsi_defs.h"

extern "C" void outDebugString(const char *format, ...);

#define BUFFER_SIZE         (1024*1024)
#define COMMAND_SIZE        10

AcsiDataTrans::AcsiDataTrans()
{
    buffer          = new BYTE[BUFFER_SIZE];       // 1 MB buffer
    recvBuffer      = new BYTE[BUFFER_SIZE];
    count           = 0;
    status          = SCSI_ST_OK;
    statusWasSet    = false;
    com             = NULL;
    dataDirection   = DATA_DIRECTION_READ;
}

AcsiDataTrans::~AcsiDataTrans()
{
    delete []buffer;
    delete []recvBuffer;
}

void AcsiDataTrans::setCommunicationObject(CConSpi *comIn)
{
    com = comIn;
}

void AcsiDataTrans::clear(void)
{
    count           = 0;
    status          = SCSI_ST_OK;
    statusWasSet    = false;
    dataDirection   = DATA_DIRECTION_READ;
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

void AcsiDataTrans::addDataBfr(BYTE *data, DWORD cnt, bool padToMul16)
{
    memcpy(&buffer[count], data, cnt);
    count += cnt;

    if(padToMul16) {                    // if should pad to multiple of 16
        padDataToMul16();
    }
}

void AcsiDataTrans::padDataToMul16(void)
{
    int mod = count % 16;           // how many we got in the last 1/16th part?
    int pad = 16 - mod;             // how many we need to add to make count % 16 equal to 0?

    if(mod != 0) {                  // if we should pad something
        memset(&buffer[count], 0, pad); // set the padded bytes to zero and add this count
        count += pad;

	    // if((count % 512) != 0) {		// if it's not a full sector
	    //     pad += 2;				// padding is greater than just to make mod16 == 0, to make the data go into ram
    	// }
    }
}

// get data from Hans
bool AcsiDataTrans::recvData(BYTE *data, DWORD cnt)
{
    if(!com) {
        outDebugString("AcsiDataTrans::recvData -- no communication object, fail!");
        return false;
    }

    dataDirection = DATA_DIRECTION_WRITE;                   // let the higher function know that we've done data write -- 130 048 Bytes

    // first send the command and tell Hans that we need WRITE data
    BYTE devCommand[COMMAND_SIZE];
    memset(devCommand, 0, COMMAND_SIZE);

    devCommand[3] = CMD_DATA_WRITE;                         // store command - WRITE
    devCommand[4] = cnt >> 16;                              // store data size
    devCommand[5] = cnt >>  8;
    devCommand[6] = cnt  & 0xff;
    devCommand[7] = 0xff;                                   // store INVALID status, because the real status will be sent on CMD_SEND_STATUS

	com->txRx(SPI_CS_HANS, COMMAND_SIZE, devCommand, recvBuffer);        // transmit this command

    memset(txBuffer, 0, 520);                               // nothing to transmit, really...

    while(cnt > 0) {
        // request maximum 512 bytes from host
        DWORD subCount = (cnt > 512) ? 512 : cnt;
        cnt -= subCount;

        bool res = waitForATN(ATN_WRITE_MORE_DATA, 1000);   // wait for ATN_WRITE_MORE_DATA

        if(!res) {                                          // this didn't come? fuck!
            return false;
        }

        com->txRx(SPI_CS_HANS, subCount + 8 - 4, txBuffer, rxBuffer);    // transmit data (size = subCount) + header and footer (size = 8) - already received 4 bytes
        memcpy(data, rxBuffer + 2, subCount);               // copy just the data, skip sequence number

        data += subCount;                                   // move in the buffer further

        //----------------------
        // just for dumping the data
        unsigned char *src = rxBuffer + 2;

        for(int i=0; i<16; i++) {
            char bfr[1024];
            char *b = &bfr[0];

            for(int j=0; j<32; j++) {
                int val = (int) *src;
                src++;
                sprintf(b, "%02x ", val);
                b += 3;
            }

            outDebugString("%s", bfr);
        }
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

    // for DATA write transmit just the status in a different way (on separate ATN)
    if(dataDirection == DATA_DIRECTION_WRITE) {
        sendStatusAfterWrite();
        return;
    }

    if(count == 0 && !statusWasSet) {       // if no data was added and no status was set, nothing to send then
        return;
    }

    // first send the command
    BYTE devCommand[COMMAND_SIZE];
    memset(devCommand, 0, COMMAND_SIZE);

    devCommand[3] = CMD_DATA_READ;                          // store command
    devCommand[4] = count >> 16;                            // store data size
    devCommand[5] = count >>  8;
    devCommand[6] = count  & 0xff;
    devCommand[7] = status;                                 // store status

    com->txRx(SPI_CS_HANS, COMMAND_SIZE, devCommand, recvBuffer);        // transmit this command

    // then send the data
    BYTE *dataNow = buffer;

    txBuffer[0] = 0;
    txBuffer[1] = CMD_DATA_MARKER;                          // mark the start of data

    while(count > 0) {                                      // while there's something to send
        bool res = waitForATN(ATN_READ_MORE_DATA, 1000);    // wait for ATN_READ_MORE_DATA

        if(!res) {                                          // this didn't come? fuck!
            return;
        }

        DWORD cntNow = (count > 512) ? 512 : count;         // max 512 bytes per transfer
        count -= cntNow;

        memcpy(txBuffer + 2, dataNow, cntNow);              // copy the data after the header (2 bytes)
        com->txRx(SPI_CS_HANS, cntNow + 2, txBuffer, rxBuffer);          // transmit this buffer with header

        dataNow += cntNow;                                  // move the data pointer further
    }
}

void AcsiDataTrans::sendStatusAfterWrite(void)
{
    bool res = waitForATN(ATN_GET_STATUS, 1000);

    if(!res) {
        return;
    }

    memset(txBuffer, 0, 16);                                // clear the tx buffer
    txBuffer[1] = CMD_SEND_STATUS;                          // set the command and the status
    txBuffer[2] = status;

    com->txRx(SPI_CS_HANS, 16 - 4, txBuffer, rxBuffer);                  // trasmit the status (10 bytes total, but 4 already received)
}

bool AcsiDataTrans::waitForATN(BYTE atnCode, DWORD timeoutMs)
{
    DWORD startTick = getTickCount();
    BYTE inBuf[4], outBuf[4];
	
	memset(outBuf, 0, 4);

    // wait for specific atn code, but maximum maxLoopCount times
    while(1) {
		if((getTickCount() - startTick) >= timeoutMs) {		// if it takes more than allowed timeout, fail
			outDebugString("waitForATN %02x fail!", atnCode);
			return false;
		}
		
		if( !spi_atn(SPI_ATN_HANS) ) {						// if Hans doesn't signal ATN yet, try later
			usleep(1000);									// wait 1 ms
			continue;
		}
		
        com->txRx(SPI_CS_HANS, 4, inBuf, outBuf);	// receive 2 WORDs - 0 and ATN code

        if(inBuf[3] == atnCode) {                      // ATN code found?
            outDebugString("waitForATN %02x good.", atnCode);
            return true;
        }
    }

	// this shouldn't happen
    return false;
}

DWORD AcsiDataTrans::getTickCount(void)
{
	timespec now;
	
	clock_gettime(CLOCK_MONOTONIC, &now);
	
	DWORD tics;
	tics  = now.tv_sec * 1000;			// convert seconds to milliseconds
	tics += now.tv_nsec / 1000000;		// convert nano to milliseconds (10e-9 -> 10e-3)
	
	return tics;
}
