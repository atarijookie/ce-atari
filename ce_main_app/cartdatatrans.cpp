#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "debug.h"
#include "utils.h"
#include "gpio.h"
#include "CartDataTrans.h"
#include "native/scsi_defs.h"

#define PIN_XDONE           RPI_V2_GPIO_P1_26
#define PIN_XPIO            RPI_V2_GPIO_P1_24
#define PIN_XDMA            RPI_V2_GPIO_P1_32
#define PIN_XRnW            RPI_V2_GPIO_P1_33
#define PIN_XDataNotStatus  RPI_V2_GPIO_P1_36

#define PIN_D0              RPI_V2_GPIO_P1_12
#define PIN_D1              RPI_V2_GPIO_P1_35
#define PIN_D2              RPI_V2_GPIO_P1_38
#define PIN_D3              RPI_V2_GPIO_P1_40
#define PIN_D4              RPI_V2_GPIO_P1_15
#define PIN_D5              RPI_V2_GPIO_P1_18
#define PIN_D6              RPI_V2_GPIO_P1_22
#define PIN_D7              RPI_V2_GPIO_P1_37

#define DATA_MASK ((1 << D7) | (1 << D6) | (1 << D5) | (1 << D4) | (1 << D3) | (1 << D2) | (1 << D1) | (1 << D0))

CartDataTrans::CartDataTrans()
{
    buffer          = new BYTE[ACSI_BUFFER_SIZE];        // 1 MB buffer
    recvBuffer      = new BYTE[ACSI_BUFFER_SIZE];
    
    memset(buffer,      0, ACSI_BUFFER_SIZE);            // init buffers to zero
    memset(recvBuffer,  0, ACSI_BUFFER_SIZE);
    
    memset(txBuffer, 0, TX_RX_BUFF_SIZE);
    memset(rxBuffer, 0, TX_RX_BUFF_SIZE);
    
    count           = 0;
    status          = SCSI_ST_OK;
    statusWasSet    = false;
    dataDirection   = DATA_DIRECTION_READ;

    dumpNextData    = false;
}

CartDataTrans::~CartDataTrans()
{
    delete []buffer;
    delete []recvBuffer;
}

void CartDataTrans::hwDirForRead(void)
{
    hwDataDirection(true);
}

void CartDataTrans::hwDirForWrite(void)
{
    hwDataDirection(false);
}

void CartDataTrans::hwDataDirection(bool readNotWrite)
{
    int dir = readNotWrite ? BCM2835_GPIO_FSEL_INPT : BCM2835_GPIO_FSEL_OUTP;
    int dataPins[8] = {PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7};
    int i;

    if(readNotWrite) {      // on READ (data from RPi to ST) - 1st set CPLD to read, then data pins to output
        bcm2835_gpio_write(PIN_XRnW, HIGH);
        bcm2835_gpio_write(PIN_XDataNotStatus, HIGH);
    }

    for(i=0; i<8; i++) {    // set direction of data pins
        bcm2835_gpio_fsel(dataPins[i], dir);
    }

    if(!readNotWrite) {      // on WRITE (data from ST to RPi) - 1st set data pins to input, then CPLD to output
        bcm2835_gpio_write(PIN_XRnW, LOW);
        bcm2835_gpio_write(PIN_XDataNotStatus, HIGH);
    }
}

