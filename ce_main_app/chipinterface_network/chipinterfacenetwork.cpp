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

    pipeFromAtariToRPi[0] = pipeFromAtariToRPi[1] = -1;
    pipeFromRPiToAtari[0] = pipeFromRPiToAtari[1] = -1;

    bufOut = new uint8_t[MFM_STREAM_SIZE];
    bufIn = new uint8_t[MFM_STREAM_SIZE];

    gotAtnId = 0;
    gotAtnCode = 0;

    lastTimeRecv = Utils::getCurrentMs();

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
    closeFdIfOpen(fdClient);            // close socket
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
    closeFdIfOpen(pipeFromAtariToRPi[0]);
    closeFdIfOpen(pipeFromAtariToRPi[1]);
    closeFdIfOpen(pipeFromRPiToAtari[0]);
    closeFdIfOpen(pipeFromRPiToAtari[1]);
}

void ChipInterfaceNetwork::ikbdUartEnable(bool enable)
{
    // nothing needed to be done here
}

int ChipInterfaceNetwork::ikbdUartReadFd(void)
{
    return pipeFromAtariToRPi[0];       // IKBD thread will READ from read end ([0]) of pipe going from Atari to RPi
}

int ChipInterfaceNetwork::ikbdUartWriteFd(void)
{
    return pipeFromRPiToAtari[1];       // IKBD thread will WRITE to write end ([1]) of pipe going from RPi to Atari
}

void ChipInterfaceNetwork::serialSetup(void)
{
    // create pipes for communication with ikbd thread
    int res;

    res = pipe(pipeFromAtariToRPi);

    if(res  < 0) {
        Debug::out(LOG_ERROR, "failed to create pipeFromAtariToRPi");
    }

    res = pipe(pipeFromRPiToAtari);

    if(res  < 0) {
        Debug::out(LOG_ERROR, "failed to create pipeFromRPiToAtari");
    }
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

bool ChipInterfaceNetwork::actionNeeded(bool &hardNotFloppy, uint8_t *inBuf)
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
        //Debug::out(LOG_DEBUG, "actionNeeded() - nothing comming from fdClient");

        if(rv == 0 && bytesAvailable == 0) {    // ioctl() succeeded, but can't read anything
            uint32_t now = Utils::getCurrentMs();
            uint32_t diff = now - lastTimeRecv;

            if(diff > 3000) {                   // if some time passed since we got some data from client, try to read anyway, to detect client disconnect
                uint8_t buf[2];
                recvFromClient(buf, 2);         // if this read fails with 0, it will automatically close socket
            }
        }

        sendIkbdDataToAtari();                  // but send IKBD data to Atari
        return false;
    }

    lastTimeRecv = Utils::getCurrentMs();       // last time we've something received - now

    // if waitForATN() succeeds, it fills 8 bytes of data in buffer
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
            if(gotAtnCode == ATN_ACSI_COMMAND) {            // for this command read all ACSI command bytes
                recvFromClient(inBuf + 8, 14);
            }

            hardNotFloppy = true;
            return true;
        }

        if(gotAtnId == NET_ATN_FRANZ_ID) {                  // for Franz
            if(gotAtnCode == ATN_SEND_TRACK) {              // for this command read 2 more bytes: side + track
                recvFromClient(inBuf + 8, 2);
            }

            hardNotFloppy = false;
            return true;
        }

        // if came here, probably weird situation, quit
        Debug::out(LOG_DEBUG, "actionNeeded() - weird situation?");
        break;
    }

    // no action needed
    return false;
}

void ChipInterfaceNetwork::getFWversion(bool hardNotFloppy, uint8_t *inFwVer)
{
    // Debug::out(LOG_DEBUG, "getFWversion(): hardNotFloppy=%d", hardNotFloppy);

    if(hardNotFloppy) {     // for HDD
        // fwResponseBfr should be filled with Hans config - by calling setHDDconfig() (and not calling anything else inbetween)
        sendDataToChip(NET_TAG_HANS_STR, fwResponseBfr, HDD_FW_RESPONSE_LEN);

        recvFromClient(inFwVer, HDD_FW_RESPONSE_LEN);

        ChipInterface::convertXilinxInfo(inFwVer[5]);  // convert xilinx info into hwInfo struct

        hansConfigWords.current.acsi = MAKEWORD(inFwVer[6], inFwVer[7]);
        hansConfigWords.current.fdd  = MAKEWORD(inFwVer[8],        0);

        int year = Utils::bcdToInt(inFwVer[1]) + 2000;
        Update::versions.hans.fromInts(year, Utils::bcdToInt(inFwVer[2]), Utils::bcdToInt(inFwVer[3]));       // store found FW version of Hans
    } else {                // for FDD
        // fwResponseBfr should be filled with Franz config - by calling setFDDconfig() (and not calling anything else inbetween)
        sendDataToChip(NET_TAG_FRANZ_STR, fwResponseBfr, FDD_FW_RESPONSE_LEN);

        recvFromClient(inFwVer, FDD_FW_RESPONSE_LEN);

        int year = Utils::bcdToInt(inFwVer[1]) + 2000;
        Update::versions.franz.fromInts(year, Utils::bcdToInt(inFwVer[2]), Utils::bcdToInt(inFwVer[3]));              // store found FW version of Franz
    }
}

