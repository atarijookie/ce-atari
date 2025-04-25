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
#include <sys/ioctl.h>

#include "chipinterfacenetwork.h"
#include "../utils.h"
#include "../debug.h"
#include "../global.h"
#include "../update.h"

extern THwConfig hwConfig;
extern TFlags    flags;                 // global flags from command line

#define SERVER_STATUS_NOT_RUNNING   0       // when this server slot is not used yet and server is not running
#define SERVER_STATUS_FREE          1       // server is running but no client is connected there
#define SERVER_STATUS_OCCUPIED      2       // server is running and client is connected

ChipInterfaceNetwork::ChipInterfaceNetwork()
{
    nextReportTime = 0;

    fdListen = -1;
    fdClient = -1;
    fdReport = -1;

    bufOut = new uint8_t[MFM_STREAM_SIZE];
    bufIn = new uint8_t[MFM_STREAM_SIZE];

    gotAtnId = 0;
    gotAtnCode = 0;

    lastTimeRecv = Utils::getCurrentMs();
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

void ChipInterfaceNetwork::createListeningSocket(void)
{
    if(fdListen >= 0) { // if the listening socket seems to be already created, just quit
        return;
    }

    // open socket
    if ((fdListen = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        Debug::out(LOG_ERROR, "netServer - failed to open socket");
        return;
    }

    // Forcefully attach socket to the port
    int opt = 1;

    if (setsockopt(fdListen, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        Debug::out(LOG_ERROR, "netServer - setsockopt() failed");
        return;
    }

    // change the socket into non-blocking
    fcntl(fdListen, F_SETFL, O_NONBLOCK);

    addressListen.sin_family = AF_INET;
    addressListen.sin_addr.s_addr = INADDR_ANY;
    addressListen.sin_port = htons( flags.portClient );

    // bind to address
    if (bind(fdListen, (struct sockaddr *) &addressListen, sizeof(addressListen)) < 0) {
        Debug::out(LOG_ERROR, "netServer - bind() failed");
        return;
    }

    // mark the socket as a passive socket
    if (listen(fdListen, 1) < 0) {
        Debug::out(LOG_ERROR, "netServer - listen() failed");
        return;
    }

    Debug::out(LOG_INFO, "netServer - listening on tcp port: %d", flags.portClient);
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

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;    // 500 ms timeout on blocking reads
    setsockopt(newSock, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv));

    // got the new client socket now
    fdClient = newSock;
    bufReader.setFd(newSock);

    Debug::out(LOG_DEBUG, "acceptSocketIfNeededAndPossible() - client connected");

    sendReportToMainServerSocket();     // let main server process know that we're occupied now
}

void ChipInterfaceNetwork::closeClientSocket(void)
{
    Utils::closeFdIfOpen(fdClient);            // close socket
    sendReportToMainServerSocket();     // let main server process know that we're free now

    Debug::out(LOG_DEBUG, "closeClientSocket() - client disconnected");
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
    addressReport.sin_port = htons(flags.portServerReport);
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

    data[4] = (uint8_t) (flags.portClient >> 8);     // 4..5: this server's port
    data[5] = (uint8_t) (flags.portClient     );

    uint8_t status = (fdClient > 0) ? SERVER_STATUS_OCCUPIED : SERVER_STATUS_FREE;  // got client socket? we're occupied, otherwise free
    data[6  ] = status;                   // 5: status

    // send report to main server report port
    sendto(fdReport, data, sizeof(data), 0, (sockaddr*) &addressReport, sizeof(addressReport));
}

bool ChipInterfaceNetwork::ciOpen(void)
{
    createServerReportSocket();
    createListeningSocket();

    return true;
}

void ChipInterfaceNetwork::ciClose(void)
{
    // close sockets
    Utils::closeFdIfOpen(fdListen);
    Utils::closeFdIfOpen(fdClient);
    Utils::closeFdIfOpen(fdReport);
}

bool ChipInterfaceNetwork::actionNeeded(uint8_t *inBuf)
{
    // send current status report every once in a while (could do it on change only, but doing it repeatedly as UDP packet might get lost, even on localhost only)
    if(Utils::getCurrentMs() >= nextReportTime) {
        nextReportTime = Utils::getEndTime(5000);
        sendReportToMainServerSocket();
    }

    acceptSocketIfNeededAndPossible();  // if don't have client connected, try to accept connection from client

    if(fdClient <= 0) {                 // (still) no client connected? no action needed
        //Debug::out(LOG_DEBUG, "actionNeeded() - client not connected yet");
        return false;
    }

    int bytesAvailable;
    int rv = ioctl(fdClient, FIONREAD, &bytesAvailable);    // how many bytes we can read?

    if(rv < 0 || bytesAvailable <= 0) {                     // ioctl fail or nothing to read? no action needed
        // Debug::out(LOG_DEBUG, "actionNeeded() - nothing comming from fdClient");

        if(rv == 0 && bytesAvailable == 0) {    // ioctl() succeeded, but can't read anything
            uint32_t now = Utils::getCurrentMs();
            uint32_t diff = now - lastTimeRecv;

            if(diff > 3000) {                   // if some time passed since we got some data from client, try to read anyway, to detect client disconnect
                uint8_t buf[2];
                recvFromClient(buf, 2);         // if this read fails with 0, it will automatically close socket
            }
        }

        return false;
    }
    Debug::out(LOG_DEBUG, "actionNeeded() - bytesAvailable: %d", bytesAvailable);

    lastTimeRecv = Utils::getCurrentMs();       // last time we've something received - now

    // if waitForAtn() succeeds, it fills 8 bytes of data in buffer
    // ...but then we might need some little more, so let's determine what it was
    // and keep reading as much as needed

    // we might need to wait for ATN multiple times, as there might be ZEROS packet or IKBD packet before we read wanted Hans or Franz packet
    while(sigintReceived == 0) {
        // check for any ATN code waiting from Hans
        bool good = waitForAtn(NET_ATN_ANY_ID, ATN_ANY, 0, inBuf);    // which chip wants to communicate? (which chip's stream we should process?)

        if(!good) {                                         // not good? break loop, no action needed
            break;
        }

        //Debug::out(LOG_DEBUG, "actionNeeded() - gotAtnId=%d, gotAtnCode=%d", gotAtnId, gotAtnCode);

        if(gotAtnId == NET_ATN_HANS_ID) {                   // for Hans
            Debug::cmdMarkStartTime();

            if(gotAtnCode == ATN_ACSI_COMMAND) {            // for this command read all ACSI command bytes
                recvFromClient(inBuf + 8, 14);
            }

            return true;
        }

        // if came here, probably weird situation, quit
        Debug::out(LOG_DEBUG, "actionNeeded() - weird situation?");
        break;
    }

    // no action needed
    return false;
}

void ChipInterfaceNetwork::getFWversion(uint8_t *inFwVer)
{
    // fwResponseBfr should be filled with Hans config - by calling setHDDconfig() (and not calling anything else inbetween)
    sendDataToChip(CMD_ACSI_CONFIG, fwResponseBfr, response.currentLength);

    recvFromClient(inFwVer, HDD_FW_RESPONSE_LEN);

    ChipInterface::convertXilinxInfo(inFwVer[5]);  // convert xilinx info into hwInfo struct

    int year = Utils::bcdToInt(inFwVer[1]) + 2000;
    Update::versions.hans.fromInts(year, Utils::bcdToInt(inFwVer[2]), Utils::bcdToInt(inFwVer[3]));       // store found FW version of Hans
}

bool ChipInterfaceNetwork::hdd_sendData_start(uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "ChipInterfaceNetwork::hdd_sendData_start -- trying to send more than 16 MB, fail");
        return false;
    }

    Utils::store24bits(&bufOut[0], totalDataCount);             // store data size
    bufOut[3] = scsiStatus;                                     // store status

    // transmit this command
    uint8_t cmd = withStatus ? CMD_DATA_READ_WITH_STATUS : CMD_DATA_READ_WITHOUT_STATUS;  // store command - with or without status
    sendDataToChip(cmd, bufOut, 4);

    return true;
}

