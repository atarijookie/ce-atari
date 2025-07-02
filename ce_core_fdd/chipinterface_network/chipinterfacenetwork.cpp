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
#include <netinet/tcp.h>

#include "chipinterfacenetwork.h"
#include "../utils.h"
#include "../debug.h"
#include "../global.h"
#include "../update.h"

extern TFlags    flags;                 // global flags from command line

#define SERVER_STATUS_NOT_RUNNING   0       // when this server slot is not used yet and server is not running
#define SERVER_STATUS_FREE          1       // server is running but no client is connected there
#define SERVER_STATUS_OCCUPIED      2       // server is running and client is connected

#define SERVER_UDP_PORT         7200        // port number where this main CE network server listens
#define CLIENT_UDP_PORT         7201
#define SERVER_TCP_PORT_FIRST   7300

extern TFlags    flags;                 // global flags from command line

ChipInterfaceNetwork::ChipInterfaceNetwork()
{
    fdListen = FD_EMPTY;

    for(int i=0; i<MAX_CLIENTS; i++) {
        fdClients[i] = FD_EMPTY;
        clientLastMs[i] = 0;
    }

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

int ChipInterfaceNetwork::getEmptyClientIndex(void)
{
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(fdClients[i] == FD_EMPTY) {
            return i;
        }
    }

    return -1;
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
    if (listen(fdListen, 5) < 0) {
        Debug::out(LOG_ERROR, "netServer - listen() failed");
        return;
    }

    Debug::out(LOG_INFO, "netServer - listening on tcp port: %d", flags.portClient);
}

void ChipInterfaceNetwork::acceptSocketIfNeededAndPossible(void)
{
    int idx = getEmptyClientIndex();

    // out of empty indexes, don't accept
    if(idx < 0 || idx >= MAX_CLIENTS) {
        return;
    }

    // don't have client socket, try accept()
    struct sockaddr_in addressClient;
    socklen_t addrSize = sizeof(addressClient);

    int newSock = accept(fdListen, (struct sockaddr *) &addressClient, &addrSize);

    if(newSock < 0) {       // nothing to accept, would block? quit
        return;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;    // 500 ms timeout on blocking reads
    setsockopt(newSock, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv));

    // turn off Nagle's algorithm
    int flag = 1;
    setsockopt(newSock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    // got the new client socket now
    fdClients[idx] = newSock;
    clientLastMs[idx] = Utils::getCurrentMs();

    char clientIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addressClient.sin_addr, clientIp, sizeof(clientIp));

    Debug::out(LOG_INFO, "acceptSocketIfNeededAndPossible() - client #%d connected from %s", idx, clientIp);
}

void ChipInterfaceNetwork::closeClientSocket(void)
{
    // Utils::closeFdIfOpen(fdClient);            // close socket

    Debug::out(LOG_DEBUG, "closeClientSocket() - client disconnected");
}

bool ChipInterfaceNetwork::ciOpen(void)
{
    createListeningSocket();
    return true;
}

void ChipInterfaceNetwork::ciClose(void)
{
    // close sockets
    Utils::closeFdIfOpen(fdListen);

    for(int i=0; i<MAX_CLIENTS; i++) {
        Utils::closeFdIfOpen(fdClients[i]);
    }
}

int ChipInterfaceNetwork::disconnectInactiveClients(void)
{
    int maxFd = -1;
    uint32_t now = Utils::getCurrentMs();

    for(int i=0; i<MAX_CLIENTS; i++) {
        if(fdClients[i] == FD_EMPTY) {      // no client at this index? skip
            continue;
        }

        // there hasn't been any data from this client for some time? close connection
        uint32_t diff = now - clientLastMs[i];

        if(diff > 15000) {
            Utils::closeFdIfOpen(fdClients[i]);
            clientLastMs[i] = 0;
            Debug::out(LOG_INFO, "disconnected inactive client #%i", i);
        }
    }

    return maxFd;
}