bool ChipInterfaceNetwork::hdd_sendData_start(uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "ChipInterfaceNetwork::hdd_sendData_start -- trying to send more than 16 MB, fail");
        return false;
    }

    memset(bufOut, 0, COMMAND_SIZE);

    bufOut[3] = withStatus ? CMD_DATA_READ_WITH_STATUS : CMD_DATA_READ_WITHOUT_STATUS;  // store command - with or without status
    bufOut[4] = totalDataCount >> 16;                           // store data size
    bufOut[5] = totalDataCount >>  8;
    bufOut[6] = totalDataCount  & 0xff;
    bufOut[7] = scsiStatus;                                     // store status

    // transmit this command
    sendDataToChip(NET_TAG_HANS_STR, bufOut, COMMAND_SIZE);

    return true;
}

bool ChipInterfaceNetwork::hdd_sendData_transferBlock(uint8_t *pData, uint32_t dataCount)
{
    bufOut[0] = 0;
    bufOut[1] = CMD_DATA_MARKER;                                  // mark the start of data

    if((dataCount & 1) != 0) {                                      // odd number of bytes? make it even, we're sending words...
        dataCount++;
    }

    while(dataCount > 0) {                                          // while there's something to send
        bool good = waitForAtn(NET_ATN_HANS_ID, ATN_READ_MORE_DATA, 1000, bufIn);

        if(!good) {         // failed to get right CMD from Hans? fail
            return false;
        }

        uint32_t cntNow = (dataCount > 512) ? 512 : dataCount;         // max 512 bytes per transfer

        memcpy(bufOut + 2, pData, cntNow);                          // copy the data after the header (2 bytes)

        // transmit this buffer with header + terminating zero (uint16_t)
        sendDataToChip(NET_TAG_HANS_STR, bufOut, cntNow + 4);

        pData       += cntNow;                                      // move the data pointer further
        dataCount   -= cntNow;
    }

    return true;
}

bool ChipInterfaceNetwork::hdd_recvData_start(uint8_t *recvBuffer, uint32_t totalDataCount)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "ChipInterfaceNetwork::hdd_recvData_start() -- trying to send more than 16 MB, fail");
        return false;
    }

    memset(bufOut, 0, COMMAND_SIZE);

    // first send the command and tell Hans that we need WRITE data
    bufOut[3] = CMD_DATA_WRITE;                                 // store command - WRITE
    bufOut[4] = totalDataCount >> 16;                           // store data size
    bufOut[5] = totalDataCount >>  8;
    bufOut[6] = totalDataCount  & 0xff;
    bufOut[7] = 0xff;                                           // store INVALID status, because the real status will be sent on CMD_SEND_STATUS

    // transmit this command
    sendDataToChip(NET_TAG_HANS_STR, bufOut, COMMAND_SIZE);

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

    memset(bufOut, 0, 16);                                // clear the tx buffer
    bufOut[1] = CMD_SEND_STATUS;                          // set the command and the statusByte
    bufOut[2] = statusByte;

    // transmit the statusByte (16 bytes total, but 8 already received)
    sendDataToChip(NET_TAG_HANS_STR, bufOut, 16 - 8);

    return true;
}

void ChipInterfaceNetwork::fdd_sendTrackToChip(int byteCount, uint8_t *encodedTrack)
{
    // send encoded track out, read garbage into bufIn and don't care about it
    sendDataToChip(NET_TAG_FRANZ_STR, encodedTrack, byteCount);
}

uint8_t* ChipInterfaceNetwork::fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount)
{
    byteCount = bufReader.getRemainingLength();             // get how many data we still have

    // get all the remaining data
    recvFromClient(bufIn, byteCount);

    // get the written sector, side, track number
    sector  = bufIn[1];
    track   = bufIn[0] & 0x7f;
    side    = (bufIn[0] & 0x80) ? 1 : 0;

    return bufIn;                                           // return pointer to received written sector
}

void ChipInterfaceNetwork::handleZerosAndIkbd(int atnId)
{
    int cntWant = bufReader.getRemainingLength();   // get how much we should get to receive whole packet
    cntWant = MIN(cntWant, MFM_STREAM_SIZE);            // limit the received lenght to buffer size

    int cntGot = recvFromClient(bufIn, cntWant);    // read data

    if(cntGot < cntWant) {                              // not enough data read? 
        Debug::out(LOG_DEBUG, "handleZerosAndIkbd: got %d bytes, but wanted %d bytes (%d < %d)", cntGot, cntWant, cntGot, cntWant);
    }

    if(cntGot <= 0) {                                   // on error, nothing more to do
        return;
    }

    if(atnId == NET_ATN_IKBD_ID) {                          // for IKBD - read data, feed to pipe
        // write data WRITE end ([1]) in ikbd pipe going from Atari to RPi
        if(pipeFromAtariToRPi[1] != -1) {                   // got this pipe open?
            write(pipeFromAtariToRPi[1], bufIn, cntGot);    // write all the data into pipe
        }
    }

    // for NET_ATN_ZEROS_ID - nothing to do, just ignore the zeros

    bufReader.clear(); 
}

