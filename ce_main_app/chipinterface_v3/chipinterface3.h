#ifndef CHIPINTERFACE3_H
#define CHIPINTERFACE3_H

#include <stdint.h>
#include "../chipinterface_v1_v2/gpio.h"
#include "../chipinterface.h"
#include "conspi2.h"

// SPI interface for connection to Horst chip.
// Used in CosmosEx v3.

class ChipInterface3: public ChipInterface
{
public:
    ChipInterface3();
    virtual ~ChipInterface3();

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
    CConSpi2 *conSpi2;

    // got 2 pipes, with 2 ends...
    // pipefd[0] refers to the read end of the pipe.
    // pipefd[1] refers to the write end of the pipe.
    int pipeFromAtariToRPi[2];
    int pipeFromRPiToAtari[2];

    uint8_t *rxDataWOhead;

    void serialSetup(void);                         // open pipes for fake IKDB serial port

    int chipDebugStringsFd;
    void chipDebugStringsSetup(void);               // open UART for debug strings from HW
    void chipDebugStringsHandle(void);              // read from UART, write to chiplog
};

#endif // CHIPINTERFACE3_H
