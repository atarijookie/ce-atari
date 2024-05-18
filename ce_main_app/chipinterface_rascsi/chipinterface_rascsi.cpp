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

#include "chipinterface_rascsi.h"
#include "gpio_rascsi.h"
#include "gpio_scsi.h"
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

ChipInterfaceRaSCSI::ChipInterfaceRaSCSI()
{
    gpioScsi = NULL;                    // no iface yet
    maximumTransferSize = (254 * 512);  // maximum ACSI transfer size - 254 sectors

    ikbdReadFd = -1;
    ikbdWriteFd = -1;

    bufOut = new uint8_t[MFM_STREAM_SIZE];
    bufIn = new uint8_t[MFM_STREAM_SIZE];

    displayData = new uint8_t[DISPLAY_DATA_SIZE];
    displayDataSize = 0;

    btnDown = false;        // button not pressed
    recoveryLevel = 0;

    memset(&hansConfigWords, 0, sizeof(hansConfigWords));

    gpioScsi = new GpioScsi();
    maximumTransferSize = 14 * 1024 * 1024;     // TT has max 10 MB of ST RAM, Falcon has max 14 MB of ST RAM
}

ChipInterfaceRaSCSI::~ChipInterfaceRaSCSI()
{
    if(gpioScsi) {          // got some interface? delete it
        delete gpioScsi;
        gpioScsi = NULL;
    }

    delete []bufOut;
    delete []bufIn;

    delete []displayData;
}

int ChipInterfaceRaSCSI::chipInterfaceType(void)
{
    return CHIP_IF_RASCSI;
}

bool ChipInterfaceRaSCSI::ciOpen(void)
{
#ifndef ONPC
    serialSetup();          // open and configure UART for IKBD
    bool ok = gpiorascsi_open(); // open GPIO and SPI

    Debug::out(LOG_DEBUG, "ChipInterfaceRaSCSI::ciOpen - ok=%d", (int) ok);

    flags.noFranz = true;   // after initialization - set that we don't have Franz in RaSCSI
    return ok;
#else
    return false;
#endif
}

void ChipInterfaceRaSCSI::ciClose(void)
{
#ifndef ONPC
    Utils::closeFdIfOpen(ikbdReadFd);
    Utils::closeFdIfOpen(ikbdWriteFd);

    gpiorascsi_close();           // close GPIO
#endif
}

void ChipInterfaceRaSCSI::ikbdUartEnable(bool enable)
{
    // don't do anything, it's always enabled
}

int ChipInterfaceRaSCSI::ikbdUartReadFd(void)
{
    return ikbdReadFd;
}

int ChipInterfaceRaSCSI::ikbdUartWriteFd(void)
{
    return ikbdWriteFd;
}

void ChipInterfaceRaSCSI::serialSetup(void)
{
    ikbdReadFd = -1;
    ikbdWriteFd = -1;
}

void ChipInterfaceRaSCSI::resetHDDandFDD(void)
{
#ifndef ONPC
    gpioScsi->reset();
#endif
}

void ChipInterfaceRaSCSI::resetHDD(void)
{
#ifndef ONPC
    gpioScsi->reset();
#endif
}

void ChipInterfaceRaSCSI::resetFDD(void)
{
    // nothing on RaSCSI - no FDD
}

bool ChipInterfaceRaSCSI::actionNeeded(bool &hardNotFloppy, uint8_t *inBuf)
{
#ifndef ONPC
    // if waitForATN() succeeds, it fills 8 bytes of data in buffer
    // ...but then we might need some little more, so let's determine what it was
    // and keep reading as much as needed

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
            gpioScsi->setConfig(hansConfigWords.current.acsi >> 8, hansConfigWords.current.acsi);
        }

        res = gpioScsi->getCmd(inBuf + 8);  // get command if possible

        if(res) {                           // got command? return with success
            inBuf[3] = ATN_ACSI_COMMAND;
            hardNotFloppy = true;
            return true;
        }
    }

    // no Franz checking in RaSCSI - there is no Franz
#endif

    // no action needed
    return false;
}