bool ChipInterfaceNetwork::hdd_sendData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
    sendDataToChip(CMD_DATA_MARKER, pData, dataCount);
    return true;
}

bool ChipInterfaceNetwork::hdd_recvData_start(uint8_t *recvBuffer, uint32_t totalDataCount)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "ChipInterfaceNetwork::hdd_recvData_start() -- trying to send more than 16 MB, fail");
        return false;
    }

    // first send the command and tell Hans that we need WRITE data
    Utils::store24bits(&bufOut[0], totalDataCount);             // store data size
    bufOut[3] = 0xff;                                           // store INVALID status, because the real status will be sent on CMD_SEND_STATUS

    // transmit this command
    sendDataToChip(CMD_DATA_WRITE, bufOut, 4);

    return true;
}

bool ChipInterfaceNetwork::hdd_recvData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
    memset(bufOut, 0, TX_RX_BUFF_SIZE);                   // nothing to transmit, really...
    uint8_t inBuf[8];

    while(dataCount > 0) {
        // request maximum 512 bytes from host
        uint32_t subCount = (dataCount > 512) ? 512 : dataCount;

        bool good = waitForAtn(NET_ATN_HANS_ID, ATN_WRITE_MORE_DATA, 1000, inBuf);

        if(!good) {         // failed to get right CMD from Hans? fail
            return false;
        }

        // transmit data (size = subCount) + header and footer (size = 8) - already received 4 bytes
        int wantCount = subCount + 8 - 4;
        int gotCount = recvFromClient(bufIn, wantCount);

        if(gotCount < wantCount) {                          // if not all data was received
            return false;
        }

        memcpy(pData, bufIn + 2, subCount);                 // copy just the data, skip sequence number

        dataCount   -= subCount;                            // decreate the data counter
        pData       += subCount;                            // move in the buffer further
    }

    return true;
}

