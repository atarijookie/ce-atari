#ifndef CHIPINTERFACE_H
#define CHIPINTERFACE_H

#include <stdint.h>
#include "settings.h"

// types of chip interface, as returned by 
#define CHIP_IF_DUMMY   -1
#define CHIP_IF_V1_V2   1
#define CHIP_IF_V3      3
#define CHIP_IF_V4      4
#define CHIP_IF_NETWORK 9

// The following commands are sent from device to host on chip interface v1 and v2, 
// but as they are used for command identification in core thread and are reused
// in chip interface v3 (even though that one doesn't really use them), it's moved here.

#define ATN_FW_VERSION                  0x01        // followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_ACSI_COMMAND                0x02
#define ATN_READ_MORE_DATA              0x03
#define ATN_WRITE_MORE_DATA             0x04
#define ATN_GET_STATUS                  0x05
#define ATN_ANY                         0xff        // this is used only on host to wait for any ATN

// defines for Floppy part
// commands sent from device to host
#define ATN_FW_VERSION              0x01            // followed by string with FW version (length: 4 WORDs - cmd, v[0], v[1], 0)
#define ATN_SEND_NEXT_SECTOR        0x02            // sent: 2, side, track #, current sector #, 0, 0, 0, 0 (length: 4 WORDs)
#define ATN_SECTOR_WRITTEN          0x03            // sent: 3, side (highest bit) + track #, current sector #
#define ATN_SEND_TRACK              0x04            // send the whole track

// commands sent from Franz v4 to host
#define ATN_GET_DISPLAY_DATA        0x05            // get current display content

#define COMMAND_SIZE            10
#define ACSI_CMD_SIZE           14
#define WRITTENMFMSECTOR_SIZE   2048
#define MFM_STREAM_SIZE         13800
#define TX_RX_BUFF_SIZE         600

// Hans: commands sent from host to device
#define CMD_ACSI_CONFIG                 0x10
#define CMD_DATA_WRITE                  0x20
#define CMD_DATA_READ_WITH_STATUS       0x30
#define CMD_SEND_STATUS                 0x40
#define CMD_DATA_READ_WITHOUT_STATUS    0x50
#define CMD_FLOPPY_CONFIG               0x70
#define CMD_FLOPPY_SWITCH               0x80
#define CMD_GET_LICENSE                 0xa0
#define CMD_DO_UPDATE                   0xb0
#define CMD_DATA_MARKER                 0xda

// Franz: commands sent from host to device
#define CMD_WRITE_PROTECT_OFF       0x10
#define CMD_WRITE_PROTECT_ON        0x20
#define CMD_DISK_CHANGE_OFF         0x30
#define CMD_DISK_CHANGE_ON          0x40
#define CMD_SET_DRIVE_ID_0          0x70
#define CMD_SET_DRIVE_ID_1          0x80
#define CMD_DRIVE_ENABLED           0xa0
#define CMD_DRIVE_DISABLED          0xb0

// Franz v4: new commands sent from host to device, they will be ignored in older Franz
#define CMD_FRANZ_MODE_1            0xc0        // Franz in v1/v2 mode
#define CMD_FRANZ_MODE_4_SOUND_ON   0xc1        // Franz in v4 mode + do floppy seek sound
#define CMD_FRANZ_MODE_4_SOUND_OFF  0xc2        // Franz in v4 mode + don't make the floppy seek sound
#define CMD_FRANZ_MODE_4_POWER_OFF  0xc5        // turn off the power of this device

#define MAKEWORD(A, B)  ( (((uint16_t)A)<<8) | ((uint16_t)B) )

#define HDD_FW_RESPONSE_LEN     12
#define FDD_FW_RESPONSE_LEN     8

#define FW_RESPONSE_LEN_BIGGER  ((HDD_FW_RESPONSE_LEN > FDD_FW_RESPONSE_LEN) ? HDD_FW_RESPONSE_LEN : FDD_FW_RESPONSE_LEN)

#define INBUF_SIZE  (WRITTENMFMSECTOR_SIZE + 8)

// This class is interface definition for communication with low-level chips.
// The derived object will handle all the low-level chip communication (e.g. via SPI or paralel data port)
// without exposing anything to higher levels of app. This should then simplify writing any other low-level chip interface support.

class ChipInterface
{
public:
    ChipInterface();
    virtual ~ChipInterface() {};

