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
#include "../display/ssd1306.h"

#ifndef ONPC
    #include <bcm2835.h>
#endif

extern THwConfig hwConfig;
extern TFlags    flags;                 // global flags from command line

bool chipLogRun = false;

int chipDebugStringsFd = -1;
void chipDebugStringsSetup(void);       // open UART for debug strings from HW
void chipDebugStringsHandle(void);      // read from UART, write to chiplog
void handlePthreadCreate(const char* threadName, pthread_t* pThreadInfo, void* threadCode);
void pthread_kill_join(const char* threadName, pthread_t& threadInfo);

void chipDebugStringsSetup(void)
{
    char msg[128];
    Debug::chipLog("\n\nchipDebugStringsSetup starting\n");

    struct termios termiosStruct;
    termios *ts = &termiosStruct;

    std::string uartPath = Utils::dotEnvValue("IKBD_SERIAL_PORT");
    int fd = open(uartPath.c_str(), O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);

    if(fd == -1) {
        sprintf(msg, "Failed to open UART: '%s' for chiplog!\n", uartPath.c_str());
        Debug::chipLog(msg);
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
        Debug::chipLog("Failed to set serial port attributes.\n");
    }

    fcntl(fd, F_SETFL, FNDELAY);                    // make reading non-blocking
    chipDebugStringsFd = fd;

    sprintf(msg, "opened UART '%s' for chiplog\n", uartPath.c_str());
    Debug::chipLog(msg);
}

void chipDebugStringsHandle(void)
{
    if(chipDebugStringsFd <= 0) {       // no fd? quit
        return;
    }

    // how many bytes we can read immediately?
    int res, bytesAvailable;
    res = ioctl(chipDebugStringsFd, FIONREAD, &bytesAvailable);

    if(res < 0) {       // ioctl() failed? no bytes available
        return;
    }

    // prepare buffer, determine max read size
    char bfr[1024];
    memset(bfr, 0, sizeof(bfr));
    int readCount = MIN(bytesAvailable, sizeof(bfr));

    // read from fd, write to log
    ssize_t rCount = read(chipDebugStringsFd, bfr, readCount);
    Debug::chipLog(rCount, bfr);
}

void *chiplogThreadCode(void *ptr)
{
    chipDebugStringsSetup();

    while(chipLogRun && sigintReceived == 0) {
        chipDebugStringsHandle();
        Utils::sleepMs(330);            // TODO: remake into SELECT instead of sleep
    }

    Utils::closeFdIfOpen(chipDebugStringsFd);
    return NULL;
}

ChipInterface3::ChipInterface3()
{
    conSpi2 = new CConSpi2();

    pipeFromAtariToRPi[0] = pipeFromAtariToRPi[1] = -1;
    pipeFromRPiToAtari[0] = pipeFromRPiToAtari[1] = -1;

    rxDataWOhead = new uint8_t[MFM_STREAM_SIZE];

    memset(&hansConfigWords, 0, sizeof(hansConfigWords));
}

ChipInterface3::~ChipInterface3()
{
    delete conSpi2;

    delete []rxDataWOhead;
}

int ChipInterface3::chipInterfaceType(void)
{
    return CHIP_IF_V3;
}

bool ChipInterface3::ciOpen(void)
{
    chipLogRun = true;
    handlePthreadCreate("chiplog", &chiplogThreadInfo, (void*) chiplogThreadCode);

    serialSetup();          // open and configure pipes for IKBD
    return gpio_open();     // open GPIO and SPI
}

void ChipInterface3::ciClose(void)
{
    if(chipLogRun) {
        chipLogRun = false;
        pthread_kill_join("chiplog", chiplogThreadInfo);
    }

    Utils::closeFdIfOpen(pipeFromAtariToRPi[0]);
    Utils::closeFdIfOpen(pipeFromAtariToRPi[1]);

    Utils::closeFdIfOpen(pipeFromRPiToAtari[0]);
    Utils::closeFdIfOpen(pipeFromRPiToAtari[1]);

    gpio_close();           // close GPIO and SPI
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

    conSpi2->setIkbdWriteFd(pipeFromAtariToRPi[1]);     // ConSPI2 get Atari data, writes them to pipe 1, IKBD thread gets them
}