int ChipInterfaceNetwork::setAllClientFds(fd_set* readfds)
{
    int maxFd = -1;

    for(int i=0; i<MAX_CLIENTS; i++) {
        if(fdClients[i] != FD_EMPTY) {      // got this client?
            FD_SET(fdClients[i], readfds);
            maxFd = MAX(fdClients[i], maxFd);
        }
    }

    return maxFd;
}

void ChipInterfaceNetwork::handleAllReadyClients(fd_set* readfds)
{
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(fdClients[i] == FD_EMPTY) {      // no client here? skip it
            continue;
        }

        if(FD_ISSET(fdClients[i], readfds)) {           // this fd read for read?
            // fdClients[i]
            int bytesRead = 0;

            // TODO: handle data

            if(bytesRead > 0) {     // something was read, mark client as active
                clientLastMs[i] = Utils::getCurrentMs();
            }
        }
    }
}

bool ChipInterfaceNetwork::actionNeeded(uint8_t *inBuf)
{
    acceptSocketIfNeededAndPossible();  // if don't have client connected, try to accept connection from client

    if(fdClient <= 0) {                 // (still) no client connected? no action needed
        //Debug::out(LOG_DEBUG, "actionNeeded() - client not connected yet");
        return false;
    }

    int bytesAvailable;
    int rv = ioctl(fdClient, FIONREAD, &bytesAvailable);    // how many bytes we can read?

    uint32_t now = Utils::getCurrentMs();

    if(rv < 0 || bytesAvailable <= 0) {                     // ioctl fail or nothing to read? no action needed
        if((now - lastTimeRecv) > 5000) {
            Utils::closeFdIfOpen(fdClient);
            Debug::out(LOG_INFO, "actionNeeded() - no client data for some time, disconnecting");
        }

        return false;
    }
    Debug::out(LOG_DEBUG, "actionNeeded() - bytesAvailable: %d", bytesAvailable);

    lastTimeRecv = now;       // last time we've something received - now

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

        if(gotAtnId == NET_ATN_FRANZ_ID) {                  // for Franz
            if(gotAtnCode == ATN_SEND_TRACK) {              // for this command read 2 more bytes: side + track
                recvFromClient(inBuf + 8, 2);
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
    // fwResponseBfr should be filled with Franz config - by calling setFDDconfig() (and not calling anything else inbetween)
    // sendDataToChip(fwResponseBfr, FDD_FW_RESPONSE_LEN);

    recvFromClient(inFwVer, bufReader.dataSizeRest());

    int year = Utils::bcdToInt(inFwVer[1]) + 2000;
    Update::versions.franz.fromInts(year, Utils::bcdToInt(inFwVer[2]), Utils::bcdToInt(inFwVer[3]));              // store found FW version of Franz
}

void ChipInterfaceNetwork::fdd_sendTrackToChip(int byteCount, uint8_t *encodedTrack)
{
    // send encoded track out, read garbage into bufIn and don't care about it
    sendDataToChip(encodedTrack, byteCount);
}

uint8_t* ChipInterfaceNetwork::fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount)
{
    byteCount = bufReader.dataSizeRest();   // get how many data we still have

    // get all the remaining data
    recvFromClient(bufIn, byteCount);

    // get the written sector, side, track number
    sector  = bufIn[1];
    track   = bufIn[0] & 0x7f;
    side    = (bufIn[0] & 0x80) ? 1 : 0;

    return bufIn;                                           // return pointer to received written sector
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

void ChipInterfaceNetwork::storeHeaderToBuffer(uint16_t cmdCode, uint32_t futureDatalen, uint8_t* buffer)
{
    Utils::storeDword(buffer + 0, SYNC_TAG_FDD);  // 0..3: 0xc050d1c5 [COSmODICS] (4 bytes)
    Utils::storeWord(buffer + 4, cmdCode);        // 4..5: ATN code (2 bytes)
    Utils::storeDword(buffer + 6, futureDatalen); // 6..9: futureDatalen (4 bytes)
}

bool ChipInterfaceNetwork::sendHeaderToChip(uint16_t cmdCode, uint32_t futureDatalen)        // send header to chip
{
    if(fdClient < 0) {                      // no client socket? quit
        return false;
    }

    uint8_t head[10];
    storeHeaderToBuffer(cmdCode, futureDatalen, head);

    int res = write(fdClient, head, 10);        // send header
    return (res == 10);
}

bool ChipInterfaceNetwork::sendDataToChip(uint8_t* data, uint32_t len)        // send data to chip
{
    if(fdClient < 0) {                      // no client socket? quit
        return false;
    }

    int res = write(fdClient, data, len);   // send data
    return (((uint32_t)res) == len);
}

bool ChipInterfaceNetwork::sendHeaderAndDataToChip(uint16_t cmdCode, uint8_t* data, uint32_t len)        // send header and data to chip
{
    bool good;

    if(len > 512)       // for larger data send using separate write() commands
    {
        if(!sendHeaderToChip(cmdCode, len))
        {
            Debug::out(LOG_DEBUG, "sendHeaderAndDataToChip failed!");
            return false;
        }

        good = sendDataToChip(data, len);
        Debug::out(LOG_DEBUG, "sendHeaderAndDataToChip - good: %d", good);
    } 
    else                // for small data first copy data into buffers, then send with one write() command
    {
        if(fdClient < 0) {                      // no client socket? quit
            return false;
        }

        uint8_t bfr[522];
        storeHeaderToBuffer(cmdCode, len, bfr);     // store header at start
        memcpy(bfr + 10, data, len);                // copy data after the header

        int res = write(fdClient, bfr, len + 10);   // send header and data
        good = (res == ((int) (len + 10)));
    }

    return good;
}

/*
    Receive data from client, up to rest of the data size specified in header,
    respecting maximum length of buffer (maxLen).
*/
uint32_t ChipInterfaceNetwork::recvFromClient(uint8_t* buf, int maxLen)
{
    int received = 0;                       // total received count

    uint32_t dataSize = bufReader.dataSizeRest();          // get how many bytes we can read from the client to get whole data part
    uint32_t readSize = MIN(dataSize, (uint32_t) maxLen);   // read less if supplied buffer is not large enough, or there isn't as much data as requested

    for(int i=0; i<3; i++)
    {
        int bytes = recv(fdClient, buf + received, readSize, 0);    // try to receive whole buffer

        if(bytes > 0)      // on data received
        {
            received += bytes;
            readSize -= bytes;
        }

        if(readSize <= 0)   // nothing to read anymore?
        {
            break;
        }
    }

    // if(received > 0) {
    //     Debug::out(LOG_DEBUG, "recvFromClient(): %d bytes", received);
    //     Debug::outBfr(buf, received);
    // }

    bufReader.decreaseDataSize((uint32_t) received);    // decrease the remaining data by the size we have received
    return received;        // return total bytes received
}

void ChipInterfaceNetwork::setFDDconfig(bool setFloppyConfig, FloppyConfig* fddConfig, bool setDiskChanged, bool diskChanged)
{
    // memset(fwResponseBfr, 0, FDD_FW_RESPONSE_LEN);

    // responseStart(FDD_FW_RESPONSE_LEN);                             // init the response struct
    // floppySoundEnabled = fddConfig->soundEnabled;               // store this in instance var for later use

    // if(setFloppyConfig) {                                       // should set floppy config?
    //     // responseAddByte(fwResponseBfr, ( fddConfig->enabled         ? CMD_DRIVE_ENABLED     : CMD_DRIVE_DISABLED) );
    //     // responseAddByte(fwResponseBfr, ((fddConfig->id == 0)        ? CMD_SET_DRIVE_ID_0    : CMD_SET_DRIVE_ID_1) );
    //     // responseAddByte(fwResponseBfr, ( fddConfig->writeProtected  ? CMD_WRITE_PROTECT_ON  : CMD_WRITE_PROTECT_OFF) );
    // }

    // if(setDiskChanged) {
    //     // responseAddByte(fwResponseBfr, ( diskChanged    ? CMD_DISK_CHANGE_ON    : CMD_DISK_CHANGE_OFF) );
    // }
}

