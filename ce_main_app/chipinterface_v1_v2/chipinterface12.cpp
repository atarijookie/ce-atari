#include <string.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <sys/select.h>
#include <sys/inotify.h>
#include <limits.h>

#include "chipinterface12.h"
#include "gpio.h"
#include "../utils.h"
#include "../debug.h"
#include "../global.h"
#include "../update.h"
#include "../ikbd/ikbd.h"

#ifndef ONPC
    #include <bcm2835.h>
#endif

extern THwConfig hwConfig;
extern TFlags    flags;                 // global flags from command line

ChipInterface12::ChipInterface12()
{
    conSpi = new CConSpi();

    ikbdReadFd = -1;
    ikbdWriteFd = -1;

    bufOut = new BYTE[MFM_STREAM_SIZE];
    bufIn = new BYTE[MFM_STREAM_SIZE];

    memset(&hansConfigWords, 0, sizeof(hansConfigWords));
}

ChipInterface12::~ChipInterface12()
{
    delete conSpi;

    delete []bufOut;
    delete []bufIn;
}

int ChipInterface12::chipInterfaceType(void)
{
    return CHIP_IF_V1_V2;
}

bool ChipInterface12::ciOpen(void)
{
#ifndef ONPC
    serialSetup();          // open and configure UART for IKBD
    return gpio_open();     // open GPIO and SPI
#else
    return false;
#endif
}

void ChipInterface12::ciClose(void)
{
#ifndef ONPC
    if(ikbdReadFd != -1) {  // if got fd, close it
        close(ikbdReadFd);
        ikbdReadFd = -1;
    }

    if(ikbdWriteFd != -1) { // if got fd, close it
        close(ikbdWriteFd);
        ikbdWriteFd = -1;
    }

    gpio_close();           // close GPIO and SPI
#endif
}

void ChipInterface12::ikbdUartEnable(bool enable)
{
#ifndef ONPC
    if(enable) {
        bcm2835_gpio_write(PIN_TX_SEL1N2, HIGH);            // TX_SEL1N2, switch the RX line to receive from Franz, which does the 9600 to 7812 baud translation
    }
#endif
}

int ChipInterface12::ikbdUartReadFd(void)
{
    return ikbdReadFd;
}

int ChipInterface12::ikbdUartWriteFd(void)
{
    return ikbdWriteFd;
}

