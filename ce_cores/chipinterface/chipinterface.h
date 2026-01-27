#ifndef __CHIPINTERFACE_H__
#define __CHIPINTERFACE_H__

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>

#include "chipinterfacedefs.h"
#include "bufferedreader.h"
#include "../misc/settings.h"

typedef struct {
    int         index;
    int         fdClient;           // tcp socket fd
    uint32_t    ipAddr;             // client's IP addr
    uint8_t     mac[6];             // client's mac addr
    uint8_t     features;           // DEV_FEATURE_ bits
    int         floppySlotIndex;    // which floppy slot this IP is using
    uint32_t    lastMs;             // value of getCurrentMs() when was last time anything was received from this client

    BufferedReader bufReader;
} ClientInfo;

class ChipInterface
{
public:
    ChipInterface(int whichLogFile, int whichAtnCode, uint32_t whichSyncTagCode, uint16_t portListen);
    virtual ~ChipInterface();

    // this return CHIP_IF_V1_V2 or some other
    int chipInterfaceType(void);

    //----------------
    // chip interface initialization and deinitialization - e.g. open GPIO, or open socket, ...
    bool ciOpen(void);
    void ciClose(void);

    int getFdListen(void);

    //----------------
    // if following function returns true, some command is waiting for action in the inBuf and hardNotFloppy flag distiguishes hard-drive or floppy-drive command
    bool actionNeeded(ClientInfo* ci, uint8_t *inBuf);
    void dropRestOfData(ClientInfo* ci, uint8_t* buffer, uint32_t bufferSize);

    // to handle FW version
    uint8_t getFWversion(ClientInfo* ci, bool hddNotFdd);
    uint8_t getFWversionHdd(ClientInfo* ci);
    bool getFWversionFdd(ClientInfo* ci);

    //----------------
    // HDD: READ/WRITE functions for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above

    bool hdd_sendData_start(int& fdClient, uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus);
    bool hdd_sendData_transferBlock(int& fdClient, uint8_t *pData, uint32_t dataCount, bool withHeader = true);

    bool hdd_recvData_start(int& fdClient, uint8_t *recvBuffer, uint32_t totalDataCount);
    bool hdd_recvData_transferBlock(int& fdClient, uint8_t *pData, uint32_t dataCount, bool withHeader = true);

    bool hdd_sendStatusToHans(int& fdClient, uint8_t statusByte);

    //----------------
    // FDD: all you need for handling the floppy interface
    void fdd_sendTrackToChip(int& fdClient, int byteCount, uint8_t *encodedTrack);    // send encodedTrack to chip for MFM streaming
    uint8_t* fdd_sectorWritten(ClientInfo* ci, int &side, int &track, int &sector, int &byteCount);
    void fdd_sendImageParamsToChip(int& fdClient, bool finished, int imgTracks, int imgSides, int imgSectorsPerTrack, std::string fileName);

    void setFDDconfig(bool setFloppyConfig, FloppyConfig* fddConfig, bool setDiskChanged, bool diskChanged);

    // the following ones are called from FloppyThread
    void clientsDisconnectInactive(void);
    ClientInfo* clientsGetOne(int clientIndex);                     // get by client index
    ClientInfo* clientsGetOneByFloppySlot(int floppySlotIndex);     // get by floppy slot
    ClientInfo* clientsGetOneByMac(uint8_t* mac);                   // get by mac
    ClientInfo* clientGetByFd(int clientFd);
    ClientInfo* clientGetByIndex(int index);

    int readRestOfData(ClientInfo* ci, uint8_t* buffer, uint32_t bufferSize);
    bool sendHeaderToChip(int& fdClient, uint16_t cmdCode, uint32_t futureDatalen);                // send header to chip
    bool sendDataToChip(int& fdClient, uint8_t* data, uint32_t len);                               // send data to chip
    bool sendHeaderAndDataToChip(int& fdClient, uint16_t cmdCode, uint8_t* data, uint32_t len);    // send header and data to chip
    void storeHeaderToBuffer(uint16_t cmdCode, uint32_t futureDatalen, uint8_t* buffer);

    int setAllClientFds(fd_set* readfds);
    int acceptSocketIfNeededAndPossible(void);

    // ikbd related
    void ikbdUartWriteToAll(uint8_t* bfr, int len);

private:
    int whichLog;
    int whichAtn;
    uint32_t whichSyncTag;
    uint16_t portListen;

    int fdListen;                       // socket for listen()
    ClientInfo clients[MAX_CLIENTS];

    struct sockaddr_in addressListen;

    uint8_t *bufOut;
    uint8_t *bufIn;

    uint8_t  gotAtnId;      // which chip wants to talk? Franz, Hans?
    uint8_t  gotAtnCode;    // which command code chips sends?

    void createListeningSocket(void);
    uint32_t recvFromClient(int& fdClient, uint8_t* buf, int maxLen);

    bool waitForAtn(int clientIndex, int atnIdWant, uint8_t atnCodeWant, uint32_t timeoutMs, uint8_t *inBuf);

    //-----------
    void clientsClearOne(ClientInfo* info);     // clear data structure of one clients
    void clientsClearAll(void);                 // clear data structure of all clients
    void clientsCloseOne(ClientInfo* info);     // closes socket if open, then does clientsClearOne()
    int  clientsGetEmptyIndex(void);
    int  clientsGetFloppySlotIndexForIp(uint32_t ipAddr);
    void clientsStoreOne(ClientInfo* info, int newSock, uint32_t ipAddr, bool isFdd);

    void storeDeviceFeatures(uint8_t* mac, uint8_t featureBits);
};

#endif