void CartDataTrans::configureHw(void)
{
#ifndef ONPC_NOTHING
    bcm2835_gpio_fsel(PIN_RESET_HANS,  BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(PIN_RESET_HANS, LOW);      // reset lines to RESET

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

    // cpld interface
    // these 2 are the same with the SPI version of CE
    bcm2835_gpio_fsel(PIN_RESET_HANS,       BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_ATN_HANS,         BCM2835_GPIO_FSEL_INPT);

    // the rest is different to SPI version of CE
    bcm2835_gpio_fsel(PIN_XDONE,            BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(PIN_XPIO,             BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_XDMA,             BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_XRnW,             BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_XDataNotStatus,   BCM2835_GPIO_FSEL_OUTP);

    // XPIO and XDMA to idle level
    bcm2835_gpio_write(PIN_XPIO, LOW);
    bcm2835_gpio_write(PIN_XDMA, LOW);

    // set data pins to WRITE direction (from ST to RPi)
    hwDirForWrite();

    bcm2835_gpio_write(PIN_RESET_HANS, HIGH);      // reset lines to RUN (not reset) state
#endif
}

WORD CartDataTrans::getRemainingLength(void)
{
    return 0;   // currently used only on floppy write, which is not supported on CartDataTrans
}

bool CartDataTrans::waitForATN(int whichSpiCs, BYTE *inBuf)
{
    if(whichSpiCs == SPI_CS_FRANZ) {    // no communication with Franz, just quit
        return false;
    }

    BYTE val = bcm2835_gpio_lev(PIN_ATN_HANS);  // read ATN pin
    return (val == HIGH);                       // ATN true if high
}

void CartDataTrans::getCmdLengthFromCmdBytesAcsi(void)
{
    // now it's time to set up the receiver buffer and length
    if((cmd[0] & 0x1f)==0x1f)   {                           // if the command is '0x1f'
        switch((cmd[1] & 0xe0)>>5)                          // get the length of the command
        {
            case  0: cmdLen =  7; break;
            case  1: cmdLen = 11; break;
            case  2: cmdLen = 11; break;
            case  5: cmdLen = 13; break;
            default: cmdLen =  7; break;
        }
    } else {                                                // if it isn't a ICD command
        cmdLen   = 6;                                       // then length is 6 bytes
    }
}

void CartDataTrans::getCommandFromST(void)
{
    hwDirForWrite();

    cmd[0]  = PIO_writeFirst();                     // get byte from ST (waiting for the 1st byte)
    id      = (cmd[0] >> 5) & 0x07;                 // get only device ID

    //----------------------
    if(!enabledIDs[id]) {                           // if this ID is not enabled, quit
        return 0;
    }

    cmdLen = 6;                                     // maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()

    for(i=1; i<cmdLen; i++) {                       // receive the next command bytes
        cmd[i] = PIO_write();                       // drop down IRQ, get byte

        if(brStat != E_OK) {                        // if something was wrong, quit, failed
            resetXilinx();
            return 0;
        }

        if(i == 1) {                                // if we got also the 2nd byte
            getCmdLengthFromCmdBytesAcsi();         // we set up the length of command, etc.
        }
    }

    return 1;
}

// function for sending / receiving data from/to lower levels of communication (e.g. to SPI)
void CartDataTrans::txRx(int whichSpiCs, int count, BYTE *sendBuffer, BYTE *receiveBufer)
{
    if(whichSpiCs == SPI_CS_FRANZ) {    // no communication with Franz, just quit
        return;
    }

    if(count == 14) {       // if it's GET ACSI COMMAND, fill *receiveBufer with command bytes
        memcpy(receiveBufer, cmd, count);
        return;
    }

    if(count == 12) {       // if it's GET FW VERSION, fill *receiveBuffer with fake fw version
        BYTE version[12] = {0xa0, 0x16, 0x01, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        memcpy(receiveBufer, version, count);
        return;
    }
}

// send all data to Hans, including status
void CartDataTrans::sendDataAndStatus(bool fromRetryModule)
{
    if(fromRetryModule) {   // if it's a RETRY, get the copy of data and proceed like it would be from real module
        retryMod->restoreDataAndStatus(dataDirection, count, buffer, statusWasSet, status);
    } else {                // if it's normal run (not a RETRY), let the retry module do a copy of data
        retryMod->copyDataAndStatus(dataDirection, count, buffer, statusWasSet, status);
    }
    
    // for DATA write transmit just the status (the data is already read by app)
    if(dataDirection == DATA_DIRECTION_WRITE) {
        sendStatusToHans(status);
        return;
    }

    if(count == 0 && !statusWasSet) {       // if no data was added and no status was set, nothing to send then
        return;
    }
	//---------------------------------------
	if(dumpNextData) {
	    dumpData();
		dumpNextData = false;
	}
	//---------------------------------------
    // first send the command
    bool res;

    res = sendData_transferBlock(buffer, count);    // transfer this block

    if(res) {               // if transfer block went OK, send status - this is different from AcsiDataTrans, where the status was sent sooner
        sendStatusToHans(status);
    }
}

bool CartDataTrans::sendData_start(DWORD totalDataCount, BYTE scsiStatus, bool withStatus)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "CartDataTrans::sendData_start -- trying to send more than 16 MB, fail");
        return false;
    }

//    devCommand[3] = withStatus ? CMD_DATA_READ_WITH_STATUS : CMD_DATA_READ_WITHOUT_STATUS;  // store command - with or without status
//    devCommand[7] = scsiStatus;                                     // store status
    return true;
}

bool CartDataTrans::sendData_transferBlock(BYTE *pData, DWORD dataCount)
{
    if((dataCount & 1) != 0) {  // odd number of bytes? make it even, we're sending words...
        dataCount++;
    }
    
    while(dataCount > 0) {      // while there's something to send
        DMA_read(*pData);       // let ST read byte
        pData++;                // move to next byte
        dataCount--;            // decrement byte counter

        if(brStat != E_OK) {    // if not ok, return fail
            return false;
        }
    }
    
    return true;                // if got here, all is ok
}

bool CartDataTrans::recvData_start(DWORD totalDataCount)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "CartDataTrans::recvData_start() -- trying to send more than 16 MB, fail");
        return false;
    }

    dataDirection = DATA_DIRECTION_WRITE;                           // let the higher function know that we've done data write -- 130 048 Bytes
    return true;
}

