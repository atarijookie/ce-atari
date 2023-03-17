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

#include "chipinterface3.h"
#include "../utils.h"
#include "../debug.h"
#include "../global.h"
#include "../update.h"
#include "../ikbd/ikbd.h"
#include "../display/displaythread.h"

#ifndef ONPC
    #include <bcm2835.h>
#endif

extern THwConfig hwConfig;
extern TFlags    flags;                 // global flags from command line

ChipInterface3::ChipInterface3()
{
    conSpi2 = new CConSpi2();

    pipeFromAtariToRPi[0] = pipeFromAtariToRPi[1] = -1;
    pipeFromRPiToAtari[0] = pipeFromRPiToAtari[1] = -1;

    bufOut = new uint8_t[MFM_STREAM_SIZE];
    bufIn = new uint8_t[MFM_STREAM_SIZE];

    memset(&hansConfigWords, 0, sizeof(hansConfigWords));
}

ChipInterface3::~ChipInterface3()
{
    delete conSpi2;

    delete []bufOut;
    delete []bufIn;
}

int ChipInterface3::chipInterfaceType(void)
{
    return CHIP_IF_V1_V2;
}

bool ChipInterface3::ciOpen(void)
{
#ifndef ONPC
    serialSetup();          // open and configure UART for IKBD
    return gpio_open();     // open GPIO and SPI
#else
    return false;
#endif
}

void ChipInterface3::ciClose(void)
{
    Utils::closeFdIfOpen(pipeFromAtariToRPi[0]);
    Utils::closeFdIfOpen(pipeFromAtariToRPi[1]);

    Utils::closeFdIfOpen(pipeFromRPiToAtari[0]);
    Utils::closeFdIfOpen(pipeFromRPiToAtari[1]);

#ifndef ONPC
    gpio_close();           // close GPIO and SPI
#endif
}

void ChipInterface3::ikbdUartEnable(bool enable)
{
    // no special enabling needed on v3
}

int ChipInterface3::ikbdUartReadFd(void)
{
    return pipeFromAtariToRPi[0];           // IKBD thread reads from Atari, pipe 0
}

int ChipInterface3::ikbdUartWriteFd(void)
{
    return pipeFromRPiToAtari[1];           // IKBD thread writes to Atari, pipe 1
}

void ChipInterface3::serialSetup(void)
{
    // create pipes for communication with ikbd thread
    int res;

    res = pipe2(pipeFromAtariToRPi, O_NONBLOCK);

    if(res  < 0) {
        Debug::out(LOG_ERROR, "failed to create pipeFromAtariToRPi");
    }

    res = pipe2(pipeFromRPiToAtari, O_NONBLOCK);

    if(res  < 0) {
        Debug::out(LOG_ERROR, "failed to create pipeFromRPiToAtari");
    }
}

void ChipInterface3::resetHDDandFDD(void)
{
    resetHDD();     // just use reset HDD as reset FDD in v3 does nothing
}

void ChipInterface3::resetHDD(void)
{
#ifndef ONPC
    bcm2835_gpio_write(PIN_RESET_HANS,          LOW);       // reset lines to RESET state
    Utils::sleepMs(10);                                     // wait a while to let the reset work
    bcm2835_gpio_write(PIN_RESET_HANS,          HIGH);      // reset lines to RUN (not reset) state
    Utils::sleepMs(50);                                     // wait a while to let the devices boot
#endif
}

void ChipInterface3::resetFDD(void)
{
    // nothing to do on v3
}

bool ChipInterface3::actionNeeded(bool &hardNotFloppy, uint8_t *inBuf)
{
    uint16_t marker;

    // check for any ATN code waiting from HDD
    marker = conSpi2->waitForATN(true, (uint8_t) ATN_ANY, 0, inBuf);

    if(marker == MARKER_HDD) {
        hardNotFloppy = true;
        return true;
    }

    // check for any ATN code waiting from FDD
    marker = conSpi2->waitForATN(false, (uint8_t) ATN_ANY, 0, inBuf);

    if(marker == MARKER_FDD) {
        hardNotFloppy = false;
        return true;
    }

    // no action needed
    return false;
}

