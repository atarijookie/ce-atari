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
#include <sys/socket.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../main_netserver.h"
#include "../chipinterface_v1_v2/chipinterface12.h"
#include "chipinterfacedummy.h"
#include "../utils.h"
#include "../debug.h"
#include "../global.h"
#include "../update.h"
#include "../ikbd/ikbd.h"

extern THwConfig hwConfig;
extern TFlags    flags;                 // global flags from command line

ChipInterfaceDummy::ChipInterfaceDummy()
{

}

ChipInterfaceDummy::~ChipInterfaceDummy()
{

}

int ChipInterfaceDummy::chipInterfaceType(void)
{
    return CHIP_IF_DUMMY;
}

bool ChipInterfaceDummy::ciOpen(void)
{
    return true;
}

void ChipInterfaceDummy::ciClose(void)
{

}

void ChipInterfaceDummy::ikbdUartEnable(bool enable)
{
    // nothing needed to be done here
}

int ChipInterfaceDummy::ikbdUartReadFd(void)
{
    return -1;
}

int ChipInterfaceDummy::ikbdUartWriteFd(void)
{
    return -1;
}

void ChipInterfaceDummy::resetHDDandFDD(void)
{
    // can't reset the chip via network
}

void ChipInterfaceDummy::resetHDD(void)
{
    // can't reset the chip via network
}

void ChipInterfaceDummy::resetFDD(void)
{
    // can't reset the chip via network
}

bool ChipInterfaceDummy::actionNeeded(bool &hardNotFloppy, uint8_t *inBuf)
{
    static uint32_t nextHddFwTime = 0;
    static uint32_t nextFddFwTime = 0;

    if(Utils::getCurrentMs() >= nextHddFwTime) {
        inBuf[3] = ATN_FW_VERSION;
        nextHddFwTime = Utils::getEndTime(1000);
        return true;
    }

    if(Utils::getCurrentMs() >= nextFddFwTime) {
        inBuf[3] = ATN_FW_VERSION;
        nextFddFwTime = Utils::getEndTime(1000);
        return true;
    }

    // no action needed
    return false;
}

void ChipInterfaceDummy::getFWversion(bool hardNotFloppy, uint8_t *inFwVer)
{
    // Debug::out(LOG_DEBUG, "getFWversion(): hardNotFloppy=%d", hardNotFloppy);

    if(hardNotFloppy) {     // for HDD
        ChipInterface::convertXilinxInfo(0x11);  // convert xilinx info into hwInfo struct

        hansConfigWords.current.acsi = 0;
        hansConfigWords.current.fdd  = 0;

        Update::versions.hans.fromInts(2023, 1, 1);     // store found FW version of Hans
    } else {                // for FDD
        Update::versions.franz.fromInts(2023, 1, 1);    // store found FW version of Franz
    }
}

bool ChipInterfaceDummy::hdd_sendData_start(uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus)
{
    return false;
}

bool ChipInterfaceDummy::hdd_sendData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
    return false;
}

bool ChipInterfaceDummy::hdd_recvData_start(uint8_t *recvBuffer, uint32_t totalDataCount)
{
    return false;
}

bool ChipInterfaceDummy::hdd_recvData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
    return false;
}

bool ChipInterfaceDummy::hdd_sendStatusToHans(uint8_t statusByte)
{
    return true;
}

void ChipInterfaceDummy::fdd_sendTrackToChip(int byteCount, uint8_t *encodedTrack)
{

}

uint8_t* ChipInterfaceDummy::fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount)
{
    return NULL;
}

void ChipInterfaceDummy::handleButton(int& btnDownTime, uint32_t& nextScreenTime)
{

}

void ChipInterfaceDummy::handleBeeperCommand(int beeperCommand, bool floppySoundEnabled)
{

}

// returns true if should handle i2c display from RPi
bool ChipInterfaceDummy::handlesDisplay(void)
{
    return false;        // dummy interface doesn't handle display locally
}

// send this display buffer data to remote display
void ChipInterfaceDummy::displayBuffer(uint8_t *bfr, uint16_t size)
{
    // nothing to do here, this is a dummy
}
