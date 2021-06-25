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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../main_netserver.h"
#include "../chipinterface_v1_v2/chipinterface12.h"
#include "chipinterfacenetwork.h"
#include "../utils.h"
#include "../debug.h"
#include "../global.h"
#include "../update.h"
#include "../ikbd/ikbd.h"

extern THwConfig hwConfig;
extern TFlags    flags;                 // global flags from command line

ChipInterfaceNetwork::ChipInterfaceNetwork()
{
    nextReportTime = 0;
    serverIndex = 0;

    fdListen = -1;
    fdClient = -1;
    fdReport = -1;

    ikbdReadFd = -1;
    ikbdWriteFd = -1;

    bufOut = new BYTE[MFM_STREAM_SIZE];
    bufIn = new BYTE[MFM_STREAM_SIZE];

    memset(&hansConfigWords, 0, sizeof(hansConfigWords));
}

ChipInterfaceNetwork::~ChipInterfaceNetwork()
{
    delete []bufOut;
    delete []bufIn;
}

int ChipInterfaceNetwork::chipInterfaceType(void)
{
    return CHIP_IF_NETWORK;
}

void ChipInterfaceNetwork::setServerIndex(int index)
{
    serverIndex = index;
}

void ChipInterfaceNetwork::createListeningSocket(void)
{
    if(fdListen >= 0) { // if the listening socket seems to be already created, just quit
        return;
    }

    // open socket
    if ((fdListen = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        Debug::out(LOG_ERROR, "netServer %d - failed to open socket", serverIndex);
        return;
    }

    // Forcefully attach socket to the port
    int opt = 1;

    if (setsockopt(fdListen, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        Debug::out(LOG_ERROR, "netServer %d - setsockopt() failed", serverIndex);
        return;
    }

    // change the socket into non-blocking
    fcntl(fdListen, F_SETFL, O_NONBLOCK);

    addressListen.sin_family = AF_INET;
    addressListen.sin_addr.s_addr = INADDR_ANY;
    addressListen.sin_port = htons( SERVER_TCP_PORT_FIRST + serverIndex );

    // bind to address
    if (bind(fdListen, (struct sockaddr *) &addressListen, sizeof(addressListen)) < 0) {
        Debug::out(LOG_ERROR, "netServer %d - bind() failed", serverIndex);
        return;
    }

    // mark the socket as a passive socket
    if (listen(fdListen, 1) < 0) {
        Debug::out(LOG_ERROR, "netServer %d - listen() failed", serverIndex);
        return;
    }
}

void ChipInterfaceNetwork::acceptSocketIfNeededAndPossible(void)
{
    // if already got client socket, no need to do anything here
    if(fdClient >= 0) {
        return;
    }

    // don't have client socket, try accept()
    socklen_t addrSize = sizeof(addressListen);

    int newSock = accept(fdListen, (struct sockaddr *) &addressListen, &addrSize);

    if(newSock < 0) {       // nothing to accept, would block? quit
        return;
    }

    // got the new client socket now
    fdClient = newSock;
}

void ChipInterfaceNetwork::createServerReportSocket(void)
{
    fdReport = socket(AF_INET, SOCK_DGRAM, 0);

    if(fdReport < 0) {        // on error
        Debug::out(LOG_ERROR, "failed to open socket for main server reporting");
        return;
    }

    bzero(&addressReport, sizeof(addressReport));

    addressReport.sin_family = AF_INET;
    addressReport.sin_addr.s_addr = inet_addr("127.0.0.1");
    addressReport.sin_port = htons(SERVER_UDP_PORT);
}

void ChipInterfaceNetwork::sendReportToMainServerSocket(void)
{
    if(fdReport < 0) {              // no report socket? try to create one
        createServerReportSocket();
    }

    if(fdReport < 0) {              // still no report socket? quit
        return;
    }

    uint8_t data[8];
    memset(data, 0, sizeof(data));      // clear data buffer
    memcpy(data, "CELS", 4);            // 0..3: message tag

    data[4] = serverIndex;              // 4: this server's index

    uint8_t status = (fdClient > 0) ? SERVER_STATUS_OCCUPIED : SERVER_STATUS_FREE;  // got client socket? we're occupied, otherwise free
    data[5] = status;                   // 5: status

    // send report to main server report port
    sendto(fdReport, data, sizeof(data), 0, (sockaddr*) &addressReport, sizeof(addressReport));
}

bool ChipInterfaceNetwork::ciOpen(void)
{
    createServerReportSocket();
    createListeningSocket();

    serialSetup();

    return true;
}

void ChipInterfaceNetwork::closeFdIfOpen(int& sock)
{
    if(sock != -1) {    // got the sock?
        close(sock);    // close it
        sock = -1;      // set it to invalid value
    }
}

void ChipInterfaceNetwork::ciClose(void)
{
    // close sockets
    closeFdIfOpen(fdListen);
    closeFdIfOpen(fdClient);
    closeFdIfOpen(fdReport);

    // close IKDB pipes
    closeFdIfOpen(ikbdReadFd);
    closeFdIfOpen(ikbdWriteFd);
}

void ChipInterfaceNetwork::ikbdUartEnable(bool enable)
{
    // nothing needed to be done here
}

int ChipInterfaceNetwork::ikbdUartReadFd(void)
{
    return ikbdReadFd;
}

int ChipInterfaceNetwork::ikbdUartWriteFd(void)
{
    return ikbdWriteFd;
}

void ChipInterfaceNetwork::serialSetup(void)
{
    // create pipes for communication with ikbd thread
    int fd = -1;


    // on real UART same fd is used for read and write
    ikbdReadFd = fd;
    ikbdWriteFd = fd;
}

void ChipInterfaceNetwork::resetHDDandFDD(void)
{
    // can't reset the chip via network
}

void ChipInterfaceNetwork::resetHDD(void)
{
    // can't reset the chip via network
}

void ChipInterfaceNetwork::resetFDD(void)
{
    // can't reset the chip via network
}

bool ChipInterfaceNetwork::actionNeeded(bool &hardNotFloppy, BYTE *inBuf)
{
    // send current status report every once in a while
    if(Utils::getCurrentMs() >= nextReportTime) {
        nextReportTime = Utils::getEndTime(1000);
        sendReportToMainServerSocket();
    }

    if(fdClient <= 0) {     // no client connected? no action needed
        return false;
    }

    // if waitForATN() succeeds, it fills 8 bytes of data in buffer
    // ...but then we might need some little more, so let's determine what it was
    // and keep reading as much as needed
    int moreData = 0;

    // check for any ATN code waiting from Hans
    // TODO:
    //bool res = conSpi->waitForATN(SPI_CS_HANS, (BYTE) ATN_ANY, 0, inBuf);
    bool res = false;

    if(res) {    // HANS is signaling attention?
        if(inBuf[3] == ATN_ACSI_COMMAND) {
            moreData = 14;                              // all ACSI command bytes
        }

        if(moreData) {
            // TODO:
            //conSpi->txRx(SPI_CS_HANS, moreData, bufOut, inBuf + 8);   // get more data, offset in input buffer by header size
        }

        hardNotFloppy = true;
        return true;
    }

    // check for any ATN code waiting from Franz
    // TODO:
    //res = conSpi->waitForATN(SPI_CS_FRANZ, (BYTE) ATN_ANY, 0, inBuf);

    if(res) {    // FRANZ is signaling attention?
        if(inBuf[3] == ATN_SEND_TRACK) {
            moreData = 2;                               // side + track
        }

        if(moreData) {
            // TODO:
            //conSpi->txRx(SPI_CS_FRANZ, moreData, bufOut, inBuf + 8);   // get more data, offset in input buffer by header size
        }

        hardNotFloppy = false;
        return true;
    }

    // no action needed
    return false;
}

void ChipInterfaceNetwork::getFWversion(bool hardNotFloppy, BYTE *inFwVer)
{
    if(hardNotFloppy) {     // for HDD
        // fwResponseBfr should be filled with Hans config - by calling setHDDconfig() (and not calling anything else inbetween)
        // TODO:
        //conSpi->txRx(SPI_CS_HANS, HDD_FW_RESPONSE_LEN, fwResponseBfr, inFwVer);

        ChipInterface::convertXilinxInfo(inFwVer[5]);  // convert xilinx info into hwInfo struct

        hansConfigWords.current.acsi = MAKEWORD(inFwVer[6], inFwVer[7]);
        hansConfigWords.current.fdd  = MAKEWORD(inFwVer[8],        0);

        int year = Utils::bcdToInt(inFwVer[1]) + 2000;
        Update::versions.hans.fromInts(year, Utils::bcdToInt(inFwVer[2]), Utils::bcdToInt(inFwVer[3]));       // store found FW version of Hans
    } else {                // for FDD
        // fwResponseBfr should be filled with Franz config - by calling setFDDconfig() (and not calling anything else inbetween)

        // TODO:
        // conSpi->txRx(SPI_CS_FRANZ, FDD_FW_RESPONSE_LEN, fwResponseBfr, inFwVer);

        int year = Utils::bcdToInt(inFwVer[1]) + 2000;
        Update::versions.franz.fromInts(year, Utils::bcdToInt(inFwVer[2]), Utils::bcdToInt(inFwVer[3]));              // store found FW version of Franz
    }
}

bool ChipInterfaceNetwork::hdd_sendData_start(DWORD totalDataCount, BYTE scsiStatus, bool withStatus)
{
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

    // TODO:
    //conSpi->txRx(SPI_CS_HANS, COMMAND_SIZE, bufOut, bufIn);        // transmit this command

    return true;
}

bool ChipInterfaceNetwork::hdd_sendData_transferBlock(BYTE *pData, DWORD dataCount)
{
    bufOut[0] = 0;
    bufOut[1] = CMD_DATA_MARKER;                                  // mark the start of data

    if((dataCount & 1) != 0) {                                      // odd number of bytes? make it even, we're sending words...
        dataCount++;
    }

    while(dataCount > 0) {                                          // while there's something to send
        // TODO:
        // bool res = conSpi->waitForATN(SPI_CS_HANS, ATN_READ_MORE_DATA, 1000, bufIn);   // wait for ATN_READ_MORE_DATA
        bool res = true;

        if(!res) {                                                  // this didn't come? fuck!
            return false;
        }

        DWORD cntNow = (dataCount > 512) ? 512 : dataCount;         // max 512 bytes per transfer

        memcpy(bufOut + 2, pData, cntNow);                          // copy the data after the header (2 bytes)

        // TODO:
        // conSpi->txRx(SPI_CS_HANS, cntNow + 4, bufOut, bufIn);          // transmit this buffer with header + terminating zero (WORD)

        pData       += cntNow;                                      // move the data pointer further
        dataCount   -= cntNow;
    }

    return true;
}

bool ChipInterfaceNetwork::hdd_recvData_start(BYTE *recvBuffer, DWORD totalDataCount)
{
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

    // TODO:
    // conSpi->txRx(SPI_CS_HANS, COMMAND_SIZE, bufOut, bufIn);        // transmit this command
    return true;
}

bool ChipInterfaceNetwork::hdd_recvData_transferBlock(BYTE *pData, DWORD dataCount)
{
    memset(bufOut, 0, TX_RX_BUFF_SIZE);                   // nothing to transmit, really...
    BYTE inBuf[8];

    while(dataCount > 0) {
        // request maximum 512 bytes from host
        DWORD subCount = (dataCount > 512) ? 512 : dataCount;

        // TODO:
        //bool res = conSpi->waitForATN(SPI_CS_HANS, ATN_WRITE_MORE_DATA, 1000, inBuf); // wait for ATN_WRITE_MORE_DATA
        bool res = true;

        if(!res) {                                          // this didn't come? fuck!
            return false;
        }

        // TODO:
        // conSpi->txRx(SPI_CS_HANS, subCount + 8 - 4, bufOut, bufIn);    // transmit data (size = subCount) + header and footer (size = 8) - already received 4 bytes
        memcpy(pData, bufIn + 2, subCount);                 // copy just the data, skip sequence number

        dataCount   -= subCount;                            // decreate the data counter
        pData       += subCount;                            // move in the buffer further
    }

    return true;
}

bool ChipInterfaceNetwork::hdd_sendStatusToHans(BYTE statusByte)
{
    // TODO:
    //bool res = conSpi->waitForATN(SPI_CS_HANS, ATN_GET_STATUS, 1000, bufIn);   // wait for ATN_GET_STATUS
    bool res = true;

    if(!res) {
        return false;
    }

    memset(bufOut, 0, 16);                                // clear the tx buffer
    bufOut[1] = CMD_SEND_STATUS;                          // set the command and the statusByte
    bufOut[2] = statusByte;

    // TODO:
    //conSpi->txRx(SPI_CS_HANS, 16 - 8, bufOut, bufIn);        // transmit the statusByte (16 bytes total, but 8 already received)

    return true;
}

void ChipInterfaceNetwork::fdd_sendTrackToChip(int byteCount, BYTE *encodedTrack)
{
    // send encoded track out, read garbage into bufIn and don't care about it
    // TODO:
    //conSpi->txRx(SPI_CS_FRANZ, byteCount, encodedTrack, bufIn);
}

BYTE* ChipInterfaceNetwork::fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount)
{
    // TODO:
    //byteCount = conSpi->getRemainingLength();               // get how many data we still have

    memset(bufOut, 0, byteCount);                           // clear the output buffer before sending it to Franz (just in case)

    // TODO:
    //conSpi->txRx(SPI_CS_FRANZ, byteCount, bufOut, bufIn);   // get all the remaining data

    // get the written sector, side, track number
    sector  = bufIn[1];
    track   = bufIn[0] & 0x7f;
    side    = (bufIn[0] & 0x80) ? 1 : 0;

    return bufIn;                                           // return pointer to received written sector
}