void ChipInterface3::getFWversion(bool hardNotFloppy, uint8_t *inFwVer)
{
#ifndef ONPC
    if(hardNotFloppy) {     // for HDD
        // fwResponseBfr should be filled with Hans config - by calling setHDDconfig() (and not calling anything else inbetween)
        conSpi2->txRx(SPI_CS_HANS, HDD_FW_RESPONSE_LEN, fwResponseBfr, inFwVer);

        ChipInterface::convertXilinxInfo(inFwVer[5]);  // convert xilinx info into hwInfo struct

        hansConfigWords.current.acsi = MAKEWORD(inFwVer[6], inFwVer[7]);
        hansConfigWords.current.fdd  = MAKEWORD(inFwVer[8],        0);

        int year = Utils::bcdToInt(inFwVer[1]) + 2000;
        Update::versions.hans.fromInts(year, Utils::bcdToInt(inFwVer[2]), Utils::bcdToInt(inFwVer[3]));       // store found FW version of Hans
    } else {                // for FDD
        // fwResponseBfr should be filled with Franz config - by calling setFDDconfig() (and not calling anything else inbetween)
        conSpi2->txRx(SPI_CS_FRANZ, FDD_FW_RESPONSE_LEN, fwResponseBfr, inFwVer);

        int year = Utils::bcdToInt(inFwVer[1]) + 2000;
        Update::versions.franz.fromInts(year, Utils::bcdToInt(inFwVer[2]), Utils::bcdToInt(inFwVer[3]));              // store found FW version of Franz
    }
#endif
}

bool ChipInterface3::hdd_sendData_start(uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus)
{
#ifndef ONPC
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "ChipInterface3::hdd_sendData_start -- trying to send more than 16 MB, fail");
        return false;
    }

    memset(bufOut, 0, COMMAND_SIZE);

    bufOut[3] = withStatus ? CMD_DATA_READ_WITH_STATUS : CMD_DATA_READ_WITHOUT_STATUS;  // store command - with or without status
    bufOut[4] = totalDataCount >> 16;                           // store data size
    bufOut[5] = totalDataCount >>  8;
    bufOut[6] = totalDataCount  & 0xff;
    bufOut[7] = scsiStatus;                                     // store status

    conSpi2->txRx(SPI_CS_HANS, COMMAND_SIZE, bufOut, bufIn);        // transmit this command
#endif

    return true;
}

bool ChipInterface3::hdd_sendData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
#ifndef ONPC
    bufOut[0] = 0;
    bufOut[1] = CMD_DATA_MARKER;                                  // mark the start of data

    if((dataCount & 1) != 0) {                                      // odd number of bytes? make it even, we're sending words...
        dataCount++;
    }

    while(dataCount > 0) {                                          // while there's something to send
        uint16_t marker = conSpi2->waitForATN(true, ATN_READ_MORE_DATA, 1000, bufIn);   // wait for ATN_READ_MORE_DATA

        if(!res) {                                                  // this didn't come? fuck!
            return false;
        }

        uint32_t cntNow = (dataCount > 512) ? 512 : dataCount;         // max 512 bytes per transfer

        memcpy(bufOut + 2, pData, cntNow);                          // copy the data after the header (2 bytes)
        conSpi2->txRx(SPI_CS_HANS, cntNow + 4, bufOut, bufIn);          // transmit this buffer with header + terminating zero (uint16_t)

        pData       += cntNow;                                      // move the data pointer further
        dataCount   -= cntNow;
    }
#endif

    return true;
}

