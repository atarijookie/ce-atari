#ifndef CHIPINTERFACE_H
#define CHIPINTERFACE_H

#include "datatypes.h"

// The following commands are sent from device to host on chip interface v1 and v2, 
// but as they are used for command identification in core thread and are reused
// in chip interface v3 (even though that one doesn't really use them), it's moved here.

#define ATN_FW_VERSION                  0x01                                // followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_ACSI_COMMAND                0x02
#define ATN_READ_MORE_DATA              0x03
#define ATN_WRITE_MORE_DATA             0x04
#define ATN_GET_STATUS                  0x05
#define ATN_ANY                         0xff                                // this is used only on host to wait for any ATN

// defines for Floppy part
// commands sent from device to host
#define ATN_FW_VERSION              0x01            // followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_SEND_NEXT_SECTOR        0x02            // sent: 2, side, track #, current sector #, 0, 0, 0, 0 (length: 4 WORDs)
#define ATN_SECTOR_WRITTEN          0x03            // sent: 3, side (highest bit) + track #, current sector #
#define ATN_SEND_TRACK              0x04            // send the whole track

// This class is interface definition for communication with low-level chips.
// The derived object will handle all the low-level chip communication (e.g. via SPI or paralel data port)
// without exposing anything to higher levels of app. This should then simplify writing any other low-level chip interface support.

class ChipInterface
{
public:
    virtual ~ChipInterface() {};

    //----------------
    // chip interface initialization and deinitialization - e.g. open GPIO, or open socket, ...
    virtual bool open(void) = 0;
    virtual void close(void) = 0;

    //----------------
    // call this with true to enable ikdb UART communication
    virtual void ikdbUartEnable(bool enable) = 0;

    //----------------
    // reset both or just one of the parts
    virtual void resetHDDandFDD(void) = 0;
    virtual void resetHDD(void) = 0;
    virtual void resetFDD(void) = 0;

    //----------------
    // if following function returns true, some command is waiting for action in the inBuf and hardNotFloppy flag distiguishes hard-drive or floppy-drive command
    virtual bool actionNeeded(bool &hardNotFloppy, BYTE *inBuf) = 0;

    // to handle FW version, first call setHDDconfig() / setFDDconfig() to fill config into bufOut, then call getFWversion to get the FW version from chip
    virtual void setHDDconfig(BYTE hddEnabledIDs, BYTE sdCardId, BYTE fddEnabledSlots, bool setNewFloppyImageLed, BYTE newFloppyImageLed) = 0;
    virtual void setFDDconfig(bool setFloppyConfig, bool fddEnabled, int id, int writeProtected, bool setDiskChanged, bool diskChanged) = 0;

    virtual void getFWversion(bool hardNotFloppy, BYTE *inFwVer) = 0;

    //----------------
    // HDD: READ/WRITE functions for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above

    virtual bool hdd_sendData_start(DWORD totalDataCount, BYTE scsiStatus, bool withStatus) = 0;
    virtual bool hdd_sendData_transferBlock(BYTE *pData, DWORD dataCount) = 0;

    virtual bool hdd_recvData_start(BYTE *recvBuffer, DWORD totalDataCount) = 0;
    virtual bool hdd_recvData_transferBlock(BYTE *pData, DWORD dataCount) = 0;

    virtual bool hdd_sendStatusToHans(BYTE statusByte) = 0;

    //----------------
    // FDD: all you need for handling the floppy interface
    virtual void fdd_sendTrackToChip(int byteCount, BYTE *encodedTrack) = 0;    // send encodedTrack to chip for MFM streaming
    virtual BYTE* fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount) = 0;
};

#endif // CHIPINTERFACE