bool CartDataTrans::recvData_transferBlock(BYTE *pData, DWORD dataCount)
{
    BYTE inBuf[8];

    while(dataCount > 0) {      // while there's something to get from ST
        *pData = DMA_write();   // get one byte from ST and sttore it
        pData++;                // move further in buffer
        dataCount--;            // decrement byte counter

        if(brStat != E_OK) {    // if not ok, return fail
            return false;
        }
    }

    return true;                // if got gere, all is OK
}

void CartDataTrans::sendStatusToHans(BYTE statusByte)
{
    PIO_read(statusByte);
}

bool CartDataTrans::timeout(void)
{
    DWORD now = Utils::getCurrentMs();      // get current time
    return (now >= timeoutTime);            // it's an timeout, if timeout time is now
}

void dataWrite(BYTE val)
{
    // GPIO 26-24 + 22-18 of RPi are used as data port, split and shift data into right position
    DWORD value = ((((DWORD) val) & 0xe0) << 19) | ((((DWORD) val) & 0x1f) << 18);
    bcm2835_gpio_write_mask(value, DATA_MASK);
}

BYTE dataRead(void)
{
    // get whole gpio by single read -- taken from bcm library
    volatile DWORD* paddr = bcm2835_gpio + BCM2835_GPLEV0/4;
    DWORD value = bcm2835_peri_read(paddr);

    // GPIO 26-24 + 22-18 of RPi are used as data port, split and shift data into right position
    DWORD upper = (value >> 19) & 0xe0;
    DWORD lower = (value >> 18) & 0x1f;

    BYTE val = upper | lower;   // combine upper and lower part together
    return val;
}

// get 1st CMD byte from ST  -- without setting INT
BYTE CartDataTrans::PIO_writeFirst(void)
{
    hwDirForWrite();                        // data as inputs (write)
    brStat = E_OK;
    timeoutTime = Utils::getEndTime(1000);  // start the timeout timer

    return dataRead();
}

// get next CMD byte from ST -- with setting INT to LOW and waiting for CS
BYTE CartDataTrans::PIO_write(void)
{
    BYTE val = 0;

    // create rising edge on XPIO
    bcm2835_gpio_write(PIN_XPIO, HIGH);     // to HIGH
    bcm2835_gpio_write(PIN_XPIO, LOW);      // to LOW

    while(1) {                  // wait for CS or timeout
        BYTE xdone = bcm2835_gpio_lev(PIN_XDONE);

        if(xdone == HIGH) {     // if CS arrived, read and quit
            return dataRead();
        }

        if(timeout()) {         // if timeout happened
            brStat = E_TimeOut; // set the bridge status
            return 0;
        }
    }
}

// send status byte to host
void CartDataTrans::PIO_read(BYTE val)
{
    hwDirForRead();                         // data as inputs (write)
    dataWrite(val);                         // write the data to output data register

    // create rising edge on XPIO
    bcm2835_gpio_write(PIN_XPIO, HIGH);     // to HIGH
    bcm2835_gpio_write(PIN_XPIO, LOW);      // to LOW

    while(1) {                                                              // wait for CS or timeout
        BYTE xdone = bcm2835_gpio_lev(PIN_XDONE);

        if(xdone == HIGH) {     // if CS arrived
            break;
        }

        if(timeout()) {         // if timeout happened
            brStat = E_TimeOut; // set the bridge status
            break;
        }
    }

    hwDirForWrite();       // data as inputs (write)
}


void CartDataTrans::DMA_read(BYTE val)
{
    dataWrite(val);                         // write the data to output data register

    // create rising edge on aDMA
    bcm2835_gpio_write(PIN_XDMA, HIGH);     // to HIGH
    bcm2835_gpio_write(PIN_XDMA, LOW);      // to LOW

    while(1) {                                                              // wait for ACK or timeout
        BYTE xdone = bcm2835_gpio_lev(PIN_XDONE);

        if(xdone == HIGH) {     // if CS arrived
            break;
        }

        if(timeout()) {         // if timeout happened
            brStat = E_TimeOut; // set the bridge status
            break;
        }
    }
}

BYTE CartDataTrans::DMA_write(void)
{
    // create rising edge on aDMA
    bcm2835_gpio_write(PIN_XDMA, HIGH);     // to HIGH
    bcm2835_gpio_write(PIN_XDMA, LOW);      // to LOW

    while(1) {                                                              // wait for ACK or timeout
        BYTE xdone = bcm2835_gpio_lev(PIN_XDONE);

        if(xdone == HIGH) {     // if CS arrived, read and quit
            return dataRead();
        }

        if(timeout()) {         // if timeout happened
            brStat = E_TimeOut; // set the bridge status
            return 0;
        }
    }
}

