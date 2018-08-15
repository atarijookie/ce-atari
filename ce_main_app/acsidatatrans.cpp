#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "debug.h"
#include "utils.h"
#include "gpio.h"
#include "acsidatatrans.h"
#include "native/scsi_defs.h"

#if defined(ONPC_HIGHLEVEL)
    #include "socks.h"
#endif

AcsiDataTrans::AcsiDataTrans()
{
#if defined(ONPC_HIGHLEVEL)
    bufferRead      = buffer;
    bufferWrite     = recvBuffer;
#endif

    memset(txBuffer, 0, TX_RX_BUFF_SIZE);
    memset(rxBuffer, 0, TX_RX_BUFF_SIZE);
    
    com = new CConSpi();
}

AcsiDataTrans::~AcsiDataTrans()
{
    delete com;
}

void AcsiDataTrans::configureHw(void)
{
#ifndef ONPC_NOTHING

    // pins for XILINX programming
    // outputs: TDI (GPIO3), TCK (GPIO4), TMS (GPIO17)
    bcm2835_gpio_fsel(PIN_TDI,  BCM2835_GPIO_FSEL_OUTP);        // TDI
    bcm2835_gpio_fsel(PIN_TMS,  BCM2835_GPIO_FSEL_OUTP);        // TMS
    bcm2835_gpio_fsel(PIN_TCK,  BCM2835_GPIO_FSEL_OUTP);        // TCK
    // inputs : TDO (GPIO2)
    bcm2835_gpio_fsel(PIN_TDO,  BCM2835_GPIO_FSEL_INPT);        // TDO

    bcm2835_gpio_write(PIN_TDI, LOW);
    bcm2835_gpio_write(PIN_TMS, LOW);
    bcm2835_gpio_write(PIN_TCK, LOW);


    // pins for both STM32 programming
    bcm2835_gpio_fsel(PIN_RESET_HANS,       BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_RESET_FRANZ,      BCM2835_GPIO_FSEL_OUTP);
//  bcm2835_gpio_fsel(PIN_TXD,              BCM2835_GPIO_FSEL_OUTP);
//  bcm2835_gpio_fsel(PIN_RXD,              BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(PIN_TX_SEL1N2,        BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_BOOT0_FRANZ_HANS, BCM2835_GPIO_FSEL_OUTP);

    bcm2835_gpio_write(PIN_TX_SEL1N2,           HIGH);
    bcm2835_gpio_write(PIN_BOOT0_FRANZ_HANS,    LOW);       // BOOT0: L means boot from flash, H means boot the boot loader

    bcm2835_gpio_write(PIN_RESET_HANS,          HIGH);      // reset lines to RUN (not reset) state
    bcm2835_gpio_write(PIN_RESET_FRANZ,         HIGH);

    bcm2835_gpio_fsel(PIN_BEEPER,           BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_BUTTON,           BCM2835_GPIO_FSEL_INPT);

    // pins for communication with Franz and Hans
    bcm2835_gpio_fsel(PIN_ATN_HANS,         BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(PIN_ATN_FRANZ,        BCM2835_GPIO_FSEL_INPT);
//  bcm2835_gpio_fsel(PIN_CS_HANS,          BCM2835_GPIO_FSEL_OUTP);
//  bcm2835_gpio_fsel(PIN_CS_FRANZ,         BCM2835_GPIO_FSEL_OUTP);
//  bcm2835_gpio_fsel(PIN_MOSI,             BCM2835_GPIO_FSEL_OUTP);
//  bcm2835_gpio_fsel(PIN_MISO,             BCM2835_GPIO_FSEL_INPT);
//  bcm2835_gpio_fsel(PIN_SCK,              BCM2835_GPIO_FSEL_OUTP);

    spi_init();
#endif
}

bool AcsiDataTrans::waitForATN(int whichSpiCs, BYTE *inBuf)
{
    return com->waitForATN(whichSpiCs, (BYTE) ATN_ANY, 0, inBuf);
}

// function for sending / receiving data from/to lower levels of communication (e.g. to SPI)
void AcsiDataTrans::txRx(int whichSpiCs, int count, BYTE *sendBuffer, BYTE *receiveBufer)
{
    com->txRx(whichSpiCs, count, sendBuffer, receiveBufer);
}

WORD AcsiDataTrans::getRemainingLength(void)
{
    return com->getRemainingLength();
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
        dumpData();
		dumpNextData = false;
	}
	//---------------------------------------
    // first send the command
    bool res;

    res = sendData_start(count, status, true);      // try to start the read data transfer, with status

    if(!res) {
        return;
    }

    res = sendData_transferBlock(buffer, count);    // transfer this block
}

