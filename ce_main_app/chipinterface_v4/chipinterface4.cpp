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

#define DISPLAY_DATA_SIZE   516

ChipInterface4::ChipInterface4()
{
#ifndef ONPC
    conSpi = new CConSpi(0, PIN_ATN_FRANZ, 0, SPI_CS_FRANZ);
#else
    conSpi = new CConSpi(0, 0, 0, 0);
#endif

    gpioAcsi = NULL;        // no iface yet

    ikbdReadFd = -1;
    ikbdWriteFd = -1;

    bufOut = new uint8_t[MFM_STREAM_SIZE];
    bufIn = new uint8_t[MFM_STREAM_SIZE];

    displayData = new uint8_t[DISPLAY_DATA_SIZE];
    displayDataSize = 0;

    btnDown = false;        // button not pressed
    recoveryLevel = 0;

    memset(&hansConfigWords, 0, sizeof(hansConfigWords));
}

ChipInterface4::~ChipInterface4()
{
    delete conSpi;

    if(gpioAcsi) {          // got some interface? delete it
        delete gpioAcsi;
        gpioAcsi = NULL;
    }

    delete []bufOut;
    delete []bufIn;

    delete []displayData;
}

int ChipInterface4::chipInterfaceType(void)
{
    return CHIP_IF_V4;
}

bool ChipInterface4::ciOpen(void)
{
#ifndef ONPC
    serialSetup();          // open and configure UART for IKBD
    bool ok = gpio_open();  // open GPIO and SPI

    if(ok) {                // if succeeded opening GPIO, we can init iface
        handleIfaceReport(IFACE_ACSI);
    }

    return ok;
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
        if(inBuf[3] == ATN_GET_DISPLAY_DATA) {      // Franz wants display data? send it here and don't let anyone else know
            conSpi->txRx(SPI_CS_FRANZ, displayDataSize, displayData, bufIn);
            return false;
        }

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
    static uint32_t btnDownStart = 0;
    static bool btnDownPrev = false;

    if(hardNotFloppy) {     // for HDD
        ChipInterface::convertXilinxInfo(gpioAcsi->getXilinxByte());    // convert xilinx info into hwInfo struct

        hansConfigWords.current.acsi = hansConfigWords.next.acsi;   // store next values to current
        hansConfigWords.current.fdd  = hansConfigWords.next.fdd;

        inFwVer[4] = currentFloppyImageLed;             // pretend that new floppy LED/slot came from Hans
        inFwVer[9] = recoveryLevel;                     // set recovery level

        recoveryLevel = 0;

        Update::versions.hans.fromInts(2024, 1, 1);     // store fake FW version of non-present Hans
    } else {                // for FDD
        // fwResponseBfr should be filled with Franz config - by calling setFDDconfig() (and not calling anything else inbetween)
        responseAddByte(fwResponseBfr, floppySoundEnabled ? CMD_FRANZ_MODE_4_SOUND_ON : CMD_FRANZ_MODE_4_SOUND_OFF);
        conSpi->txRx(SPI_CS_FRANZ, FDD_FW_RESPONSE_LEN, fwResponseBfr, inFwVer);

        int year = Utils::bcdToInt(inFwVer[1]) + 2000;
        Update::versions.franz.fromInts(year, Utils::bcdToInt(inFwVer[2]), Utils::bcdToInt(inFwVer[3]));              // store found FW version of Franz

        handleIfaceReport(inFwVer[4]);                  // handle interface report - can witch between ACSI and SCSI

        if(inFwVer[5] == BTN_SHUTDOWN) {
            // TODO: handle shutdown
        } else {
            btnDown = (inFwVer[5] == BTN_PRESSED);      // Franz v4 sends button pressed status

            if(btnDownPrev != btnDown) {                // button state changed, so it's not pressed or released
                btnDownPrev = btnDown;

                uint32_t now = Utils::getCurrentMs();
                if(btnDown) {                           // button now pressed?
                    btnDownStart = now;
                } else {                                // button now released?
                    uint32_t btnDownTimeMs = (now - btnDownStart); // calculate how long the button was down in ms
                    if(btnDownTimeMs < 250) {           // short button press? do floppy slot switch
                        handleFloppySlotSwitch();
                    }
                }
            }
        }
    }
#endif
}

