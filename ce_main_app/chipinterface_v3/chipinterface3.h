#ifndef CHIPINTERFACE3_H
#define CHIPINTERFACE3_H

#include "../datatypes.h"
#include "../chipinterface.h"

// Parallel interface to chip.
// Used in CosmosEx v3.

#if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)
// if compiling for RPi

#include <bcm2835.h>

// JTAG pins programming
#define PIN_TDO         RPI_V2_GPIO_P1_03
#define PIN_TDI         RPI_V2_GPIO_P1_05
#define PIN_TCK         RPI_V2_GPIO_P1_07
#define PIN_TMS         RPI_V2_GPIO_P1_11

// FPGA control pins
#define PIN_DATA_ADDR   RPI_V2_GPIO_P1_13
#define PIN_RW          RPI_V2_GPIO_P1_16
#define PIN_TRIG        RPI_V2_GPIO_P1_36

// FPGA data pins
#define PIN_DATA0       RPI_V2_GPIO_P1_12   // GPIO 18
#define PIN_DATA1       RPI_V2_GPIO_P1_35   // GPIO 19
#define PIN_DATA2       RPI_V2_GPIO_P1_38   // GPIO 20
#define PIN_DATA3       RPI_V2_GPIO_P1_40   // GPIO 21
#define PIN_DATA4       RPI_V2_GPIO_P1_15   // GPIO 22
#define PIN_DATA5       RPI_V2_GPIO_P1_18   // GPIO 24
#define PIN_DATA6       RPI_V2_GPIO_P1_22   // GPIO 25
#define PIN_DATA7       RPI_V2_GPIO_P1_37   // GPIO 26

#define DATA_MASK ((1 << PIN_DATA7) | (1 << PIN_DATA6) | (1 << PIN_DATA5) | (1 << PIN_DATA4) | (1 << PIN_DATA3) | (1 << PIN_DATA2) | (1 << PIN_DATA1) | (1 << PIN_DATA0))

#endif

//--------------------------------------------------------
// the addresses for FPGA interface follow
#define FPGA_ADDR_STATUS                0       // on read  - status byte
#define FPGA_ADDR_CONFIG                0       // on write - config byte

// HDD REGISTERS 
#define FPGA_ADDR_READ_FIFO_CNT         1       // on read  - READ  FIFO count
#define FPGA_ADDR_WRITE_FIFO_CNT        2       // on read  - WRITE FIFO count
#define FPGA_ADDR_WRITE_FIFO_DATA       3       // on read  - WRITE FIFO data 
#define FPGA_ADDR_MODE_DIR_CNT          3       // on write - mode + dir + cnt (config byte)
#define FPGA_ADDR_WRITE_FIFO_DATA2      4       // on read  - WRITE FIFO data (same as read from FPGA_ADDR_WRITE_FIFO_DATA)
#define FPGA_ADDR_READ_FIFO_DATA        4       // on write - READ  FIFO data
#define FPGA_ADDR_STATUS2               5       // on read  - status byte 2nd

#define FPGA_ADDR_TEST_REG              7       // on write - write to test reg, on read - read inverted value of what was written to test reg

// FDD REGISTERS 
#define FPGA_ADDR_SIDE_TRACK            8       // on read  - requested side+track - values based on DIR, STEP and SIDE input signals
#define FPGA_ADDR_READ_MFM_RAM          9       // on write - storing data into MFM RAM - MFM READ stream halted when this address is selected!
#define FPGA_ADDR_STRMD_SIDE_TRACK      10      // on read  - streamed side+track - based on value found in MFM READ stream (exposed for debugging purposes)
#define FPGA_ADDR_STRMD_SECTOR          11      // on read  - streamed sector no. - based on value found in MFM READ stream (exposed for debugging purposes)
#define FPGA_ADDR_WRITE_MFM_FIFO        12      // on read  - WRITE MFM FIFO (written sector data)
#define FPGA_ADDR_WRITE_MFM_FIFO_CNT    13      // on read  - WRITE FIFO count (how many bytes can be read from WRITE MFM FIFO)

// FW INFO
#define FPGA_ADDR_FW_VERSION1           14      // on read  - firmware version 1 <-- b7 is highest bit of day, b6 is free, b5..0 is year
#define FPGA_ADDR_FW_VERSION2           15      // on read  - firmware version 2 <-- b7..4 is month, b3..0 is day (+ highest bit of day is b7 in FW version 1)

//--------------------------------------------------------
// STATUS (at adddress 0) bits definition
#define STATUS_HDD_READ_FIFO_FULL       (1 << 7)        // READ FIFO full   (don't add to READ FIFO when it's full)
#define STATUS_HDD_READ_FIFO_EMPTY      (1 << 6)        // READ FIFO empty  (Atari finished reading all bytes from READ FIFO)
#define STATUS_HDD_WRITE_FIFO_EMPTY     (1 << 5)        // WRITE FIFO empty (we can get byte from WRITE FIFO if it's not empty)
#define STATUS_HDD_ATN_SIGNAL           (1 << 4)        // read the ATN signal to see if we should switch to MSG phase or not
#define STATUS_HDD_HANDSHAKE_IDLE       (1 << 3)        // handshake IDLE - when read or write FIFO is empty, check this signal before changing at_mode

