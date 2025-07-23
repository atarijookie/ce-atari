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
#include "../floppythread.h"

extern TFlags    flags;                 // global flags from command line

#define SERVER_STATUS_NOT_RUNNING   0       // when this server slot is not used yet and server is not running
#define SERVER_STATUS_FREE          1       // server is running but no client is connected there
#define SERVER_STATUS_OCCUPIED      2       // server is running and client is connected

#define SERVER_UDP_PORT         7200        // port number where this main CE network server listens
#define CLIENT_UDP_PORT         7201
#define SERVER_TCP_PORT_FIRST   7300

extern TFlags    flags;                 // global flags from command line

ChipInterfaceNetwork::ChipInterfaceNetwork(int whichLogFile)
{
    whichLog = whichLogFile;

    fdListen = FD_EMPTY;

    clientsClearAll();

    bufOut = new uint8_t[MFM_STREAM_SIZE];
    bufIn = new uint8_t[MFM_STREAM_SIZE];

    gotAtnId = 0;
    gotAtnCode = 0;
}

ChipInterfaceNetwork::~ChipInterfaceNetwork()
{
    delete []bufOut;
    delete []bufIn;
}

void ChipInterfaceNetwork::createListeningSocket(void)
{
    if(fdListen >= 0) { // if the listening socket seems to be already created, just quit
        return;
    }

    // open socket
    if ((fdListen = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        Debug::out(whichLog, LOG_ERROR, "netServer - failed to open socket");
        return;
    }

    // Forcefully attach socket to the port
    int opt = 1;

    if (setsockopt(fdListen, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        Debug::out(whichLog, LOG_ERROR, "netServer - setsockopt() failed");
        return;
    }

    // change the socket into non-blocking
    fcntl(fdListen, F_SETFL, O_NONBLOCK);

    addressListen.sin_family = AF_INET;
    addressListen.sin_addr.s_addr = INADDR_ANY;
    addressListen.sin_port = htons( SERVER_TCP_PORT_FDD );

    // bind to address
    if (bind(fdListen, (struct sockaddr *) &addressListen, sizeof(addressListen)) < 0) {
        Debug::out(whichLog, LOG_ERROR, "netServer - bind() failed");
        return;
    }

    // mark the socket as a passive socket
    if (listen(fdListen, 5) < 0) {
        Debug::out(whichLog, LOG_ERROR, "netServer - listen() failed");
        return;
    }

    Debug::out(whichLog, LOG_INFO, "netServer - listening on tcp port: %d", SERVER_TCP_PORT_FDD);
}

void ChipInterfaceNetwork::acceptSocketIfNeededAndPossible(void)
{
    int idx = clientsGetEmptyIndex();

    // out of empty indexes, don't accept
    if(idx < 0 || idx >= MAX_CLIENTS) {
        return;
    }

    ClientInfo* clientInfo = &clients[idx];

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

    char clientIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addressClient.sin_addr, clientIp, sizeof(clientIp));
    uint32_t clientIpInt = ntohl(addressClient.sin_addr.s_addr);

    // got the new client socket now
    clientsStoreOne(clientInfo, newSock, clientIpInt);

    Debug::out(whichLog, LOG_INFO, "acceptSocketIfNeededAndPossible() - client #%d connected from %s, will use floppy slot #%d", idx, clientIp, clientInfo->floppySlotIndex);
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
       clientsCloseOne(&clients[i]);
    }
}

int ChipInterfaceNetwork::setAllClientFds(fd_set* readfds)
{
    int maxFd = FD_EMPTY;

    for(int i=0; i<MAX_CLIENTS; i++) {
        int fdClient = clients[i].fdClient;
        if(fdClient != FD_EMPTY) {      // got this client?
            FD_SET(fdClient, readfds);
            maxFd = MAX(fdClient, maxFd);
        }
    }

    return maxFd;
}

