#ifndef CHIPINTERFACENETWORK_H
#define CHIPINTERFACENETWORK_H

#include "../datatypes.h"
#include "../chipinterface.h"

class ChipInterfaceNetwork: public ChipInterface
{
public:
    ChipInterfaceNetwork();
    virtual ~ChipInterfaceNetwork();

    // as this network server interface will run multiple times, this index will tell it on which port it should run
    void setServerIndex(int index);

    // this return CHIP_IF_V1_V2 or CHIP_IF_V3
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
    bool actionNeeded(bool &hardNotFloppy, BYTE *inBuf);

    // to handle FW version, first call setHDDconfig() / setFDDconfig() to fill config into bufOut, then call getFWversion to get the FW version from chip
    void getFWversion(bool hardNotFloppy, BYTE *inFwVer);

    //----------------
    // HDD: READ/WRITE functions for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above

    bool hdd_sendData_start(DWORD totalDataCount, BYTE scsiStatus, bool withStatus);
    bool hdd_sendData_transferBlock(BYTE *pData, DWORD dataCount);

    bool hdd_recvData_start(BYTE *recvBuffer, DWORD totalDataCount);
    bool hdd_recvData_transferBlock(BYTE *pData, DWORD dataCount);

    bool hdd_sendStatusToHans(BYTE statusByte);

    //----------------
    // FDD: all you need for handling the floppy interface
    void fdd_sendTrackToChip(int byteCount, BYTE *encodedTrack);    // send encodedTrack to chip for MFM streaming
    BYTE* fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount);

    //----------------
private:
    int serverIndex;    // on which server index we're running

    int fdListen;       // socket for listen()
    int fdClient;       // socket received on accept()
    int fdReport;       // socket for reporting status to main server thread

    struct sockaddr_in addressListen;
    struct sockaddr_in addressReport;

    int ikbdReadFd;     // fd used for IKBD read
    int ikbdWriteFd;    // fd used for IKDB write

    uint32_t nextReportTime;    // when should we send next report to main server socket

    BYTE *bufOut;
    BYTE *bufIn;

    void createListeningSocket(void);
    void acceptSocketIfNeededAndPossible(void);
    void closeFdIfOpen(int& sock);
    void createServerReportSocket(void);
    void sendReportToMainServerSocket(void);

    void serialSetup(void);                             // open IKDB serial port
};

#endif // CHIPINTERFACE12_H