void ChipInterface12::serialSetup(void)
{
    struct termios termiosStruct;
    termios *ts = &termiosStruct;

    int fd;

    ikbdReadFd = -1;
    ikbdWriteFd = -1;

    fd = open(UARTFILE, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
    if(fd == -1) {
        logDebugAndIkbd(LOG_ERROR, "Failed to open %s", UARTFILE);
        return;
    }

    fcntl(fd, F_SETFL, 0);
    tcgetattr(fd, ts);

    /* reset the settings */
    cfmakeraw(ts);
    ts->c_cflag &= ~(CSIZE | CRTSCTS);
    ts->c_iflag &= ~(IXON | IXOFF | IXANY | IGNPAR);
    ts->c_lflag &= ~(ECHOK | ECHOCTL | ECHOKE);
    ts->c_oflag &= ~(OPOST | ONLCR);

    /* setup the new settings */
    cfsetispeed(ts, B19200);
    cfsetospeed(ts, B19200);
    ts->c_cflag |=  CS8 | CLOCAL | CREAD;            // uart: 8N1

    ts->c_cc[VMIN ] = 0;
    ts->c_cc[VTIME] = 0;

    /* set the settings */
    tcflush(fd, TCIFLUSH);

    if (tcsetattr(fd, TCSANOW, ts) != 0) {
        close(fd);
        return;
    }

    /* confirm they were set */
    struct termios settings;
    tcgetattr(fd, &settings);
    if (settings.c_iflag != ts->c_iflag ||
        settings.c_oflag != ts->c_oflag ||
        settings.c_cflag != ts->c_cflag ||
        settings.c_lflag != ts->c_lflag) {
        close(fd);
        return;
    }

    fcntl(fd, F_SETFL, FNDELAY);                    // make reading non-blocking

    // on real UART same fd is used for read and write
    ikbdReadFd = fd;
    ikbdWriteFd = fd;
}

void ChipInterface12::resetHDDandFDD(void)
{
#ifndef ONPC
    bcm2835_gpio_write(PIN_RESET_HANS,          LOW);       // reset lines to RESET state
    bcm2835_gpio_write(PIN_RESET_FRANZ,         LOW);

    Utils::sleepMs(10);                                     // wait a while to let the reset work

    bcm2835_gpio_write(PIN_RESET_HANS,          HIGH);      // reset lines to RUN (not reset) state
    bcm2835_gpio_write(PIN_RESET_FRANZ,         HIGH);

    Utils::sleepMs(50);                                     // wait a while to let the devices boot
#endif
}

void ChipInterface12::resetHDD(void)
{
#ifndef ONPC
    bcm2835_gpio_write(PIN_RESET_HANS,          LOW);       // reset lines to RESET state
    Utils::sleepMs(10);                                     // wait a while to let the reset work
    bcm2835_gpio_write(PIN_RESET_HANS,          HIGH);      // reset lines to RUN (not reset) state
    Utils::sleepMs(50);                                     // wait a while to let the devices boot
#endif
}

void ChipInterface12::resetFDD(void)
{
#ifndef ONPC
    bcm2835_gpio_write(PIN_RESET_FRANZ,         LOW);
    Utils::sleepMs(10);                                     // wait a while to let the reset work
    bcm2835_gpio_write(PIN_RESET_FRANZ,         HIGH);
    Utils::sleepMs(50);                                     // wait a while to let the devices boot
#endif
}

bool ChipInterface12::actionNeeded(bool &hardNotFloppy, BYTE *inBuf)
{
#ifndef ONPC
    // if waitForATN() succeeds, it fills 8 bytes of data in buffer
    // ...but then we might need some little more, so let's determine what it was
    // and keep reading as much as needed
    int moreData = 0;

    // check for any ATN code waiting from Hans
    bool res = conSpi->waitForATN(SPI_CS_HANS, (BYTE) ATN_ANY, 0, inBuf);

    if(res) {    // HANS is signaling attention?
        if(inBuf[3] == ATN_ACSI_COMMAND) {
            moreData = 14;                              // all ACSI command bytes
        }

        if(moreData) {
            conSpi->txRx(SPI_CS_HANS, moreData, bufOut, inBuf + 8);   // get more data, offset in input buffer by header size
        }

        hardNotFloppy = true;
        return true;
    }

    // check for any ATN code waiting from Franz
    res = conSpi->waitForATN(SPI_CS_FRANZ, (BYTE) ATN_ANY, 0, inBuf);

    if(res) {    // FRANZ is signaling attention?
        if(inBuf[3] == ATN_SEND_TRACK) {
            moreData = 2;                               // side + track
        }

        if(moreData) {
            conSpi->txRx(SPI_CS_FRANZ, moreData, bufOut, inBuf + 8);   // get more data, offset in input buffer by header size
        }

        hardNotFloppy = false;
        return true;
    }
#endif

    // no action needed
    return false;
}

void ChipInterface12::getFWversion(bool hardNotFloppy, BYTE *inFwVer)
{
#ifndef ONPC
    if(hardNotFloppy) {     // for HDD
        // fwResponseBfr should be filled with Hans config - by calling setHDDconfig() (and not calling anything else inbetween)
        conSpi->txRx(SPI_CS_HANS, HDD_FW_RESPONSE_LEN, fwResponseBfr, inFwVer);

        ChipInterface::convertXilinxInfo(inFwVer[5]);  // convert xilinx info into hwInfo struct

        hansConfigWords.current.acsi = MAKEWORD(inFwVer[6], inFwVer[7]);
        hansConfigWords.current.fdd  = MAKEWORD(inFwVer[8],        0);

        int year = Utils::bcdToInt(inFwVer[1]) + 2000;
        Update::versions.hans.fromInts(year, Utils::bcdToInt(inFwVer[2]), Utils::bcdToInt(inFwVer[3]));       // store found FW version of Hans
    } else {                // for FDD
        // fwResponseBfr should be filled with Franz config - by calling setFDDconfig() (and not calling anything else inbetween)
        conSpi->txRx(SPI_CS_FRANZ, FDD_FW_RESPONSE_LEN, fwResponseBfr, inFwVer);

        int year = Utils::bcdToInt(inFwVer[1]) + 2000;
        Update::versions.franz.fromInts(year, Utils::bcdToInt(inFwVer[2]), Utils::bcdToInt(inFwVer[3]));              // store found FW version of Franz
    }
#endif
}

bool ChipInterface12::hdd_sendData_start(DWORD totalDataCount, BYTE scsiStatus, bool withStatus)
{
#ifndef ONPC
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::sendData_start -- trying to send more than 16 MB, fail");
        return false;
    }

    memset(bufOut, 0, COMMAND_SIZE);

    bufOut[3] = withStatus ? CMD_DATA_READ_WITH_STATUS : CMD_DATA_READ_WITHOUT_STATUS;  // store command - with or without status
    bufOut[4] = totalDataCount >> 16;                           // store data size
    bufOut[5] = totalDataCount >>  8;
    bufOut[6] = totalDataCount  & 0xff;
    bufOut[7] = scsiStatus;                                     // store status

    conSpi->txRx(SPI_CS_HANS, COMMAND_SIZE, bufOut, bufIn);        // transmit this command
#endif

    return true;
}

