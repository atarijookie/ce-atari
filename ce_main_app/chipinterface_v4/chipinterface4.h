#ifndef __CHIPINTERFACE4_H__
#define __CHIPINTERFACE4_H__

#include <stdint.h>
#include "../chipinterface_v1_v2/chipinterface12.h"
#include "../chipinterface_v1_v2/conspi.h"

// SPI interface for connection to Franz (floppy chip) and ACSI via GPIO.
// Used in CosmosEx v4.

#define BTN_RELEASED    0
#define BTN_PRESSED     1
#define BTN_SHUTDOWN    2

#define IFACE_ACSI      0
#define IFACE_SCSI      1

class GpioAcsi;

class ChipInterface4: public ChipInterface12
{
public:
    ChipInterface4();
    virtual ~ChipInterface4();

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
    // button, beeper and display handling
    void handleButton(int& btnDownTime, uint32_t& nextScreenTime);
    void handleBeeperCommand(int beeperCommand, bool floppySoundEnabled);
    bool handlesDisplay(void);                              // returns true if should handle i2c display from RPi
    void displayBuffer(uint8_t *bfr, uint16_t size);        // send this display buffer data to remote display
    //----------------
private:
    uint8_t* displayData;           // holds display data which should be sent to Franz
    uint16_t displayDataSize;

    bool btnDown;           // current button state, button is pressed (down) if true
    uint8_t recoveryLevel;

    GpioAcsi* gpioAcsi;
    CConSpi *conSpi;

    int ikbdReadFd;     // fd used for IKBD read
    int ikbdWriteFd;    // fd used for IKDB write

    uint8_t *bufOut;
    uint8_t *bufIn;

    void serialSetup(void);                             // open IKDB serial port
    void initButtonAndBeeperPins(void);
    void handleFloppySlotSwitch(void);
    void handleRecoveryButtonPress(int btnDownTime);
    void handleIfaceReport(uint8_t ifaceReport);
};

#endif // __CHIPINTERFACE4_H__