void ChipInterface3::resetHDDandFDD(void)
{
    resetHDD();     // just use reset HDD as reset FDD in v3 does nothing
}

void ChipInterface3::resetHDD(void)
{
#ifndef ONPC
    bcm2835_gpio_write(PIN_RESET_HANS, LOW);        // reset lines to RESET state
    Utils::sleepMs(10);                             // wait a while to let the reset work
    bcm2835_gpio_write(PIN_RESET_HANS, HIGH);       // reset lines to RUN (not reset) state
    Utils::sleepMs(50);                             // wait a while to let the devices boot
#endif
}

void ChipInterface3::resetFDD(void)
{
    // nothing to do on v3
}

bool ChipInterface3::actionNeeded(bool &hardNotFloppy, uint8_t *inBuf)
{
    SpiRxPacket* rxPacket;

    // periodically read IKBD data from pipeFromRPiToAtari[0] and send to CE
    static uint32_t lastIkbdSend = 0;
    uint32_t now = Utils::getCurrentMs();
    uint32_t diff = now - lastIkbdSend;

    if(diff > 50 && pipeFromRPiToAtari[0] > -1) {   // if last ikbd send was at least this interval ago, send now
        lastIkbdSend = now;                     // we're sending the data now, next after interval period

        int bytesAvailable;
        int rv = ioctl(pipeFromRPiToAtari[0], FIONREAD, &bytesAvailable);    // how many bytes we can read?

        if(rv == 0 && bytesAvailable > 0) {     // ioctl was OK and we got something to read
            uint8_t bfr[64];
            int readSize = MIN(64, bytesAvailable);             // limit read to buffer size
            rv = read(pipeFromRPiToAtari[0], bfr, readSize);    // do the read

            if(rv > 0) {                        // something was read? send it
                conSpi2->addToTxQueue(MARKER_IKBD, CMD_IKBD_DATA, bytesAvailable, bfr);
            }
        }
    }

    // check for any ATN code waiting from HDD or FDD
    rxPacket = conSpi2->waitForATN(WAIT_FOR_HDD_OR_FDD, ATN_ANY, 0);

    if(!rxPacket) {         // no action needed if no rx packet present
        return false;
    }

    hardNotFloppy = rxPacket->isHdd();  // store is-hdd flag

    if(rxPacket->txLen() > INBUF_SIZE) {
        Debug::out(LOG_ERROR, "ChipInterface3::actionNeeded -- rxPacket->txLen(): %d > INBUF_SIZE: %d", rxPacket->txLen(), INBUF_SIZE);
    }
    int copyCount = MIN(rxPacket->txLen(), INBUF_SIZE);
    memcpy(inBuf, rxPacket->getBaseDataPointer(), copyCount);   // copy the whole received packet to inBuf

#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "ChipInterface3::actionNeeded -- hardNotFloppy: %d, packet length: %d", (int) hardNotFloppy, rxPacket->txLen());
#endif
    // Debug::outBfr(rxPacket->getBaseDataPointer(), rxPacket->txLen());

    // for these cases we also need a copy of just data (without header) into our local receive buffer
    if((rxPacket->isFdd() && rxPacket->atnCode() == ATN_SECTOR_WRITTEN) ||  // for FDD     + ATN_SECTOR_WRITTEN
       (                     rxPacket->atnCode() == ATN_FW_VERSION)) {      // for FDD/HDD + ATN_FW_VERSION
        if(rxPacket->txLen() > INBUF_SIZE) {
            Debug::out(LOG_ERROR, "ChipInterface3::actionNeeded -- rxPacket->getDataSize(): %d > MFM_STREAM_SIZE: %d", rxPacket->getDataSize(), MFM_STREAM_SIZE);
        }
        copyCount = MIN(rxPacket->getDataSize(), MFM_STREAM_SIZE);
        memcpy(rxDataWOhead, rxPacket->getDataPointer(), copyCount);
#ifdef DEBUG_SPI_COMMUNICATION
        Debug::out(LOG_DEBUG, "ChipInterface3::actionNeeded -- additionally copied to rxDataWOhead: %d", rxPacket->getDataSize());
        Debug::outBfr(rxDataWOhead, rxPacket->getDataSize());
#endif
    }

    delete rxPacket;                    // free the RX packet from memory
    return true;                        // action needed
}

