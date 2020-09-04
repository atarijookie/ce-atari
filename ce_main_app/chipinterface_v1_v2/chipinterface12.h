#ifndef CHIPINTERFACE12_H
#define CHIPINTERFACE12_H

#include "../datatypes.h"
#include "../chipinterface.h"
#include "conspi.h"

// SPI interface for connection to Hans (hard drive chip) and Franz (floppy chip).
// Used in CosmosEx v1 and v2.

#define COMMAND_SIZE            10
#define ACSI_CMD_SIZE           14
#define WRITTENMFMSECTOR_SIZE   2048
#define MFM_STREAM_SIZE         13800
#define TX_RX_BUFF_SIZE         600

// commands sent from host to device
#define CMD_ACSI_CONFIG                 0x10
#define CMD_DATA_WRITE                  0x20
#define CMD_DATA_READ_WITH_STATUS       0x30
#define CMD_SEND_STATUS                 0x40
#define CMD_DATA_READ_WITHOUT_STATUS    0x50
#define CMD_FLOPPY_CONFIG               0x70
#define CMD_FLOPPY_SWITCH               0x80
#define CMD_DATA_MARKER                 0xda

// commands sent from host to device
#define CMD_WRITE_PROTECT_OFF       0x10
#define CMD_WRITE_PROTECT_ON        0x20
#define CMD_DISK_CHANGE_OFF         0x30
#define CMD_DISK_CHANGE_ON          0x40
#define CMD_SET_DRIVE_ID_0          0x70
#define CMD_SET_DRIVE_ID_1          0x80
#define CMD_DRIVE_ENABLED           0xa0
#define CMD_DRIVE_DISABLED          0xb0

class ChipInterface12: public ChipInterface
{
public:
    ChipInterface12();
    virtual ~ChipInterface12();

    // this return CHIP_IF_V1_V2 or CHIP_IF_V3
    int chipInterfaceType(void);

    //----------------
    // chip interface initialization and deinitialization - e.g. open GPIO, or open socket, ...
    bool open(void);
    void close(void);

    //----------------
    // call this with true to enable ikdb UART communication
    void ikdbUartEnable(bool enable);

    //----------------
    // reset both or just one of the parts
    void resetHDDandFDD(void);
    void resetHDD(void);
    void resetFDD(void);

    //----------------
    // if following function returns true, some command is waiting for action in the inBuf and hardNotFloppy flag distiguishes hard-drive or floppy-drive command
    bool actionNeeded(bool &hardNotFloppy, BYTE *inBuf);

    // to handle FW version, first call setHDDconfig() / setFDDconfig() to fill config into bufOut, then call getFWversion to get the FW version from chip
    void setHDDconfig(BYTE hddEnabledIDs, BYTE sdCardId, BYTE fddEnabledSlots, bool setNewFloppyImageLed, BYTE newFloppyImageLed);
    void setFDDconfig(bool setFloppyConfig, bool fddEnabled, int id, int writeProtected, bool setDiskChanged, bool diskChanged);

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
    CConSpi *conSpi;

    BYTE *bufOut;
    BYTE *bufIn;

    struct {
        struct {
            WORD acsi;
            WORD fdd;
        } current;

        struct {
            WORD acsi;
            WORD fdd;
        } next;

        bool skipNextSet;
    } hansConfigWords;

    struct {
        int bfrLengthInBytes;
        int currentLength;
    } response;

    void responseStart(int bufferLengthInBytes);        // use this to start creating response (commands) to Hans or Franz
    void responseAddWord(BYTE *bfr, WORD value);        // add a WORD to the response (command) to Hans or Franz
    void responseAddByte(BYTE *bfr, BYTE value);        // add a BYTE to the response (command) to Hans or Franz

    int bcdToInt(int bcd);
    void convertXilinxInfo(BYTE xilinxInfo);            // used to process xilinx info into hwInfo struct
};

#endif // CHIPINTERFACE12_H
