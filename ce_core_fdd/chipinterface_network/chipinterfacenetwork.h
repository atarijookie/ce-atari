#ifndef __CHIPINTERFACENETWORK_H__
#define __CHIPINTERFACENETWORK_H__

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "chipinterfacenetwork.h"
#include "../settings.h"
#include "bufferedreader.h"

#define SYNC_TAG_FDD    0xc0500fdd

#define MAX_CLIENTS     8

#define MFM_STREAM_SIZE             13800

// defines for Floppy part
// commands sent from device to host
#define ATN_FW_VERSION              0x01            // followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_SECTOR_WRITTEN          0x03            // sent: 3, side (highest bit) + track #, current sector #
#define ATN_SEND_TRACK              0x04            // send the whole track
#define ATN_ANY                     0xff            // this is used only on host to wait for any ATN

// Franz: commands sent from host to device
#define CMD_WRITE_PROTECT_OFF       0x10
#define CMD_WRITE_PROTECT_ON        0x20
#define CMD_DISK_CHANGE_OFF         0x30
#define CMD_DISK_CHANGE_ON          0x40
#define CMD_SET_DRIVE_ID_0          0x70
#define CMD_SET_DRIVE_ID_1          0x80
#define CMD_DRIVE_ENABLED           0xa0
#define CMD_DRIVE_DISABLED          0xb0
#define CMD_FRANZ_SOUND_ON          0xc1        // do floppy seek sound
#define CMD_FRANZ_SOUND_OFF         0xc2        // don't make the floppy seek sound

class ChipInterfaceNetwork
{
public:
    ChipInterfaceNetwork();
    virtual ~ChipInterfaceNetwork();

    //----------------
    // chip interface initialization and deinitialization - e.g. open GPIO, or open socket, ...
    bool ciOpen(void);
    void ciClose(void);

    //----------------
    // if following function returns true, some command is waiting for action in the inBuf and hardNotFloppy flag distiguishes hard-drive or floppy-drive command
    bool actionNeeded(uint8_t *inBuf);

    // to handle FW version, first call setHDDconfig() to fill config into bufOut, then call getFWversion to get the FW version from chip
    void getFWversion(uint8_t *inFwVer);

    //----------------
    // FDD: all you need for handling the floppy interface
    void fdd_sendTrackToChip(int byteCount, uint8_t *encodedTrack);    // send encodedTrack to chip for MFM streaming
    uint8_t* fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount);

    void setFDDconfig(bool setFloppyConfig, FloppyConfig* fddConfig, bool setDiskChanged, bool diskChanged);

private:
    uint32_t lastTimeRecv;

    int fdListen;                       // socket for listen()
    int fdClients[MAX_CLIENTS];         // socket received on accept()
    uint32_t clientLastMs[MAX_CLIENTS]; // value of getCurrentMs() when was last time anything was received from this client

    struct sockaddr_in addressListen;

    uint8_t *bufOut;
    uint8_t *bufIn;

    uint8_t  gotAtnId;      // which chip wants to talk? Franz, Hans?
    uint8_t  gotAtnCode;    // which command code chips sends? 
    BufferedReader bufReader;

    void createListeningSocket(void);
    void acceptSocketIfNeededAndPossible(void);
    void closeClientSocket(void);
    uint32_t recvFromClient(uint8_t* buf, int maxLen);

    int setAllClientFds(fd_set* readfds);
    void handleAllReadyClients(fd_set* readfds);
    int disconnectInactiveClients(void);

    bool waitForAtn(int atnIdWant, uint8_t atnCode, uint32_t timeoutMs, uint8_t *inBuf);

    bool sendHeaderToChip(uint16_t cmdCode, uint32_t futureDatalen);                // send header to chip
    bool sendDataToChip(uint8_t* data, uint32_t len);                               // send data to chip  
    bool sendHeaderAndDataToChip(uint16_t cmdCode, uint8_t* data, uint32_t len);    // send header and data to chip
    void storeHeaderToBuffer(uint16_t cmdCode, uint32_t futureDatalen, uint8_t* buffer);
    int getEmptyClientIndex(void);
};

#endif // __CHIPINTERFACENETWORK_H__
