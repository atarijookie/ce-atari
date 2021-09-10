#ifndef __CHIPINTERFACENETWORK_H__
#define __CHIPINTERFACENETWORK_H__

#include <stdint.h>
#include "../chipinterface.h"
#include "bufferedreader.h"

// tags that are being sent from RPi to chip to mark start of data
#define NET_TAG_HANS_STR    "TGHA"
#define NET_TAG_FRANZ_STR   "TGFR"
#define NET_TAG_IKBD_STR    "TGIK"
#define NET_TAG_ZEROS_STR   "TGZE"

class ChipInterfaceNetwork: public ChipInterface
{
public:
    ChipInterfaceNetwork();
    virtual ~ChipInterfaceNetwork();

    // this return CHIP_IF_V1_V2 or some other
    int chipInterfaceType(void);

    //----------------
    // chip interface initialization and deinitialization - e.g. open GPIO, or open socket, ...
    bool ciOpen(void);
    void ciClose(void);

    //----------------
    // call this with true to enable ikdb UART communication
    void ikbdUartEnable(bool enable);
    int  ikbdUartReadFd(void);
    int  ikbdUartWriteFd(void);

    //----------------
    // reset both or just one of the parts
    void resetHDDandFDD(void);
    void resetHDD(void);
    void resetFDD(void);

    //----------------
    // if following function returns true, some command is waiting for action in the inBuf and hardNotFloppy flag distiguishes hard-drive or floppy-drive command
    bool actionNeeded(bool &hardNotFloppy, uint8_t *inBuf);

    // to handle FW version, first call setHDDconfig() / setFDDconfig() to fill config into bufOut, then call getFWversion to get the FW version from chip
    void getFWversion(bool hardNotFloppy, uint8_t *inFwVer);

    //----------------
    // HDD: READ/WRITE functions for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above

    bool hdd_sendData_start(uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus);
    bool hdd_sendData_transferBlock(uint8_t *pData, uint32_t dataCount);

    bool hdd_recvData_start(uint8_t *recvBuffer, uint32_t totalDataCount);
    bool hdd_recvData_transferBlock(uint8_t *pData, uint32_t dataCount);

    bool hdd_sendStatusToHans(uint8_t statusByte);

    //----------------
    // FDD: all you need for handling the floppy interface
    void fdd_sendTrackToChip(int byteCount, uint8_t *encodedTrack);    // send encodedTrack to chip for MFM streaming
    uint8_t* fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount);

    //----------------
private:
    uint32_t lastTimeRecv;
    int serverIndex;    // on which server index we're running

    int fdListen;       // socket for listen()
    int fdClient;       // socket received on accept()
    int fdReport;       // socket for reporting status to main server thread

    struct sockaddr_in addressListen;
    struct sockaddr_in addressReport;

    int ikbdReadFd;     // fd used for IKBD read
    int ikbdWriteFd;    // fd used for IKDB write

    // got 2 pipes, with 2 ends...
    // pipefd[0] refers to the read end of the pipe.
    // pipefd[1] refers to the write end of the pipe.
    int pipeFromAtariToRPi[2];
    int pipeFromRPiToAtari[2];

    uint32_t nextReportTime;    // when should we send next report to main server socket

    uint8_t *bufOut;
    uint8_t *bufIn;

    uint8_t  gotAtnId;      // which chip wants to talk? Franz, Hans?
    uint8_t  gotAtnCode;    // which command code chips sends? 
    BufferedReader bufReader;

    void createListeningSocket(void);
    void acceptSocketIfNeededAndPossible(void);
    void closeClientSocket(void);
    int  recvFromClient(uint8_t* buf, int len, bool byteSwap=true);
    void closeFdIfOpen(int& sock);
    void createServerReportSocket(void);
    void sendReportToMainServerSocket(void);

    bool waitForAtn(int atnIdWant, uint8_t atnCode, uint32_t timeoutMs, uint8_t *inBuf);
    void handleZerosAndIkbd(int atnId);

    void serialSetup(void);                             // open IKDB serial port
    void sendIkbdDataToAtari(void);

    void sendDataToChip(const char* tag, uint8_t* data, uint16_t len);    // send data to chip with specified tag
    void byteSwapBfr(uint8_t* buf, int len);
};

#endif // __CHIPINTERFACENETWORK_H__
