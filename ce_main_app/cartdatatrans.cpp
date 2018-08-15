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

#define DATA_MASK ((1 << PIN_D7) | (1 << PIN_D6) | (1 << PIN_D5) | (1 << PIN_D4) | (1 << PIN_D3) | (1 << PIN_D2) | (1 << PIN_D1) | (1 << PIN_D0))

CartDataTrans::CartDataTrans()
{
    gotCmd = false;     // initially - we don't have a command
    memset(cmd, 0, sizeof(cmd));

    nextFakeFwAtn = Utils::getEndTime(1000);
}

CartDataTrans::~CartDataTrans()
{

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
    inBuf[3] = 0;                       // no ATN at this moment

    if(whichSpiCs == SPI_CS_FRANZ) {    // no communication with Franz, just quit
        return false;
    }

    DWORD now = Utils::getCurrentMs();          // get current time

    if(now >= nextFakeFwAtn) {                  // if we should send next fake FW ATN
        nextFakeFwAtn = Utils::getEndTime(1000); // get next time we should do the same
        inBuf[3] = ATN_FW_VERSION;              // pretend it's FW version ATN
        return true;
    }

    BYTE val = bcm2835_gpio_lev(PIN_ATN_HANS);  // read ATN pin
    if (val == HIGH) {                          // ATN true if high
        getCommandFromST();                     // try to get command from ST

        if(gotCmd) {
            inBuf[3] = ATN_ACSI_COMMAND;        // pretend it's ACSI command ATN
            return true;
        }
    }

    return false;           // if came here, no ATN
}

int CartDataTrans::getCmdLengthFromCmdBytesAcsi(void)
{
    // now it's time to set up the receiver buffer and length
    if((cmd[0] & 0x1f)==0x1f)   {       // if the command is '0x1f'
        switch((cmd[1] & 0xe0)>>5)      // get the length of the command
        {
            case  0: return  7; break;
            case  1: return 11; break;
            case  2: return 11; break;
            case  5: return 13; break;
            default: return  7; break;
        }
    } else {                            // if it isn't a ICD command
        return 6;                       // then length is 6 bytes
    }
}

void CartDataTrans::resetCpld(void)
{
    bcm2835_gpio_write(PIN_RESET_HANS, LOW);
    bcm2835_gpio_write(PIN_RESET_HANS, HIGH);
}

void CartDataTrans::getCommandFromST(void)
{
    gotCmd = false;                 // initially no command
    memset(cmd, 0, sizeof(cmd));

    cmd[0] = PIO_writeFirst();      // get byte from ST (waiting for the 1st byte)
    int id = (cmd[0] >> 5) & 0x07;  // get only device ID

    //----------------------
    if((acsiIdInfo.enabledIDbits & (1 << id)) == 0) {   // if this ID is not enabled, quit
        resetCpld();
        return;
    }

    cmdLen = 6;                                     // maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()
    int i;

    for(i=1; i<cmdLen; i++) {                       // receive the next command bytes
        cmd[i] = PIO_write();                       // drop down IRQ, get byte

        if(brStat != E_OK) {                        // if something was wrong, quit, failed
            resetCpld();
            return;
        }

        if(i == 1) {                                    // if we got also the 2nd byte
            cmdLen = getCmdLengthFromCmdBytesAcsi();    // we set up the length of command, etc.
        }
    }

    gotCmd = true;
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

bool CartDataTrans::sendData_start(DWORD totalDataCount, BYTE scsiStatus, bool withStatus)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "CartDataTrans::sendData_start -- trying to send more than 16 MB, fail");
        return false;
    }

    return true;
}

bool CartDataTrans::sendData_transferBlock(BYTE *pData, DWORD dataCount)
{
    hwDirForRead();             // data as outputs (ST does data READ)

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
    hwDirForWrite();            // data as inputs (ST does data WRITE)

    while(dataCount > 0) {      // while there's something to get from ST
        *pData = DMA_write();   // get one byte from ST and store it
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

void CartDataTrans::writeDataToGPIO(BYTE val)
{
    // GPIO 26-24 + 22-18 of RPi are used as data port, split and shift data into right position
    DWORD value = ((((DWORD) val) & 0xe0) << 19) | ((((DWORD) val) & 0x1f) << 18);
    bcm2835_gpio_write_mask(value, DATA_MASK);
}

BYTE CartDataTrans::readDataFromGPIO(void)
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
    hwDirForWrite();                        // data as inputs (ST does data WRITE)
    brStat = E_OK;
    timeoutTime = Utils::getEndTime(1000);  // start the timeout timer

    return readDataFromGPIO();
}

// get next CMD byte from ST -- with setting INT to LOW and waiting for CS
BYTE CartDataTrans::PIO_write(void)
{
    // create rising edge on XPIO
    bcm2835_gpio_write(PIN_XPIO, HIGH);     // to HIGH
    bcm2835_gpio_write(PIN_XPIO, LOW);      // to LOW

    while(1) {                  // wait for CS or timeout
        BYTE xdone = bcm2835_gpio_lev(PIN_XDONE);

        if(xdone == HIGH) {     // if CS arrived, read and quit
            return readDataFromGPIO();
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
    hwDirForRead();                         // data as outputs (ST does data READ)
    writeDataToGPIO(val);                   // write the data to output data register

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
    writeDataToGPIO(val);                   // write the data to output data register

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
            return readDataFromGPIO();
        }

        if(timeout()) {         // if timeout happened
            brStat = E_TimeOut; // set the bridge status
            return 0;
        }
    }
}

