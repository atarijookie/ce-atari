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

#include "chipinterface4.h"
#include "gpio4.h"
#include "gpio_acsi.h"
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

ChipInterface4::ChipInterface4()
{
#ifndef ONPC
    conSpi = new CConSpi(0, PIN_ATN_FRANZ, 0, SPI_CS_FRANZ);
#else
    conSpi = new CConSpi(0, 0, 0, 0);
#endif

    gpioAcsi = new GpioAcsi();

    ikbdReadFd = -1;
    ikbdWriteFd = -1;

    bufOut = new uint8_t[MFM_STREAM_SIZE];
    bufIn = new uint8_t[MFM_STREAM_SIZE];

    memset(&hansConfigWords, 0, sizeof(hansConfigWords));
}

ChipInterface4::~ChipInterface4()
{
    delete conSpi;
    delete gpioAcsi;

    delete []bufOut;
    delete []bufIn;
}

int ChipInterface4::chipInterfaceType(void)
{
    return CHIP_IF_V4;
}

bool ChipInterface4::ciOpen(void)
{
#ifndef ONPC
    gpioAcsi->init();
    serialSetup();          // open and configure UART for IKBD
    return gpio_open();     // open GPIO and SPI
#else
    return false;
#endif
}

void ChipInterface4::ciClose(void)
{
#ifndef ONPC
    Utils::closeFdIfOpen(ikbdReadFd);
    Utils::closeFdIfOpen(ikbdWriteFd);

    gpio_close();           // close GPIO and SPI
#endif
}

void ChipInterface4::ikbdUartEnable(bool enable)
{
    // don't do anything, it's always enabled
}

int ChipInterface4::ikbdUartReadFd(void)
{
    return ikbdReadFd;
}

int ChipInterface4::ikbdUartWriteFd(void)
{
    return ikbdWriteFd;
}