bool ChipInterface12::hdd_sendData_transferBlock(BYTE *pData, DWORD dataCount)
{
#ifndef ONPC
    bufOut[0] = 0;
    bufOut[1] = CMD_DATA_MARKER;                                  // mark the start of data

    if((dataCount & 1) != 0) {                                      // odd number of bytes? make it even, we're sending words...
        dataCount++;
    }

    while(dataCount > 0) {                                          // while there's something to send
        bool res = conSpi->waitForATN(SPI_CS_HANS, ATN_READ_MORE_DATA, 1000, bufIn);   // wait for ATN_READ_MORE_DATA

        if(!res) {                                                  // this didn't come? fuck!
            return false;
        }

        DWORD cntNow = (dataCount > 512) ? 512 : dataCount;         // max 512 bytes per transfer

        memcpy(bufOut + 2, pData, cntNow);                          // copy the data after the header (2 bytes)
        conSpi->txRx(SPI_CS_HANS, cntNow + 4, bufOut, bufIn);          // transmit this buffer with header + terminating zero (WORD)

        pData       += cntNow;                                      // move the data pointer further
        dataCount   -= cntNow;
    }
#endif

    return true;
}

bool ChipInterface12::hdd_recvData_start(BYTE *recvBuffer, DWORD totalDataCount)
{
#ifndef ONPC
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::recvData_start() -- trying to send more than 16 MB, fail");
        return false;
    }

    memset(bufOut, 0, COMMAND_SIZE);

    // first send the command and tell Hans that we need WRITE data
    bufOut[3] = CMD_DATA_WRITE;                                 // store command - WRITE
    bufOut[4] = totalDataCount >> 16;                           // store data size
    bufOut[5] = totalDataCount >>  8;
    bufOut[6] = totalDataCount  & 0xff;
    bufOut[7] = 0xff;                                           // store INVALID status, because the real status will be sent on CMD_SEND_STATUS

    conSpi->txRx(SPI_CS_HANS, COMMAND_SIZE, bufOut, bufIn);        // transmit this command
#endif
    return true;
}

bool ChipInterface12::hdd_recvData_transferBlock(BYTE *pData, DWORD dataCount)
{
#ifndef ONPC
    memset(bufOut, 0, TX_RX_BUFF_SIZE);                   // nothing to transmit, really...
    BYTE inBuf[8];

    while(dataCount > 0) {
        // request maximum 512 bytes from host
        DWORD subCount = (dataCount > 512) ? 512 : dataCount;

        bool res = conSpi->waitForATN(SPI_CS_HANS, ATN_WRITE_MORE_DATA, 1000, inBuf); // wait for ATN_WRITE_MORE_DATA

        if(!res) {                                          // this didn't come? fuck!
            return false;
        }

        conSpi->txRx(SPI_CS_HANS, subCount + 8 - 4, bufOut, bufIn);    // transmit data (size = subCount) + header and footer (size = 8) - already received 4 bytes
        memcpy(pData, bufIn + 2, subCount);                 // copy just the data, skip sequence number

        dataCount   -= subCount;                            // decreate the data counter
        pData       += subCount;                            // move in the buffer further
    }
#endif

    return true;
}

bool ChipInterface12::hdd_sendStatusToHans(BYTE statusByte)
{
#ifndef ONPC
    bool res = conSpi->waitForATN(SPI_CS_HANS, ATN_GET_STATUS, 1000, bufIn);   // wait for ATN_GET_STATUS

    if(!res) {
        return false;
    }

    memset(bufOut, 0, 16);                                // clear the tx buffer
    bufOut[1] = CMD_SEND_STATUS;                          // set the command and the statusByte
    bufOut[2] = statusByte;

    conSpi->txRx(SPI_CS_HANS, 16 - 8, bufOut, bufIn);        // transmit the statusByte (16 bytes total, but 8 already received)
#endif

    return true;
}

void ChipInterface12::fdd_sendTrackToChip(int byteCount, BYTE *encodedTrack)
{
#ifndef ONPC
    // send encoded track out, read garbage into bufIn and don't care about it
    conSpi->txRx(SPI_CS_FRANZ, byteCount, encodedTrack, bufIn);
#endif
}

BYTE* ChipInterface12::fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount)
{
#ifndef ONPC
    byteCount = conSpi->getRemainingLength();               // get how many data we still have

    memset(bufOut, 0, byteCount);                           // clear the output buffer before sending it to Franz (just in case)
    conSpi->txRx(SPI_CS_FRANZ, byteCount, bufOut, bufIn);   // get all the remaining data

    // get the written sector, side, track number
    sector  = bufIn[1];
    track   = bufIn[0] & 0x7f;
    side    = (bufIn[0] & 0x80) ? 1 : 0;
#endif

    return bufIn;                                           // return pointer to received written sector
}