bool ChipInterfaceNetwork::waitForAtn(int atnIdWant, uint8_t atnCode, uint32_t timeoutMs, uint8_t *inBuf)
{
    gotAtnId = 0;
    gotAtnCode = 0;

    // we might need to wait for ATN multiple times, as there might be ZEROS packet or IKBD packet before we read wanted Hans or Franz packet
    while(sigintReceived == 0) {
        // check for any ATN code waiting from Hans
        int atnIdGot = bufReader.waitForATN(atnCode, timeoutMs);            // which chip wants to communicate? (which chip's stream we should process?)
        uint8_t atnCode = bufReader.getAtnCode();                           // what command does this chip wants us to handle?

        if(atnIdGot == NET_ATN_DISCONNECTED) {         // if buffered reader detected client disconnect, close it and quit
            Debug::out(LOG_DEBUG, "waitForAtn() - DISCONNECTED!");

            closeClientSocket();
            return false;
        }

        // if ZEROS or IKBD packed was found, process it and then try looking for Franz or Hans packet again
        if(atnIdGot == NET_ATN_ZEROS_ID || atnIdGot == NET_ATN_IKBD_ID) {
            Debug::out(LOG_DEBUG, "waitForAtn() - got ZEROS or IKDB");

            handleZerosAndIkbd(atnIdGot);
            continue;
        }

        // it's not ZEROS and not IKBD, but it's NONE? fail
        if(atnIdGot == NET_ATN_NONE_ID) {
            return false;
        }

        //Debug::out(LOG_DEBUG, "waitForAtn() - atnIdGot=%d, atnCode=%d", atnIdGot, atnCode);

        // if we got here, it'z not ZEROS, IKDB or NONE, so it's FRANZ or HANS
        memcpy(inBuf, bufReader.getHeaderPointer(), 8); // copy in the header to start of buffer
        byteSwapBfr(inBuf, 8);
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

void ChipInterfaceNetwork::sendIkbdDataToAtari(void)
{
    if(pipeFromRPiToAtari[0] < 0 || fdClient < 0) {         // no pipe or socket open? quit
        return;
    }

    int bytesAvailable;
    int rv = ioctl(pipeFromRPiToAtari[0], FIONREAD, &bytesAvailable);    // how many bytes we can read?

    if(rv <= 0 || bytesAvailable <= 0) {                    // ioctl fail or nothing to read? no action needed
        return;
    }

    ssize_t cntWant = MIN(bytesAvailable, MFM_STREAM_SIZE); // limit read size to maximum of buffer size
    ssize_t cntGot = read(pipeFromRPiToAtari[0], bufOut, cntWant);

    if(cntGot > 0) {                                        // got some data from pipe? send it to socket
        sendDataToChip(NET_TAG_IKBD_STR, bufOut, cntGot);         // write all the data into socket
    }
}

void ChipInterfaceNetwork::sendDataToChip(const char* tag, uint8_t* data, uint16_t len)        // send data to chip with specified tag
{
    if(fdClient < 0) {                      // no client socket? quit
        return;
    }

    uint8_t head[6];
    memcpy(head, tag, 4);                   // 0..3: tag
    Utils::storeWord(head + 4, len);        // 4..5: length

    write(fdClient, head, 6);               // send header
    write(fdClient, data, len);             // send data
}

int ChipInterfaceNetwork::recvFromClient(uint8_t* buf, int len, bool byteSwap)
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

    if(byteSwap) {                              // if should byteswap data
        byteSwapBfr(buf, len);
    }

    // if(received > 0) {
    //     Debug::out(LOG_DEBUG, "recvFromClient(): %d bytes", received);
    //     Debug::outBfr(buf, received);
    // }

    return received;                            // return total bytes received
}

void ChipInterfaceNetwork::byteSwapBfr(uint8_t* buf, int len)
{
    for(int i=0; i<len; i += 2) {
        uint8_t tmp = buf[i];
        buf[i] = buf[i+1];
        buf[i+1] = tmp;
    }
}

void ChipInterfaceNetwork::handleButton(int& btnDownTime, uint32_t& nextScreenTime)
{

}

void ChipInterfaceNetwork::handleBeeperCommand(int beeperCommand, FloppyConfig *fc)
{

}

// returns true if should handle i2c display from RPi
bool ChipInterfaceNetwork::handlesDisplay(void)
{
    return false;        // network interface doesn't handle display locally
}

// send this display buffer data to remote display
void ChipInterfaceNetwork::displayBuffer(uint8_t *bfr, uint16_t size)
{
    // TODO: send this buffer to client, so he can show it
}