bool ChipInterface3::hdd_recvData_start(uint8_t *recvBuffer, uint32_t totalDataCount)
{
#ifndef ONPC
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "ChipInterface3::hdd_recvData_start() -- trying to send more than 16 MB, fail");
        return false;
    }

    memset(bufOut, 0, COMMAND_SIZE);

    // first send the command and tell Hans that we need WRITE data
    bufOut[3] = CMD_DATA_WRITE;                                 // store command - WRITE
    bufOut[4] = totalDataCount >> 16;                           // store data size
    bufOut[5] = totalDataCount >>  8;
    bufOut[6] = totalDataCount  & 0xff;
    bufOut[7] = 0xff;                                           // store INVALID status, because the real status will be sent on CMD_SEND_STATUS

    conSpi2->txRx(SPI_CS_HANS, COMMAND_SIZE, bufOut, bufIn);        // transmit this command
#endif
    return true;
}

bool ChipInterface3::hdd_recvData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
#ifndef ONPC
    memset(bufOut, 0, TX_RX_BUFF_SIZE);                   // nothing to transmit, really...
    uint8_t inBuf[8];

    while(dataCount > 0) {
        // request maximum 512 bytes from host
        uint32_t subCount = (dataCount > 512) ? 512 : dataCount;

        uint16_t marker = conSpi2->waitForATN(true, ATN_WRITE_MORE_DATA, 1000, inBuf); // wait for ATN_WRITE_MORE_DATA

        if(!res) {                                          // this didn't come? fuck!
            return false;
        }

        conSpi2->txRx(SPI_CS_HANS, subCount + 8 - 4, bufOut, bufIn);    // transmit data (size = subCount) + header and footer (size = 8) - already received 4 bytes
        memcpy(pData, bufIn + 2, subCount);                 // copy just the data, skip sequence number

        dataCount   -= subCount;                            // decreate the data counter
        pData       += subCount;                            // move in the buffer further
    }
#endif

    return true;
}

bool ChipInterface3::hdd_sendStatusToHans(uint8_t statusByte)
{
#ifndef ONPC
    uint16_t marker = conSpi2->waitForATN(true, ATN_GET_STATUS, 1000, bufIn);   // wait for ATN_GET_STATUS

    if(!res) {
        return false;
    }

    memset(bufOut, 0, 16);                                // clear the tx buffer
    bufOut[1] = CMD_SEND_STATUS;                          // set the command and the statusByte
    bufOut[2] = statusByte;

    conSpi2->txRx(SPI_CS_HANS, 16 - 8, bufOut, bufIn);        // transmit the statusByte (16 bytes total, but 8 already received)
#endif

    return true;
}

void ChipInterface3::fdd_sendTrackToChip(int byteCount, uint8_t *encodedTrack)
{
#ifndef ONPC
    // send encoded track out, read garbage into bufIn and don't care about it
    conSpi2->txRx(SPI_CS_FRANZ, byteCount, encodedTrack, bufIn);
#endif
}

uint8_t* ChipInterface3::fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount)
{
#ifndef ONPC
    byteCount = conSpi2->getRemainingLength();               // get how many data we still have

    memset(bufOut, 0, byteCount);                           // clear the output buffer before sending it to Franz (just in case)
    conSpi2->txRx(SPI_CS_FRANZ, byteCount, bufOut, bufIn);   // get all the remaining data

    // get the written sector, side, track number
    sector  = bufIn[1];
    track   = bufIn[0] & 0x7f;
    side    = (bufIn[0] & 0x80) ? 1 : 0;
#endif

    return bufIn;                                           // return pointer to received written sector
}

void ChipInterface3::handleButton(int& btnDownTime, uint32_t& nextScreenTime)
{
    // v3 does NOT handle button from RPi
}

void ChipInterface3::handleBeeperCommand(int beeperCommand, bool floppySoundEnabled)
{
    // v3 does NOT handle beeper from RPi
}

// returns true if should handle i2c display from RPi
bool ChipInterface3::handlesDisplay(void)
{
    // v3 does NOT handle display from RPi
    return false;
}

// send this display buffer data to remote display
void ChipInterface3::displayBuffer(uint8_t *bfr, uint16_t size)
{
    // TODO: send this buffer to chip
}