#define STATUS_FDD_SIDE_TRACK_CHANGED   (1 << 2)        // when '1' then track or side changed and we need to put in correct track data
#define STATUS_FDD_WRITE_FIFO_EMPTY     (1 << 1)        // when '0' then some data can be read from floppy write floppy

//--------------------------------------------------------
// STATUS 2 (at adddress 5) bits definition
#define STATUS2_HDD_MODE                ((1 << 3) | (1 << 2))   // current mode (idle / pio / dma / msg)
#define STATUS2_HDD_INTERFACE_TYPE      ((1 << 1) | (1 << 0))   // current if type (acsi / scsi / cart)

#define INTERFACE_TYPE_ACSI             0
#define INTERFACE_TYPE_SCSI             1
#define INTERFACE_TYPE_CART             2

//--------------------------------------------------------
#define WRITTENMFMSECTOR_SIZE   2048

//--------------------------------------------------------
// config values for MODE-DIR-CNT register
#define MODE_MSG        0xC0
#define MODE_DMA        0x80
#define MODE_PIO        0x40
#define MODE_IDLE       0x00

#define DIR_READ        0x20
#define DIR_WRITE       0x00

#define SIZE_IN_SECTORS 0x10
#define SIZE_IN_BYTES   0x00

//--------------------------------------------------------
// states for fdd written sector FSM
#define FDD_WRITE_FIND_HEADER       0   // we're looking for 0xCAFE word
#define FDD_WRITE_GET_SIDE_TRACK    1   // current byte holds side+track
#define FDD_WRITE_GET_SECTOR_NO     2   // current byte holds sector number
#define FDD_WRITE_WRITTEN_DATA      3   // keep storing written data, until 0x00 is found

//--------------------------------------------------------

class ChipInterface3: public ChipInterface
{
public:
    ChipInterface3();
    virtual ~ChipInterface3();

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

private:
    BYTE hddEnabledIDs;
    int  dataPins[8];           // holds which GPIO pins are data pins - used int fpgaDataPortAsInput()

    bool ikbdEnabled;           // if false, the FPGA will not send IKBD data to RPi

    struct {
        bool enabled;           // if true, floppy part is enabled
        bool id1;               // FDD ID0 on false, FDD ID1 on true
        bool writeProtected;    // if true, writes to floppy are ignored
        bool diskChanged;       // if true, disk change is happening
    } fdd;

    bool newTrackRequest;       // true if there was track request which wasn't handled yet
    int fddReqSide;             // FDD requested side (0/1)
    int fddReqTrack;            // FDD requested track
    DWORD fddTrackRequestTime;  // when was FDD track requested last time

    struct {
        BYTE *data;             // holds the written sector data until it's all and ready to be processed
        int index;              // index of position where the next found data should be written

        int state;              // one of the states from FDD_WRITE_..., which define what we're doing now with written data

        int side;
        int track;
        int sector;
    } fddWrittenSector;

    struct {                    // holds info from hdd_sendData_start() and hdd_recvData_start()
        DWORD totalDataCount;
        BYTE scsiStatus;
        bool withStatus;
    } hddTransferInfo;

    BYTE interfaceType;                             // ACSI (0), SCSI (1) or CART (2)

    void fpgaResetAndSetConfig(bool resetHdd, bool resetFdd);   // this function does reset of HDD and FDD part, but also sets the config of HDD and FDD, so we have to construct new config byte and set it when doing reset
    bool readFwVersionFromFpga(BYTE *inFwVer);      // read FPGA version from chip, return true if the values look ok, copy the values into inFwVer

    DWORD timeoutForDataCount(DWORD dataCount);
    bool waitForBusIdle(DWORD maxWaitTime);         // waits until both HDD FIFOs are empty and then until handshake is idle
    int  getCmdLengthFromCmdBytes(BYTE *cmd);       // from the received command bytes determine the final length of the command
    bool getHddCommand(BYTE *inBuf);                // get whole HDD command and request action
    void handleTrackChanged(void);                  // get requested track and side, but don't request action just yet
    bool trackChangedNeedsAction(BYTE *inBuf);      // request action if last track request happened enough time ago
    bool handleFloppyWriteBytes(BYTE *inBuf);       // buffer written data until there is whole sector present, then request action

    // low level functions to do the simplest operations on the FPGA port
    void fpgaDataPortSetDirection(int pinDirection);
    void fpgaAddressSet(BYTE addr, bool force=false);   // set OUTPUT direction, output address to data port, switch to ADDRESS, TRIG the operation
    void fpgaDataWrite(BYTE data);                  // set OUTPUT direction, output data    to data port, switch to DATA,    TRIG the operation
    BYTE fpgaDataRead(void);                        // set INPUT  direction, switch to DATA, TRIG the operation, get the data from data port

    void fpgaDataOutput(BYTE val);
    BYTE fpgaDataInput(void);

    // other helpers
    BYTE intToBcd(int integer);
    int  bitValueTo01(BYTE val);
};

#endif // CHIPINTERFACE3_H