    // this return CHIP_IF_V1_V2 or some other
    virtual int chipInterfaceType(void) = 0;

    void setInstanceIndex(int index);
    int  getInstanceIndex(void);            // -1 for physical interface (only single instance), 0-15 for network interfaces

    //----------------
    // chip interface initialization and deinitialization - e.g. open GPIO, or open socket, ...
    virtual bool ciOpen(void) = 0;
    virtual void ciClose(void) = 0;

    //----------------
    // call this with true to enable ikdb UART communication
    virtual void ikbdUartEnable(bool enable) = 0;
    virtual int  ikbdUartReadFd(void) = 0;
    virtual int  ikbdUartWriteFd(void) = 0;

    //----------------
    // reset both or just one of the parts
    virtual void resetHDDandFDD(void) = 0;
    virtual void resetHDD(void) = 0;
    virtual void resetFDD(void) = 0;

    //----------------
    // if following function returns true, some command is waiting for action in the inBuf and hardNotFloppy flag distiguishes hard-drive or floppy-drive command
    virtual bool actionNeeded(bool &hardNotFloppy, uint8_t *inBuf) = 0;

    // to handle FW version, first call setHDDconfig() / setFDDconfig() to fill config into bufOut, then call getFWversion to get the FW version from chip
    virtual void getFWversion(bool hardNotFloppy, uint8_t *inFwVer) = 0;
    virtual void setHDDconfig(uint8_t hddEnabledIDs, uint8_t sdCardId, uint8_t fddEnabledSlots, bool setNewFloppyImageLed, uint8_t newFloppyImageLed);
    virtual void setFDDconfig(bool setFloppyConfig, FloppyConfig* fddConfig, bool setDiskChanged, bool diskChanged);

    //----------------
    // HDD: READ/WRITE functions for large (>1 MB) block transfers (Scsi::readSectors(), Scsi::writeSectors()) and also by the convenient functions above

    virtual bool hdd_sendData_start(uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus) = 0;
    virtual bool hdd_sendData_transferBlock(uint8_t *pData, uint32_t dataCount) = 0;

    virtual bool hdd_recvData_start(uint8_t *recvBuffer, uint32_t totalDataCount) = 0;
    virtual bool hdd_recvData_transferBlock(uint8_t *pData, uint32_t dataCount) = 0;

    virtual bool hdd_sendStatusToHans(uint8_t statusByte) = 0;

    //----------------
    // FDD: all you need for handling the floppy interface
    virtual void fdd_sendTrackToChip(int byteCount, uint8_t *encodedTrack) = 0;    // send encodedTrack to chip for MFM streaming
    virtual uint8_t* fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount) = 0;

    //----------------
    // button, beeper and display handling
    virtual void handleButton(int& btnDownTime, uint32_t& nextScreenTime) = 0;
    virtual void handleBeeperCommand(int beeperCommand, bool floppySoundEnabled) = 0;
    virtual bool handlesDisplay(void) = 0;                          // returns true if should handle i2c display from RPi
    virtual void displayBuffer(uint8_t *bfr, uint16_t size) = 0;    // send this display buffer data to remote display
    virtual void getDisplayGpioSignals(uint32_t& gpioScl, uint32_t& gpioSda);   // which GPIO pins are used for i2c display

protected:
    uint8_t fwResponseBfr[FW_RESPONSE_LEN_BIGGER];

    struct {
        struct {
            uint16_t acsi;
            uint16_t fdd;
        } current;

        struct {
            uint16_t acsi;
            uint16_t fdd;
        } next;

        bool skipNextSet;
    } hansConfigWords;

    struct {
        int bfrLengthInBytes;
        int currentLength;
    } response;

    bool floppySoundEnabled;
    uint8_t currentFloppyImageLed;

    virtual void responseStart(int bufferLengthInBytes);        // use this to start creating response (commands) to Hans or Franz
    virtual void responseAddWord(uint8_t *bfr, uint16_t value);        // add a uint16_t to the response (command) to Hans or Franz
    virtual bool responseAddByte(uint8_t *bfr, uint8_t value);        // add a uint8_t to the response (command) to Hans or Franz

    static void convertXilinxInfo(uint8_t xilinxInfo);

    int instanceIndex;
};

#endif // CHIPINTERFACE