void ChipInterfaceNetwork::handleAllReadyClients(fd_set* readfds, FloppyThread* core)
{
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(clients[i].fdClient == FD_EMPTY) {           // no client here? skip it
            continue;
        }

        if(FD_ISSET(clients[i].fdClient, readfds)) {    // this fd read for read?
            bool hadData = core->handleOneClient(i, clients[i].fdClient, clients[i].floppySlotIndex);

            if(hadData) {       // something was read, mark client as active
                clients[i].lastMs = Utils::getCurrentMs();
            }
        }
    }
}

bool ChipInterfaceNetwork::actionNeeded(int clientIndex, uint8_t *inBuf)
{
    int& fdClient = clients[clientIndex].fdClient;

    if(fdClient <= 0) {                 // (still) no client connected? no action needed
        //Debug::out(whichLog, LOG_DEBUG, "actionNeeded() - client not connected yet");
        return false;
    }

    int bytesAvailable;
    int rv = ioctl(fdClient, FIONREAD, &bytesAvailable);    // how many bytes we can read?

    if(rv < 0 || bytesAvailable <= 0) {                     // ioctl fail or nothing to read? no action needed
        return false;
    }

    Debug::out(whichLog, LOG_DEBUG, "actionNeeded() - bytesAvailable: %d", bytesAvailable);

    // if waitForAtn() succeeds, it fills 8 bytes of data in buffer
    // ...but then we might need some little more, so let's determine what it was
    // and keep reading as much as needed

    // we might need to wait for ATN multiple times, as there might be ZEROS packet or IKBD packet before we read wanted Hans or Franz packet
    while(sigintReceived == 0) {
        // check for any ATN code waiting from Hans
        bool good = waitForAtn(clientIndex, NET_ATN_ANY_ID, ATN_ANY, 0, inBuf);    // which chip wants to communicate? (which chip's stream we should process?)

        if(!good) {                                         // not good? break loop, no action needed
            break;
        }

        //Debug::out(whichLog, LOG_DEBUG, "actionNeeded() - gotAtnId=%d, gotAtnCode=%d", gotAtnId, gotAtnCode);

        if(gotAtnId == NET_ATN_FRANZ_ID) {                  // for Franz
            if(gotAtnCode == ATN_SEND_TRACK) {              // for this command read 2 more bytes: side + track
                recvFromClient(clientIndex, inBuf + 8, 2);
            }

            return true;
        }

        // if came here, probably weird situation, quit
        Debug::out(whichLog, LOG_DEBUG, "actionNeeded() - weird situation?");
        break;
    }

    // no action needed
    return false;
}