void ChipInterface3::getFWversion(bool hardNotFloppy, uint8_t *inFwVer)
{
    if(hardNotFloppy) {     // for HDD
        // fwResponseBfr should be filled with Hans config - by calling setHDDconfig() (and not calling anything else in between)

        // send response to HDD + ATN_FW_VERSION later
        conSpi2->addToTxQueue(MARKER_HDD, ATN_FW_VERSION, HDD_FW_RESPONSE_LEN, fwResponseBfr);
        memcpy(inFwVer, rxDataWOhead, HDD_FW_RESPONSE_LEN);    // copy the received data without header into the supplied buffer

        ChipInterface::convertXilinxInfo(rxDataWOhead[5]);     // convert xilinx info into hwInfo struct

        hansConfigWords.current.acsi = MAKEWORD(rxDataWOhead[6], rxDataWOhead[7]);
        hansConfigWords.current.fdd  = MAKEWORD(rxDataWOhead[8], 0);

        int year = Utils::bcdToInt(rxDataWOhead[1]) + 2000;
        Update::versions.hans.fromInts(year, Utils::bcdToInt(rxDataWOhead[2]), Utils::bcdToInt(rxDataWOhead[3]));     // store found FW version of Hans
    } else {                // for FDD
        // fwResponseBfr should be filled with Franz config - by calling setFDDconfig() (and not calling anything else in between)

        // send response to FDD + ATN_FW_VERSION later
        conSpi2->addToTxQueue(MARKER_FDD, ATN_FW_VERSION, FDD_FW_RESPONSE_LEN, fwResponseBfr);
        memcpy(inFwVer, rxDataWOhead, HDD_FW_RESPONSE_LEN);    // copy the received data without header into the supplied buffer

        int year = Utils::bcdToInt(rxDataWOhead[1]) + 2000;
        Update::versions.franz.fromInts(year, Utils::bcdToInt(rxDataWOhead[2]), Utils::bcdToInt(rxDataWOhead[3]));    // store found FW version of Franz
    }
}

bool ChipInterface3::hdd_sendData_start(uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "ChipInterface3::hdd_sendData_start -- trying to send more than 16 MB, fail");
        return false;
    }

    uint8_t bfr[6];
    bfr[0] = withStatus ? CMD_DATA_READ_WITH_STATUS : CMD_DATA_READ_WITHOUT_STATUS;
    Utils::store24bits(bfr + 1, totalDataCount);    // store data size at 1,2,3
    bfr[4] = scsiStatus;                 // store status

#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "ChipInterface3::hdd_sendData_start - next state: %02x, dataCnt: %d, status: %02x", bfr[0], totalDataCount, scsiStatus);
    Debug::outBfr(bfr, 5);
#endif

    conSpi2->addToTxQueue(MARKER_HDD, ATN_ACSI_COMMAND, 5, bfr);
    conSpi2->transferNow();             // transfer the last TX queue item added above

    return true;
}

bool ChipInterface3::hdd_sendData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
    while(dataCount > 0) {                  // while there's something to send
        SpiRxPacket* rxPacket = conSpi2->waitForATN(WAIT_FOR_HDD, ATN_READ_MORE_DATA, 1000);    // wait for ATN_READ_MORE_DATA

        if(!rxPacket) {                     // this didn't come? fail
            Debug::out(LOG_ERROR, "ChipInterface3::hdd_sendData_transferBlock - failed to wait for WAIT_FOR_HDD + ATN_READ_MORE_DATA");
            return false;
        }

        delete rxPacket;                    // free the RX packet from memory

        uint32_t cntNow = MIN(dataCount, 512);      // max 512 bytes per transfer
        conSpi2->addToTxQueue(MARKER_HDD, ATN_READ_MORE_DATA, cntNow, pData);
        pData       += cntNow;                      // move the data pointer further
        dataCount   -= cntNow;                      // subtract the amount we've sent

#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "ChipInterface3::hdd_sendData_transferBlock - dataCnt: %d", cntNow);
#endif
    }

    conSpi2->transferNow();                         // transfer the last TX queue item added above
    return true;
}

