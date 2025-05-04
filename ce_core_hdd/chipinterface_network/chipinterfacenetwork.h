#ifndef __CHIPINTERFACENETWORK_H__
#define __CHIPINTERFACENETWORK_H__

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../chipinterface.h"
#include "bufferedreader.h"

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
    // if following function returns true, some command is waiting for action in the inBuf and hardNotFloppy flag distiguishes hard-drive or floppy-drive command
    bool actionNeeded(uint8_t *inBuf);

    // to handle FW version, first call setHDDconfig() to fill config into bufOut, then call getFWversion to get the FW version from chip
    void getFWversion(void);

    //----------------
    // HDD: READ/WRITE functions for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above

    bool hdd_sendData_start(uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus);
    bool hdd_sendData_transferBlock(uint8_t *pData, uint32_t dataCount, bool withHeader = true);

    bool hdd_recvData_start(uint8_t *recvBuffer, uint32_t totalDataCount);
    bool hdd_recvData_transferBlock(uint8_t *pData, uint32_t dataCount, bool withHeader = true);

    bool hdd_sendStatusToHans(uint8_t statusByte);

private:
    uint32_t lastTimeRecv;

    int fdListen;       // socket for listen()
    int fdClient;       // socket received on accept()
    int fdReport;       // socket for reporting status to main server thread

    struct sockaddr_in addressListen;
    struct sockaddr_in addressReport;

    uint32_t nextReportTime;    // when should we send next report to main server socket

    uint8_t *bufOut;
    uint8_t *bufIn;

    uint8_t  gotAtnId;      // which chip wants to talk? Franz, Hans?
    uint8_t  gotAtnCode;    // which command code chips sends? 
    BufferedReader bufReader;

    void createListeningSocket(void);
    void acceptSocketIfNeededAndPossible(void);
    void closeClientSocket(void);
    uint32_t recvFromClient(uint8_t* buf, int maxLen);
    void createServerReportSocket(void);
    void sendReportToMainServerSocket(void);

    bool waitForAtn(int atnIdWant, uint8_t atnCode, uint32_t timeoutMs, uint8_t *inBuf);
    void handleZerosAndIkbd(int atnId);

    bool sendHeaderToChip(uint16_t cmdCode, uint32_t futureDatalen);                // send header to chip
    bool sendDataToChip(uint8_t* data, uint32_t len);                               // send data to chip  
    bool sendHeaderAndDataToChip(uint16_t cmdCode, uint8_t* data, uint32_t len);    // send header and data to chip
};

#endif // __CHIPINTERFACENETWORK_H__