bool AcsiDataTrans::sendData_start(DWORD totalDataCount, BYTE scsiStatus, bool withStatus)
{
    if(!com) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::sendData_start -- no communication object, fail");
        return false;
    }

    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::sendData_start -- trying to send more than 16 MB, fail");
        return false;
    }

    BYTE devCommand[COMMAND_SIZE];
    memset(devCommand, 0, COMMAND_SIZE);

    devCommand[3] = withStatus ? CMD_DATA_READ_WITH_STATUS : CMD_DATA_READ_WITHOUT_STATUS;  // store command - with or without status
    devCommand[4] = totalDataCount >> 16;                           // store data size
    devCommand[5] = totalDataCount >>  8;
    devCommand[6] = totalDataCount  & 0xff;
    devCommand[7] = scsiStatus;                                     // store status

    com->txRx(SPI_CS_HANS, COMMAND_SIZE, devCommand, recvBuffer);   // transmit this command
    return true;
}

bool AcsiDataTrans::sendData_transferBlock(BYTE *pData, DWORD dataCount)
{
    txBuffer[0] = 0;
    txBuffer[1] = CMD_DATA_MARKER;                                  // mark the start of data

    BYTE inBuf[8];

    if((dataCount & 1) != 0) {                                      // odd number of bytes? make it even, we're sending words...
        dataCount++;
    }
    
    while(dataCount > 0) {                                          // while there's something to send
        bool res = com->waitForATN(SPI_CS_HANS, ATN_READ_MORE_DATA, 1000, inBuf);	// wait for ATN_READ_MORE_DATA

        if(!res) {                                                  // this didn't come? fuck!
            clear();                                                // clear all the variables
            return false;
        }

        DWORD cntNow = (dataCount > 512) ? 512 : dataCount;         // max 512 bytes per transfer

        memcpy(txBuffer + 2, pData, cntNow);                        // copy the data after the header (2 bytes)
        com->txRx(SPI_CS_HANS, cntNow + 4, txBuffer, rxBuffer);     // transmit this buffer with header + terminating zero (WORD)

        pData       += cntNow;                                      // move the data pointer further
        dataCount   -= cntNow;
    }
    
    return true;
}

bool AcsiDataTrans::recvData_start(DWORD totalDataCount)
{
    if(!com) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::recvData_start() -- no communication object, fail!");
        return false;
    }

    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::recvData_start() -- trying to send more than 16 MB, fail");
        return false;
    }

    dataDirection = DATA_DIRECTION_WRITE;                           // let the higher function know that we've done data write -- 130 048 Bytes

    // first send the command and tell Hans that we need WRITE data
    BYTE devCommand[COMMAND_SIZE];
    memset(devCommand, 0, COMMAND_SIZE);

    devCommand[3] = CMD_DATA_WRITE;                                 // store command - WRITE
    devCommand[4] = totalDataCount >> 16;                           // store data size
    devCommand[5] = totalDataCount >>  8;
    devCommand[6] = totalDataCount  & 0xff;
    devCommand[7] = 0xff;                                           // store INVALID status, because the real status will be sent on CMD_SEND_STATUS

    com->txRx(SPI_CS_HANS, COMMAND_SIZE, devCommand, recvBuffer);   // transmit this command
    return true;
}

bool AcsiDataTrans::recvData_transferBlock(BYTE *pData, DWORD dataCount)
{
    memset(txBuffer, 0, TX_RX_BUFF_SIZE);                   // nothing to transmit, really...
    BYTE inBuf[8];

    while(dataCount > 0) {
        // request maximum 512 bytes from host
        DWORD subCount = (dataCount > 512) ? 512 : dataCount;

        bool res = com->waitForATN(SPI_CS_HANS, ATN_WRITE_MORE_DATA, 1000, inBuf); // wait for ATN_WRITE_MORE_DATA

        if(!res) {                                          // this didn't come? fuck!
            clear(false);                                   // clear all the variables - except dataDirection, which will be used for retry
            return false;
        }

        com->txRx(SPI_CS_HANS, subCount + 8 - 4, txBuffer, rxBuffer);    // transmit data (size = subCount) + header and footer (size = 8) - already received 4 bytes
        memcpy(pData, rxBuffer + 2, subCount);              // copy just the data, skip sequence number

        dataCount   -= subCount;                            // decreate the data counter
        pData       += subCount;                            // move in the buffer further
    }

    return true;
}

void AcsiDataTrans::sendStatusToHans(BYTE statusByte)
{
#if defined(ONPC_HIGHLEVEL)        
//    Debug::out(LOG_ERROR, "AcsiDataTrans::sendStatusToHans -- sending statusByte %02x", statusByte);

    serverSocket_write(&statusByte, 1);
#elif defined(ONPC_NOTHING) 
    // nothing here
#else
    BYTE inBuf[8];
    bool res = com->waitForATN(SPI_CS_HANS, ATN_GET_STATUS, 1000, inBuf);   // wait for ATN_GET_STATUS

    if(!res) {
        clear();                                            // clear all the variables
        return;
    }

    memset(txBuffer, 0, 16);                                // clear the tx buffer
    txBuffer[1] = CMD_SEND_STATUS;                          // set the command and the statusByte
    txBuffer[2] = statusByte;

    com->txRx(SPI_CS_HANS, 16 - 8, txBuffer, rxBuffer);     // transmit the statusByte (16 bytes total, but 8 already received)
#endif
}