bool ChipInterface3::hdd_recvData_start(uint8_t *recvBuffer, uint32_t totalDataCount)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "ChipInterface3::hdd_recvData_start() -- trying to send more than 16 MB, fail");
        return false;
    }

    // first send the command and tell Hans that we need WRITE data
    uint8_t bfr[6];
    bfr[0] = CMD_DATA_WRITE;
    Utils::store24bits(bfr + 1, totalDataCount);    // store data size at 1,2,3
    bfr[4] = 0xff;                                  // store INVALID status, because the real status will be sent on CMD_SEND_STATUS

#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "ChipInterface3::hdd_recvData_start - next state: %02x, dataCnt: %d", bfr[0], totalDataCount);
    Debug::outBfr(bfr, 5);
#endif

    conSpi2->addToTxQueue(MARKER_HDD, ATN_ACSI_COMMAND, 5, bfr);
    conSpi2->transferNow();                         // transfer the last TX queue item added above

    return true;
}

bool ChipInterface3::hdd_recvData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
    while(dataCount > 0) {
        SpiRxPacket* rxPacket = conSpi2->waitForATN(WAIT_FOR_HDD, ATN_WRITE_MORE_DATA, 1000);   // wait for ATN_WRITE_MORE_DATA

        if(!rxPacket) {                                     // this didn't come? fail
            Debug::out(LOG_ERROR, "ChipInterface3::hdd_recvData_transferBlock - failed to wait for WAIT_FOR_HDD + ATN_WRITE_MORE_DATA");
            return false;
        }

        uint32_t subCount = rxPacket->getDataSize();            // use all the data we have received
        if(subCount > dataCount) {
            Debug::out(LOG_ERROR, "ChipInterface3::hdd_recvData_transferBlock -- subCount: %d > dataCount: %d", subCount, dataCount);
        }
        subCount = MIN(subCount, dataCount);

        memcpy(pData, rxPacket->getDataPointer(), subCount);    // copy just the data, skip sequence number
        delete rxPacket;                                        // free the RX packet from memory

#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "ChipInterface3::hdd_recvData_transferBlock - dataCnt: %d", subCount);
#endif

        dataCount -= subCount;      // decrement the data counter
        pData += subCount;          // move in the buffer further
    }

    return true;
}

bool ChipInterface3::hdd_sendStatusToHans(uint8_t statusByte)
{
    SpiRxPacket* rxPacket = conSpi2->waitForATN(WAIT_FOR_HDD, ATN_GET_STATUS, 1000);    // wait for ATN_GET_STATUS

    if(!rxPacket) {
        Debug::out(LOG_ERROR, "ChipInterface3::hdd_sendStatusToHans - failed to wait for WAIT_FOR_HDD + ATN_GET_STATUS");
        return false;
    }

    delete rxPacket;                 // free the RX packet from memory

    uint8_t bfr[2];
    bfr[0] = statusByte;             // single data byte to send
    bfr[1] = 0;

#ifdef DEBUG_SPI_COMMUNICATION
    Debug::out(LOG_DEBUG, "ChipInterface3::hdd_sendStatusToHans - statusByte: %02x", statusByte);
    Debug::outBfr(bfr, 2);
#endif

    conSpi2->addToTxQueue(MARKER_HDD, ATN_GET_STATUS, 1, bfr);
    conSpi2->transferNow();         // transfer the last TX queue item added above
    return true;
}

void ChipInterface3::fdd_sendTrackToChip(int byteCount, uint8_t *encodedTrack)
{
    // send encoded track out, use ATN value as CMD
    conSpi2->addToTxQueue(MARKER_FDD, ATN_SEND_TRACK, byteCount, encodedTrack);
    conSpi2->transferNow();      // transfer the last TX queue item added above
}

uint8_t* ChipInterface3::fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount)
{
    // get the written sector, side, track number - the rxDataWOhead has copy of the data part of packet since call to actionNeeded()
    sector  = rxDataWOhead[1];
    track   = rxDataWOhead[0] & 0x7f;
    side    = (rxDataWOhead[0] & 0x80) ? 1 : 0;

    return rxDataWOhead;            // return pointer to received written sector
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
    conSpi2->addToTxQueue(MARKER_DISP, CMD_DISPLAY, size, bfr);
}