bool ChipInterfaceNetwork::hdd_sendStatusToHans(uint8_t statusByte)
{
    bool good = waitForAtn(NET_ATN_HANS_ID, ATN_GET_STATUS, 1000, bufIn);

    if(!good) {         // failed to get right CMD from Hans? fail
        return false;
    }

    bufOut[0] = statusByte;                          // set the command and the statusByte
    sendDataToChip(CMD_SEND_STATUS, bufOut, 1);

    return true;
}

bool ChipInterfaceNetwork::waitForAtn(int atnIdWant, uint8_t atnCode, uint32_t timeoutMs, uint8_t *inBuf)
{
    gotAtnId = 0;
    gotAtnCode = 0;

    // we might need to wait for ATN multiple times, as there might be ZEROS packet or IKBD packet before we read wanted Hans or Franz packet
    while(sigintReceived == 0) {
        // check for any ATN code waiting from Hans
        int atnIdGot = bufReader.waitForAtn(atnCode, timeoutMs);            // which chip wants to communicate? (which chip's stream we should process?)
        uint8_t atnCode = bufReader.getAtnCode();                           // what command does this chip wants us to handle?

        if(atnIdGot == NET_ATN_DISCONNECTED) {         // if buffered reader detected client disconnect, close it and quit
            Debug::out(LOG_DEBUG, "waitForAtn() - DISCONNECTED!");

            closeClientSocket();
            return false;
        }

        // it's not ZEROS and not IKBD, but it's NONE? fail
        if(atnIdGot == NET_ATN_NONE_ID) {
            return false;
        }

        //Debug::out(LOG_DEBUG, "waitForAtn() - atnIdGot=%d, atnCode=%d", atnIdGot, atnCode);

        // if we got here, it'z not ZEROS, IKDB or NONE, so it's FRANZ or HANS
        memcpy(inBuf, bufReader.getHeaderPointer(), 8); // copy in the header to start of buffer
        bufReader.clear();                              // clear buffered reader after reading data

        //Debug::out(LOG_DEBUG, "waitForAtn() - %02X %02X %02X %02X %02X %02X %02X %02X", inBuf[0], inBuf[1], inBuf[2], inBuf[3], inBuf[4], inBuf[5], inBuf[6], inBuf[7]);

        // store which chip wants which command to be handled
        gotAtnId = atnIdGot;
        gotAtnCode = atnCode;

        if(atnIdWant == NET_ATN_ANY_ID) {               // waiting for ANY? then Franz or Hans is fine
            return true;
        }

        // wanted Hans and got Hans, or wanted Franz and got Franz? good
        if(atnIdWant == atnIdGot) {
            return true;
        }

        // if arrived here, no wanted ATN was found
        break;
    }

    // wanted ATN didn't come
    return false;
}

void ChipInterfaceNetwork::sendDataToChip(uint16_t cmdCode, uint8_t* data, uint16_t len)        // send data to chip with specified tag
{
    if(fdClient < 0) {                      // no client socket? quit
        return;
    }

    uint8_t head[10];
    Utils::storeDword(head + 0, 0xc050d1c5);    // 0..3: 0xc050d1c5 [COSmODICS] (4 bytes)
    Utils::storeWord(head + 4, cmdCode);        // 4..5: ATN code (2 bytes)
    Utils::storeDword(head + 6, len);           // 6..9: txLen (4 bytes)

    write(fdClient, head, 10);              // send header
    write(fdClient, data, len);             // send data
}

int ChipInterfaceNetwork::recvFromClient(uint8_t* buf, int len)
{
    int received = 0;                       // total received count

    int bytes = recv(fdClient, buf, len, 0);    // try to receive whole buffer
    received += bytes;

    if(bytes > 0 && bytes < len) {              // if something was received, but it was less than wanted count, only part of data was received - we might either receive rest of data, or zero on client disconnected
        bytes = recv(fdClient, buf + bytes, len - bytes, 0);    // try to receive rest of data. If client disconnected, this is return 0
        received += bytes;
    }

    if(bytes == 0) {                            // if recv() returned 0, client has disconnected
        closeClientSocket();
    }

    // if(received > 0) {
    //     Debug::out(LOG_DEBUG, "recvFromClient(): %d bytes", received);
    //     Debug::outBfr(buf, received);
    // }

    return received;                            // return total bytes received
}