void ChipInterface4::handleIfaceReport(uint8_t ifaceReport)
{
    static uint8_t ifaceReportPrev = 0xff;

    if(ifaceReport != IFACE_ACSI && ifaceReport != IFACE_SCSI) {    // invalid value supplied? ignore it and quit
        return;
    }

    if(ifaceReportPrev == ifaceReport) {    // interface not changed to what we've handled last time? just quit
        return;
    }

    ifaceReportPrev = ifaceReport;          // store current iface report

    if(gpioAcsi) {                          // got some interface? delete it
        delete gpioAcsi;
        gpioAcsi = NULL;
    }

    if(ifaceReport == IFACE_ACSI) {         // it's ACSI
        gpioAcsi = new GpioAcsi();
        gpioAcsi->init();
    } else {                                // it's SCSI
        // TODO: implement SCSI
    }
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
    static bool btnDownPrev = false;
    static uint32_t btnDownStart = 0;
    static int btnDownTimePrev = 0;
    static bool powerOffIs = false;
    static bool powerOffWas = false;

    // btnDown state is sent from Franz, but handled here

    uint32_t now = Utils::getCurrentMs();

    if(btnDown) {                                   // if button is pressed
        if(!btnDownPrev) {                          // it was just pressed down (falling edge)
            btnDownStart = now;                     // store button down time
        }

        uint32_t btnDownTimeMs = (now - btnDownStart); // calculate how long the button is down in ms

        //-------------
        // it's the power off interval, if button down time is between this MIN and MAX time
        powerOffIs = (btnDownTimeMs >= PWR_OFF_PRESS_TIME_MIN) && (btnDownTimeMs < PWR_OFF_PRESS_TIME_MAX);

        if(!powerOffWas && powerOffIs) {            // if we just entered power off interval (not was, but now is)
            display_showNow(DISP_SCREEN_POWEROFF);      // show power off question
            nextScreenTime = Utils::getEndTime(1000);   // redraw display in 1 second
        }

        powerOffWas = powerOffIs;                   // store previous value of are-we-in-the-power-off-interval flag

        //-------------
        btnDownTime = btnDownTimeMs / 1000;         // ms to seconds
        if(btnDownTime != btnDownTimePrev) {        // if button down time (in seconds) changed since the last time we've checked, update strings, show on screen
            btnDownTimePrev = btnDownTime;          // store this as previous time

            if(btnDownTime >= PWR_OFF_PRESS_TIME_MAX_SECONDS) { // if we're after the power-off interval
                fillLine_recovery(btnDownTime);         // fill recovery screen lines
                display_showNow(DISP_SCREEN_RECOVERY);  // show the recovery screen
            }
        }
    } else if(!btnDown && btnDownPrev) {    // if button was just released (rising edge)
        // if user was holding button down, we should redraw the recovery screen to something normal
        if(btnDownTime > 0) {
            int refreshInterval = (btnDownTime < 5) ? 1000 : NEXT_SCREEN_INTERVAL;  // if not doing recovery, redraw in 1 second, otherwise in normal interval
            nextScreenTime = Utils::getEndTime(refreshInterval);
        }

        handleRecoveryButtonPress(btnDownTime); // if this is a recovery request, handle it

        btnDownTime = 0;                    // not holding down button anymore, no button down time
        btnDownTimePrev = 0;
    }

    btnDownPrev = btnDown;                  // store current button down state as previous state
}

void ChipInterface4::handleFloppySlotSwitch(void)
{
    uint8_t fddEnabledSlots = hansConfigWords.next.fdd >> 8;

    if(fddEnabledSlots == 0) {              // no slots enabled?
        currentFloppyImageLed = 0xff;       // no LED on
    } else {
        if(currentFloppyImageLed == 0xff) { // no LED set before?
            currentFloppyImageLed = 2;      // start with last LED, it will be switched immediatelly to 0th
        }

        for(int i=0; i<3; i++) {            // we will find next LED in 3 attempts or less
            currentFloppyImageLed++;        // move one LED up
            if(currentFloppyImageLed > 2) { // we're at the end? roll back to 0
                currentFloppyImageLed = 0;
            }

            if(fddEnabledSlots & (1 << currentFloppyImageLed)) {    // this slot enabled? use it
                return;
            }
        }
    }
}

void ChipInterface4::handleRecoveryButtonPress(int btnDownTime)
{
    if(btnDownTime >= 15) {
        recoveryLevel = (uint8_t) 'T';
    } else if(btnDownTime >= 10) {
        recoveryLevel = (uint8_t) 'S';
    } else if(btnDownTime >= 5) {
        recoveryLevel = (uint8_t) 'R';
    }
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

// Send this display buffer data to remote display... once it asks for it.
// Just copy the data at the time of call.
void ChipInterface4::displayBuffer(uint8_t *bfr, uint16_t size)
{
    uint16_t copySize = MIN(DISPLAY_DATA_SIZE - 4, size);       // pick smaller and don't overflow

    Utils::storeWord(displayData, 0xd1da);          // starting tag - DIsplay DAta
    Utils::storeWord(displayData + 2, copySize);    // how much data is there

    displayDataSize = copySize + 4;                 // how much data should be transfered
    memcpy(displayData + 4, bfr, copySize);         // copy in the data
}