bool ChipInterfaceNetwork::getFWversion(int clientIndex)
{
    // fwResponseBfr should be filled with Franz config - by calling setFDDconfig() (and not calling anything else inbetween)
    // sendDataToChip(fdClient, fwResponseBfr, FDD_FW_RESPONSE_LEN);

    #define FW_VER_SIZE     32
    uint8_t fwVer[FW_VER_SIZE];

    memset(fwVer, 0, FW_VER_SIZE);
    int readCnt = readRestOfData(clientIndex, fwVer, FW_VER_SIZE);

    if(readCnt < 12) {
        Debug::out(whichLog, LOG_ERROR, "getFWversion() -- not enough data received: %d", readCnt);
        return false;
    }

    int year = Utils::bcdToInt(fwVer[1]) + 2000;
    int month = Utils::bcdToInt(fwVer[2]);
    int day = Utils::bcdToInt(fwVer[3]);

    bool macChanged = false;
    if(memcmp(clients[clientIndex].mac, fwVer + 6, 6) != 0) {   // mac changed? (e.g. first received)
        memcpy(clients[clientIndex].mac, fwVer + 6, 6);         // copy mac address into client's info
        macChanged = true;
    }

    uint8_t* mac = clients[clientIndex].mac;

    Debug::out(whichLog, LOG_DEBUG, "FW: Franz, %d-%02d-%02d, mac: %02X:%02X:%02X:%02X:%02X:%02X", year, month, day, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return macChanged;
}

void ChipInterfaceNetwork::fdd_sendTrackToChip(int& fdClient, int byteCount, uint8_t *encodedTrack)
{
    // Debug::out(whichLog, LOG_DEBUG, "fdd_sendTrackToChip -- byteCount: %d", byteCount);
    sendHeaderAndDataToChip(fdClient, ATN_SEND_TRACK, encodedTrack, byteCount);
}

void ChipInterfaceNetwork::fdd_sendImageParamsToChip(int& fdClient, bool finished, int imgTracks, int imgSides, int imgSectorsPerTrack, std::string fileName)
{
    uint8_t bfr[64];
    memset(bfr, 0, 64);

    bfr[0] = finished ? 1 : 0;              // 1 for finished sending, 0 for start of sending
    bfr[1] = (uint8_t) imgTracks;
    bfr[2] = (uint8_t) imgSides;
    bfr[3] = (uint8_t) imgSectorsPerTrack;

    int fnameLen = MIN(fileName.length(), 31);
    memcpy(bfr + 4, fileName.c_str(), fnameLen);    // filename, max 31 chars + zero terminator

    Debug::out(whichLog, LOG_DEBUG, "fdd_sendImageParamsToChip -- finished: %d, tracks: %d, sides: %d, spt: %d, filename: %s", bfr[0], bfr[1], bfr[2], bfr[3], bfr + 4);
    sendHeaderAndDataToChip(fdClient, ATN_SEND_WHOLE_IMAGE, bfr, 4 + 32);   // 4 bytes param, 32 bytes filename
}

uint8_t* ChipInterfaceNetwork::fdd_sectorWritten(int clientIndex, int &side, int &track, int &sector, int &byteCount)
{
    // get all the remaining data
    byteCount = readRestOfData(clientIndex, bufIn, MFM_STREAM_SIZE);

    // get the written sector, side, track number
    sector  = bufIn[1];
    track   = bufIn[0] & 0x7f;
    side    = (bufIn[0] & 0x80) ? 1 : 0;

    return bufIn;                                           // return pointer to received written sector
}

bool ChipInterfaceNetwork::waitForAtn(int clientIndex, int atnIdWant, uint8_t atnCode, uint32_t timeoutMs, uint8_t *inBuf)
{
    gotAtnId = 0;
    gotAtnCode = 0;

    BufferedReader* bufReader = &clients[clientIndex].bufReader;
    bufReader->setFd(clients[clientIndex].fdClient);

    // we might need to wait for ATN multiple times, as there might be ZEROS packet or IKBD packet before we read wanted Hans or Franz packet
    while(sigintReceived == 0) {
        // check for any ATN code waiting from Hans
        int atnIdGot = bufReader->waitForAtn(atnCode, timeoutMs);            // which chip wants to communicate? (which chip's stream we should process?)
        uint8_t atnCode = bufReader->getAtnCode();                           // what command does this chip wants us to handle?

        if(atnIdGot == NET_ATN_DISCONNECTED) {         // if buffered reader detected client disconnect, close it and quit
            Debug::out(whichLog, LOG_DEBUG, "waitForAtn() - DISCONNECTED!");

            clientsCloseOne(&clients[clientIndex]);
            return false;
        }

        // it's not ZEROS and not IKBD, but it's NONE? fail
        if(atnIdGot == NET_ATN_NONE_ID) {
            return false;
        }

        //Debug::out(whichLog, LOG_DEBUG, "waitForAtn() - atnIdGot=%d, atnCode=%d", atnIdGot, atnCode);

        // if we got here, it'z not ZEROS, IKDB or NONE, so it's FRANZ or HANS
        memcpy(inBuf, bufReader->getHeaderPointer(), 8); // copy in the header to start of buffer
        bufReader->clear();                              // clear buffered reader after reading data

        //Debug::out(whichLog, LOG_DEBUG, "waitForAtn() - %02X %02X %02X %02X %02X %02X %02X %02X", inBuf[0], inBuf[1], inBuf[2], inBuf[3], inBuf[4], inBuf[5], inBuf[6], inBuf[7]);

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

bool ChipInterfaceNetwork::sendHeaderToChip(int& fdClient, uint16_t cmdCode, uint32_t futureDatalen)        // send header to chip
{
    if(fdClient < 0) {                      // no client socket? quit
        return false;
    }

    uint8_t head[10];
    storeHeaderToBuffer(cmdCode, futureDatalen, head);

    int res = send(fdClient, head, 10, MSG_NOSIGNAL);        // send header
    return (res == 10);
}

bool ChipInterfaceNetwork::sendDataToChip(int& fdClient, uint8_t* data, uint32_t len)        // send data to chip
{
    if(fdClient < 0) {                      // no client socket? quit
        return false;
    }

    int res = send(fdClient, data, len, MSG_NOSIGNAL);   // send data
    return (((uint32_t)res) == len);
}

bool ChipInterfaceNetwork::sendHeaderAndDataToChip(int& fdClient, uint16_t cmdCode, uint8_t* data, uint32_t len)        // send header and data to chip
{
    bool good;

    if(len > 512)       // for larger data send using separate send() commands
    {
        if(!sendHeaderToChip(fdClient, cmdCode, len))
        {
            Debug::out(whichLog, LOG_DEBUG, "sendHeaderAndDataToChip failed!");
            return false;
        }

        good = sendDataToChip(fdClient, data, len);
        // Debug::out(whichLog, LOG_DEBUG, "sendHeaderAndDataToChip - good: %d", good);
    } 
    else                // for small data first copy data into buffers, then send with one send() command
    {
        if(fdClient < 0) {                      // no client socket? quit
            return false;
        }

        uint8_t bfr[522];
        storeHeaderToBuffer(cmdCode, len, bfr);     // store header at start
        memcpy(bfr + 10, data, len);                // copy data after the header

        int res = send(fdClient, bfr, len + 10, MSG_NOSIGNAL);   // send header and data
        good = (res == ((int) (len + 10)));
    }

    return good;
}

/*
    Receive data from client, up to rest of the data size specified in header,
    respecting maximum length of buffer (maxLen).
*/
uint32_t ChipInterfaceNetwork::recvFromClient(int clientIndex, uint8_t* buf, int maxLen)
{
    int received = 0;                       // total received count

    uint32_t dataSize = clients[clientIndex].bufReader.dataSizeRest();          // get how many bytes we can read from the client to get whole data part
    uint32_t readSize = MIN(dataSize, (uint32_t) maxLen);   // read less if supplied buffer is not large enough, or there isn't as much data as requested

    for(int i=0; i<3; i++)
    {
        int bytes = recv(clients[clientIndex].fdClient, buf + received, readSize, 0);    // try to receive whole buffer

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
    //     Debug::out(whichLog, LOG_DEBUG, "recvFromClient(): %d bytes", received);
    //     Debug::outBfr(buf, received);
    // }

    clients[clientIndex].bufReader.decreaseDataSize((uint32_t) received);    // decrease the remaining data by the size we have received
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

void ChipInterfaceNetwork::clientsClearOne(ClientInfo* info)
{
    info->fdClient = FD_EMPTY;
    info->ipAddr = 0;
    memset(info->mac, 0, 6);
    info->floppySlotIndex = FD_EMPTY;
    info->lastMs = 0;
}

void ChipInterfaceNetwork::clientsClearAll(void)
{
    for(int i=0; i<MAX_CLIENTS; i++) {
        clientsClearOne(&clients[i]);
    }
}

// closes socket if open, then does clientsClearOne()
void ChipInterfaceNetwork::clientsCloseOne(ClientInfo* info)
{
    Utils::closeFdIfOpen(info->fdClient);
    clientsClearOne(info);
}

int ChipInterfaceNetwork::clientsGetEmptyIndex(void)
{
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(clients[i].fdClient == FD_EMPTY) {   // if this slot is empty, return it's index
            return i;
        }
    }

    return FD_EMPTY;        // nothing is empty
}

int ChipInterfaceNetwork::clientsGetFloppySlotIndexForIp(uint32_t ipAddr)
{
    uint32_t usedSlots = 0;

    // check if this ip is already using some slot and build usedSlots bits
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(clients[i].ipAddr == ipAddr) {   // if this slot is already using this ipAddress, return the floppy slot
            Debug::out(whichLog, LOG_INFO, "clientsGetFloppySlotIndexForIp - found IP in table, will reuse floppySlotIndex: %d", clients[i].floppySlotIndex);
            return clients[i].floppySlotIndex;
        }

        if(clients[i].floppySlotIndex != FD_EMPTY) {        // this client has a floppy slot, add its bit to usedSlots
            usedSlots |= (1 << clients[i].floppySlotIndex);
        }
    }

    // find a floppy slot that is not used
    for(int i=0; i<MAX_CLIENTS; i++) {
        if((usedSlots & (1 << i)) == 0) {       // floppy slot i not used, return it
            Debug::out(whichLog, LOG_INFO, "clientsGetFloppySlotIndexForIp - IP not found in table, start using floppySlotIndex: %d", i);
            return i;
        }
    }

    // no empty floppy slot found
    Debug::out(whichLog, LOG_ERROR, "clientsGetFloppySlotIndexForIp - no empty slot found, will return: %d", FD_EMPTY);
    return FD_EMPTY;
}

void ChipInterfaceNetwork::clientsStoreOne(ClientInfo* info, int newSock, uint32_t ipAddr)
{
    info->fdClient = newSock;
    info->lastMs = Utils::getCurrentMs();
    info->floppySlotIndex = clientsGetFloppySlotIndexForIp(ipAddr);
    info->ipAddr = ipAddr;  // store ip after calling clientsGetFloppySlotIndexForIp() so it won't match this same client for the 1st time
}

void ChipInterfaceNetwork::clientsDisconnectInactive(void)
{
    uint32_t now = Utils::getCurrentMs();

    for(int i=0; i<MAX_CLIENTS; i++) {
        if(clients[i].fdClient == FD_EMPTY) {      // no client at this index? skip
            continue;
        }

        // there hasn't been any data from this client for some time? close connection
        uint32_t diff = now - clients[i].lastMs;

        if(diff > 15000) {
            clientsCloseOne(&clients[i]);
            Debug::out(whichLog, LOG_INFO, "disconnected inactive client #%i", i);
        }
    }
}

int ChipInterfaceNetwork::getFdListen(void)
{
    return fdListen;
}

ClientInfo* ChipInterfaceNetwork::clientsGetOne(int clientIndex)
{
    if(clientIndex < 0 || clientIndex >= MAX_CLIENTS) {
        return NULL;
    }

    return &clients[clientIndex];
}

ClientInfo* ChipInterfaceNetwork::clientsGetOneByFloppySlot(int floppySlotIndex)     // get by floppy slot
{
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(clients[i].floppySlotIndex == floppySlotIndex) {     // found matching floppy slot index? return pointer
            return &clients[i];
        }
    }

    return NULL;    // not found, return null
}

int ChipInterfaceNetwork::readRestOfData(int clientIndex, uint8_t* buffer, uint32_t bufferSize)
{
    memset(buffer, 0, bufferSize);
    int readSize = MIN(clients[clientIndex].bufReader.dataSizeRest(), bufferSize);
    return recvFromClient(clientIndex, buffer, readSize);
}