void ChipInterface4::serialSetup(void)
{
    struct termios termiosStruct;
    termios *ts = &termiosStruct;

    int fd;

    ikbdReadFd = -1;
    ikbdWriteFd = -1;

    std::string uartPath = Utils::dotEnvValue("IKBD_SERIAL_PORT");
    fd = open(uartPath.c_str(), O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);

    if(fd == -1) {
        logDebugAndIkbd(LOG_ERROR, "Failed to open '%s'", uartPath.c_str());
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

void ChipInterface4::resetHDDandFDD(void)
{
#ifndef ONPC
    gpioAcsi->reset();

    bcm2835_gpio_write(PIN_RESET_FRANZ,         LOW);
    Utils::sleepMs(10);                                     // wait a while to let the reset work
    bcm2835_gpio_write(PIN_RESET_FRANZ,         HIGH);
    Utils::sleepMs(50);                                     // wait a while to let the devices boot
#endif
}

void ChipInterface4::resetHDD(void)
{
#ifndef ONPC
    gpioAcsi->reset();
#endif
}

void ChipInterface4::resetFDD(void)
{
#ifndef ONPC
    bcm2835_gpio_write(PIN_RESET_FRANZ,         LOW);
    Utils::sleepMs(10);                                     // wait a while to let the reset work
    bcm2835_gpio_write(PIN_RESET_FRANZ,         HIGH);
    Utils::sleepMs(50);                                     // wait a while to let the devices boot
#endif
}

bool ChipInterface4::actionNeeded(bool &hardNotFloppy, uint8_t *inBuf)
{
#ifndef ONPC
    // if waitForATN() succeeds, it fills 8 bytes of data in buffer
    // ...but then we might need some little more, so let's determine what it was
    // and keep reading as much as needed
    int moreData = 0;

    // check on HDD interface
    bool res = false;
    static uint32_t nextHddFwTime = 0;

    if(Utils::getCurrentMs() >= nextHddFwTime) {    // time to send fake FW version?
        inBuf[3] = ATN_FW_VERSION;
        nextHddFwTime = Utils::getEndTime(1000);
        hardNotFloppy = true;
        return true;
    } else {
        // if config changed, update config in gpio handler
        if(hansConfigWords.next.acsi != hansConfigWords.current.acsi || hansConfigWords.next.fdd  != hansConfigWords.current.fdd) {
            hansConfigWords.current.acsi = hansConfigWords.next.acsi;
            hansConfigWords.current.fdd = hansConfigWords.next.fdd;
            gpioAcsi->setConfig(hansConfigWords.current.acsi >> 8, hansConfigWords.current.acsi);
        }

        res = gpioAcsi->getCmd(inBuf + 8);  // get command if possible

        if(res) {                           // got command? return with success
            inBuf[3] = ATN_ACSI_COMMAND;
            hardNotFloppy = true;
            return true;
        }
    }

    // check for any ATN code waiting from Franz
    res = conSpi->waitForATN(SPI_CS_FRANZ, (uint8_t) ATN_ANY, 0, inBuf);

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

void ChipInterface4::getFWversion(bool hardNotFloppy, uint8_t *inFwVer)
{
#ifndef ONPC
    if(hardNotFloppy) {     // for HDD
        ChipInterface::convertXilinxInfo(gpioAcsi->getXilinxByte());    // convert xilinx info into hwInfo struct
        int year = Utils::bcdToInt(inFwVer[1]) + 2000;
        Update::versions.hans.fromInts(2024, 1, 1);                     // store fake FW version of non-present Hans
    } else {                // for FDD
        // fwResponseBfr should be filled with Franz config - by calling setFDDconfig() (and not calling anything else inbetween)
        conSpi->txRx(SPI_CS_FRANZ, FDD_FW_RESPONSE_LEN, fwResponseBfr, inFwVer);

        int year = Utils::bcdToInt(inFwVer[1]) + 2000;
        Update::versions.franz.fromInts(year, Utils::bcdToInt(inFwVer[2]), Utils::bcdToInt(inFwVer[3]));              // store found FW version of Franz
    }
#endif
}

bool ChipInterface4::hdd_sendData_start(uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "ChipInterface4::hdd_sendData_start -- trying to send more than 16 MB, fail");
        return false;
    }

    gpioAcsi->startTransfer(1, totalDataCount, scsiStatus, withStatus);
    return true;
}

bool ChipInterface4::hdd_sendData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
    return gpioAcsi->sendBlock(pData, dataCount);
}

bool ChipInterface4::hdd_recvData_start(uint8_t *recvBuffer, uint32_t totalDataCount)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "ChipInterface4::hdd_recvData_start() -- trying to send more than 16 MB, fail");
        return false;
    }

    gpioAcsi->startTransfer(0, totalDataCount, 0xff, false);
    return true;
}

bool ChipInterface4::hdd_recvData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
    return gpioAcsi->recvBlock(pData, dataCount);
}

bool ChipInterface4::hdd_sendStatusToHans(uint8_t statusByte)
{
    return gpioAcsi->sendStatus(statusByte);
}

void ChipInterface4::fdd_sendTrackToChip(int byteCount, uint8_t *encodedTrack)
{
#ifndef ONPC
    // send encoded track out, read garbage into bufIn and don't care about it
    conSpi->txRx(SPI_CS_FRANZ, byteCount, encodedTrack, bufIn);
#endif
}

uint8_t* ChipInterface4::fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount)
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

void ChipInterface4::initButtonAndBeeperPins(void)
{
    // button and beeper are not directly attached to GPIO, nothing to do here
}

void ChipInterface4::handleButton(int& btnDownTime, uint32_t& nextScreenTime)
{
    // nothing to do
}

void ChipInterface4::handleBeeperCommand(int beeperCommand, bool floppySoundEnabled)
{
    // nothing to do
}

// returns true if should handle i2c display from RPi
bool ChipInterface4::handlesDisplay(void)
{
    return false;        // v4 does NOT handle display localy
}

// send this display buffer data to remote display
void ChipInterface4::displayBuffer(uint8_t *bfr, uint16_t size)
{
    // TODO: send data to display
}