void ChipInterfaceRaSCSI::getFWversion(bool hardNotFloppy, uint8_t *inFwVer)
{
#ifndef ONPC
    if(hardNotFloppy) {     // for HDD
        ChipInterface::convertXilinxInfo(gpioScsi->getXilinxByte());    // convert xilinx info into hwInfo struct

        if(hansConfigWords.next.acsi != hansConfigWords.current.acsi) { // config changed?
            gpioScsi->setConfig(hansConfigWords.next.acsi >> 8, hansConfigWords.next.acsi);
        }

        hansConfigWords.current.acsi = hansConfigWords.next.acsi;   // store next values to current
        hansConfigWords.current.fdd  = hansConfigWords.next.fdd;

        inFwVer[4] = currentFloppyImageLed;             // pretend that new floppy LED/slot came from Hans
        inFwVer[9] = recoveryLevel;                     // set recovery level

        recoveryLevel = 0;

        Update::versions.hans.fromInts(2024, 1, 1);     // store fake FW version of non-present Hans
    }
#endif
}

bool ChipInterfaceRaSCSI::hdd_sendData_start(uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus)
{
    if(totalDataCount > maximumTransferSize) {
        Debug::out(LOG_ERROR, "ChipInterfaceRaSCSI::hdd_sendData_start -- totalDataCount: %d, trying to send more than %d bytes, fail", totalDataCount, maximumTransferSize);
        return false;
    }

    gpioScsi->startTransfer(1, totalDataCount, scsiStatus, withStatus);
    return true;
}

bool ChipInterfaceRaSCSI::hdd_sendData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
    return gpioScsi->sendBlock(pData, dataCount);
}

bool ChipInterfaceRaSCSI::hdd_recvData_start(uint8_t *recvBuffer, uint32_t totalDataCount)
{
    if(totalDataCount > maximumTransferSize) {
        Debug::out(LOG_ERROR, "ChipInterfaceRaSCSI::hdd_recvData_start() -- totalDataCount: %d, trying to send more than %d bytes, fail", totalDataCount, maximumTransferSize);
        return false;
    }

    gpioScsi->startTransfer(0, totalDataCount, 0xff, false);
    return true;
}

bool ChipInterfaceRaSCSI::hdd_recvData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
    return gpioScsi->recvBlock(pData, dataCount);
}

bool ChipInterfaceRaSCSI::hdd_sendStatusToHans(uint8_t statusByte)
{
    return gpioScsi->sendStatus(statusByte);
}

void ChipInterfaceRaSCSI::fdd_sendTrackToChip(int byteCount, uint8_t *encodedTrack)
{
    // nothing - RaSCSI doesn't have FDD emulation
}

uint8_t* ChipInterfaceRaSCSI::fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount)
{
    // nothing - RaSCSI doesn't have FDD emulation
    return bufIn;                                           // return pointer to received written sector
}

void ChipInterfaceRaSCSI::initButtonAndBeeperPins(void)
{
    // button and beeper are not directly attached to GPIO, nothing to do here
}

void ChipInterfaceRaSCSI::handleButton(int& btnDownTime, uint32_t& nextScreenTime)
{
    // nothing - RaSCSI doesn't have button
}

void ChipInterfaceRaSCSI::handleFloppySlotSwitch(void)
{
    // nothing - RaSCSI doesn't have FDD emulation
}

void ChipInterfaceRaSCSI::handleRecoveryButtonPress(int btnDownTime)
{
    // nothing
}

void ChipInterfaceRaSCSI::handleBeeperCommand(int beeperCommand, bool floppySoundEnabled)
{
    // nothing to do
}

// returns true if should handle i2c display from RPi
bool ChipInterfaceRaSCSI::handlesDisplay(void)
{
    return false;        // v4 does NOT handle display localy
}

// Send this display buffer data to remote display... once it asks for it.
// Just copy the data at the time of call.
void ChipInterfaceRaSCSI::displayBuffer(uint8_t *bfr, uint16_t size)
{
    uint16_t copySize = MIN(DISPLAY_DATA_SIZE - 4, size);       // pick smaller and don't overflow

    Utils::storeWord(displayData, 0xd1da);          // starting tag - DIsplay DAta
    Utils::storeWord(displayData + 2, copySize);    // how much data is there

    displayDataSize = copySize + 4;                 // how much data should be transfered
    memcpy(displayData + 4, bfr, copySize);         // copy in the data
}

// Get on which GPIO pins the i2c display is. Pins valid for ChipInterface v4.
void ChipInterfaceRaSCSI::getDisplayGpioSignals(uint32_t& gpioScl, uint32_t& gpioSda)
{
#ifndef ONPC
    // TODO: figure this out from PCB of RaSCSI
    gpioScl = RPI_V2_GPIO_P1_05;
    gpioSda = RPI_V2_GPIO_P1_13;
#else
    gpioScl = 0;
    gpioSda = 0;
#endif
}
