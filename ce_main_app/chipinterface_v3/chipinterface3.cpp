#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)
    #include <bcm2835.h>
#endif

#include "chipinterface3.h"
#include "../chipinterface_v1_v2/gpio.h"
#include "../utils.h"
#include "../debug.h"

ChipInterface3::ChipInterface3()
{
    ikbdEnabled = false;        // ikbd is not enabled by default, so we need to enable it

    fddConfig.enabled = true;           // if true, floppy part is enabled
    fddConfig.id1 = false;              // FDD ID0 on false, FDD ID1 on true
    fddConfig.writeProtected = false;   // if true, writes to floppy are ignored
    fddConfig.diskChanged = false;      // if true, disk change is happening

    fddNewTrack.side = 0;           // FDD requested side (0/1)
    fddNewTrack.track = 0;          // FDD requested track

    fddWrittenSector.data = new BYTE[WRITTENMFMSECTOR_SIZE];    // holds the written sector data until it's all and ready to be processed
    fddWrittenSector.index = 0;                                 // index at which the sector data should be stored
    fddWrittenSector.state = FDD_WRITE_FIND_HEADER;             // start by looking for header

    hddTransferInfo.totalDataCount = 0;
    hddTransferInfo.scsiStatus = 0;
    hddTransferInfo.withStatus = true;

    int dataPinsInit[8] = {PIN_DATA0, PIN_DATA1, PIN_DATA2, PIN_DATA3, PIN_DATA4, PIN_DATA5, PIN_DATA6, PIN_DATA7};

    for(int i=0; i<8; i++) {        // copy data pin numbers from this init array to member array, so we can reuse them without recreating them
        dataPins[i] = dataPinsInit[i];
    }
}

ChipInterface3::~ChipInterface3()
{
    delete []fddWrittenSector.data;
}

int ChipInterface3::chipInterfaceType(void)
{
    return CHIP_IF_V3;
}

bool ChipInterface3::open(void)
{
    #if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)

    if(geteuid() != 0) {
        Debug::out(LOG_ERROR, "The bcm2835 library requires to be run as root, try again...");
        printf("\nThe bcm2835 library requires to be run as root, try again...\n");
        return false;
    }

    // try to init the GPIO library
    if (!bcm2835_init()) {
        Debug::out(LOG_ERROR, "bcm2835_init failed, can't use GPIO.");
        printf("\nbcm2835_init failed, can't use GPIO.\n");
        return false;
    }

    // JTAG pins 
    bcm2835_gpio_fsel(PIN_TDI,  BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_TMS,  BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_TCK,  BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_TDO,  BCM2835_GPIO_FSEL_INPT);

    bcm2835_gpio_write(PIN_TDI, LOW);
    bcm2835_gpio_write(PIN_TMS, LOW);
    bcm2835_gpio_write(PIN_TCK, LOW);

    // FPGA control pins
    bcm2835_gpio_fsel(PIN_DATA_ADDR, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_RW,        BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_TRIG,      BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_STATUS,    BCM2835_GPIO_FSEL_INPT);

    bcm2835_gpio_write(PIN_DATA_ADDR, LOW);
    bcm2835_gpio_write(PIN_RW,        LOW);
    bcm2835_gpio_write(PIN_TRIG,      LOW);

    // FPGA data pins as inputs
    fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_INPT);

    //-----------------------------------------
    // test the connection to FPGA
    fpgaAddressSet(FPGA_ADDR_TEST_REG);     // set the address of test register

    BYTE testValues[6] = {0x00, 0xff, 0xaa, 0x55, 0x0f, 0xf0};
    bool good = true;

    for(int i=0; i<6; i++) {
        fpgaDataWrite(testValues[i]);        // write value

        BYTE response = fpgaDataRead();     // read value back
        BYTE expected = ~testValues[i];      // inverted value is expected

        if(response != expected) {          // response is different from what was expected, write to log, fail the open() function
            Debug::out(LOG_ERROR, "ChipInterface3::open() -- connection test - write: 0x%02X, read: 0x%02X, expected: 0x%02X", testValues[i], response, expected);
            good = false;
        }
    }

    if(!good) {                             // some test byte failed?
        Debug::out(LOG_ERROR, "Connection / chip test failed. FPGA might be not programmed yet, or there's a bad connection between RPi and FPGA.");
        printf("\nConnection / chip test failed. FPGA might be not programmed yet, or there's a bad connection between RPi and FPGA.\n");
        return false;
    }

    BYTE fwVer[4];
    good = readFwVersionFromFpga(fwVer);    // read FW version from chip, check the year-month-day values

    if(!good) {                             // failed to get valid FPGA version?
        return false;
    }

    #endif

    return true;
}

void ChipInterface3::close(void)
{
    #if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)

    fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_INPT);   // FPGA data pins as inputs

    bcm2835_gpio_fsel(PIN_TDI, BCM2835_GPIO_FSEL_INPT); // JTAG pins as inputs to not interfere with programmer
    bcm2835_gpio_fsel(PIN_TMS, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(PIN_TCK, BCM2835_GPIO_FSEL_INPT);
    bcm2835_gpio_fsel(PIN_TDO, BCM2835_GPIO_FSEL_INPT);

    bcm2835_close();                                    // close the GPIO library and finish

    #endif
}

void ChipInterface3::ikdbUartEnable(bool enable)        // enable or disable IKDB routing to RPi
{
    ikbdEnabled = enable;
    fpgaResetAndSetConfig(false, false);                // not doing reset of HDD or FDD, but enabling / disabling IKBD
}

void ChipInterface3::resetHDDandFDD(void)               // reset both HDD and FDD
{
    fpgaResetAndSetConfig(true, true);
}

void ChipInterface3::resetHDD(void)                     // reset just HDD
{
    fpgaResetAndSetConfig(true, false);
}

void ChipInterface3::resetFDD(void)                     // reset just FDD
{
    fpgaResetAndSetConfig(false, true);
}

// this function does reset of HDD and FDD part, but also sets the config of HDD and FDD, so we have to construct new config byte and set it when doing reset
void ChipInterface3::fpgaResetAndSetConfig(bool resetHdd, bool resetFdd)
{
    BYTE config = 0;
    config |= resetHdd          ? (1 << 7) : 0;         // for HDD reset - set bit 7
    config |= resetFdd          ? (1 << 6) : 0;         // for FDD reset - set bit 6

    config |= fddConfig.enabled        ? (1 << 5) : 0;         // if true, floppy part is enabled
    config |= fddConfig.id1            ? (1 << 4) : 0;         // FDD ID0 on false, FDD ID1 on true
    config |= fddConfig.writeProtected ? (1 << 3) : 0;         // if true, writes to floppy are ignored
    config |= fddConfig.diskChanged    ? (1 << 2) : 0;         // if true, disk change is happening

    config |= ikbdEnabled       ? 0        : (1 << 0);  // for disabling IKDB - set bit 0

    fpgaAddressSet(FPGA_ADDR_CONFIG);                   // set the address
    fpgaDataWrite(config);                              // write config data to config register
}

const char *ChipInterface3::atModeString(int at_mode)
{
    switch(at_mode) {
        case AT_MODE_MSG:   return "AT_MODE_MSG";
        case AT_MODE_DMA:   return "AT_MODE_DMA";
        case AT_MODE_PIO:   return "AT_MODE_PIO";
        case AT_MODE_IDLE:  return "AT_MODE_IDLE";
        default:            return "AT_MODE_????";
    }
}

const char *ChipInterface3::interfaceTypeString(int iface)
{
    switch(iface) {
        case INTERFACE_TYPE_ACSI:   return "INTERFACE_TYPE_ACSI";
        case INTERFACE_TYPE_SCSI:   return "INTERFACE_TYPE_SCSI";
        case INTERFACE_TYPE_CART:   return "INTERFACE_TYPE_CART";
        default:                    return "INTERFACE_TYPE_????";
    }
}

bool ChipInterface3::actionNeeded(bool &hardNotFloppy, BYTE *inBuf)
{
    static DWORD lastFirmwareVersionReportTime = 0;         // this holds the time when we last checked for selected interface type. We don't need to do this often.
    static bool reportHddVersion = false;                   // when true, will report HDD FW version
    static bool reportFddVersion = false;                   // when true, will report HDD FW version

    BYTE status;
    DWORD now = Utils::getCurrentMs();
    bool actionNeeded;

    fpgaAddressSet(FPGA_ADDR_STATUS2);                      // set status 2 registess
    status = fpgaDataRead();                                // read the status 2
    BYTE at_mode = (status & STATUS2_HDD_MODE) >> 2;        // bits 3..2 are AT_MODE
    interfaceType = status & STATUS2_HDD_INTERFACE_TYPE;    // bits 1..0 are interface type

    if(at_mode != AT_MODE_IDLE) {                           // if we're in this main situation handled, but the HDD interface mode is not IDLE (= waiting for 0th cmd byte)
        //Debug::out(LOG_DEBUG, "actionNeeded() - AT_MODE was %s (%d), doing resetHDD()", atModeString(at_mode), at_mode);
        resetHDD();
    }

    if((now - lastFirmwareVersionReportTime) >= 1000) {     // if at least 1 second passed since we've pretended we got firmware version from chip
        lastFirmwareVersionReportTime = now;

        reportHddVersion = true;                            // report both HDD and FDD FW versions
        reportFddVersion = true;
    }

    if(reportHddVersion) {                                  // if should report FW version
        reportHddVersion = false;
        inBuf[3] = ATN_FW_VERSION;
        hardNotFloppy = true;                               // HDD needs action
        return true;                                        // action needs handling, FW version will be retrieved via getFWversion() function
    }

    if(reportFddVersion) {                                  // if should report FW version
        reportFddVersion = false;
        inBuf[3] = ATN_FW_VERSION;
        hardNotFloppy = false;                              // FDD needs action
        return true;                                        // action needs handling, FW version will be retrieved via getFWversion() function
    }

    //------------------------------------
    fpgaAddressSet(FPGA_ADDR_STATUS);                   // set status register address
    status = fpgaDataRead();                            // read the status

    if((status & STATUS_HDD_WRITE_FIFO_EMPTY) == 0) {   // if WRITE FIFO is not empty (flag is 0), then ST has sent us CMD byte
        //Debug::out(LOG_DEBUG, "HDD FIFO NOT EMPTY");
        actionNeeded = getHddCommand(inBuf);            // if this command was for one of our enabled HDD IDs, then this will return true and will get handled

        if(actionNeeded) {                              // action is needed
            hardNotFloppy = true;                       // HDD needs action
            return true;
        }
    }

    //----------------------------------------------------
    // FDD READ handling of action needed
    actionNeeded = false;                               // not needing action yet

    if(status & STATUS_FDD_SIDE_TRACK_CHANGED) {        // if floppy track or side changed, we need to send new track data
        readRequestedTrackAndSideFromFPGA();            // get requested track and side from FPGA
        storeRequestedTrackAndSideToInBuf(inBuf);       // store requested track and side into inBuf
        actionNeeded = true;
    }

    if(fddConfig.diskChanged) {                         // if disk changed, we should send new track to FPGA
        fddConfig.diskChanged = false;                  // reset this flag

        Debug::out(LOG_DEBUG, "trackChangedNeedsAction() - disk changed, forcing loading of new track data");

        storeRequestedTrackAndSideToInBuf(inBuf);       // store the PREVIOUSLY requested track and side into inBuf to force new floppy load
        actionNeeded = true;
    }

    if(actionNeeded) {                                  // if FPGA requested new track data or diskChanged forced track data reload
        hardNotFloppy = false;                          // FDD needs action
        return true;
    }

    //----------------------------------------------------
    // FDD WRITE handling of action needed

    if((status & STATUS_FDD_WRITE_FIFO_EMPTY) == 0) {   // WRITE FIFO NOT EMPTY, so some floppy data was written
        actionNeeded = handleFloppyWriteBytes(inBuf);   // get written data and store them in buffer

        if(actionNeeded) {                              // if got whole written sector, quit and request action, otherwise continue with the rest
            hardNotFloppy = false;                      // FDD needs action
            return true;
        }
    }

    // no action needed
    return false;
}

int ChipInterface3::getCmdLengthFromCmdBytes(BYTE *cmd)
{
    BYTE justCmd = cmd[0] & 0x1f;
    BYTE opcode;

    if(interfaceType == INTERFACE_TYPE_ACSI) {          // ACSI interface
        if(justCmd != 0x1f) {                           // not ICD command - it's 6 bytes long
            return 6;
        } else {                                        // it's ICD command - get command opcode from cmd[1]
            opcode = (cmd[1] & 0xe0) >> 5;
        }
    } else if(interfaceType == INTERFACE_TYPE_SCSI) {   // SCSI interface
        opcode = (cmd[0] & 0xe0) >> 5;                  // get command opcode from cmd[0]
    } else {                                            // other interfaces - always 6 for now
        return 6;
    }

    int cmdLen;
    switch(opcode) {                                     // get the length of the command from opcode
        case  0: cmdLen =  6; break;
        case  1: cmdLen = 10; break;
        case  2: cmdLen = 10; break;
        case  4: cmdLen = 16; break;
        case  5: cmdLen = 12; break;
        default: cmdLen =  6; break;
    }

    if(interfaceType == INTERFACE_TYPE_ACSI) {          // when this is ACSI interface, it's 1 byte longer as there's the additional ICD command on start
        cmdLen++;
    }

    return cmdLen;
}

bool ChipInterface3::getHddCommand(BYTE *inBuf)
{
    // HDD WRITE FIFO not empty, so we can read 0th cmd byte
    BYTE *cmd = inBuf + 8;                              // this is where we will store cmd
    memset(cmd, 0, 17);                                 // clear place for cmd

    BYTE status;
    DWORD start = Utils::getCurrentMs();
    DWORD now;

    fpgaAddressSet(FPGA_ADDR_WRITE_FIFO_DATA);          // set address of WRITE FIFO
    cmd[0] = fpgaDataRead();                            // read the 0th cmd byte or SCSI selection bits
    BYTE id, i;
    int storeIdx;

    //Debug::out(LOG_DEBUG, "interfaceType: %s", interfaceTypeString(interfaceType));
    //Debug::out(LOG_DEBUG, "cmd[0]: %02x", cmd[0]);

    if(interfaceType == INTERFACE_TYPE_ACSI) {          // for ACSI
        id = (cmd[0] >> 5) & 0x07;                      // get only device ID
        storeIdx = 1;                                   // store next cmd byte to index 1

        if((hddEnabledIDs & (1 << id)) == 0) {          // ID not enabled? no further action needed
            //Debug::out(LOG_DEBUG, "id %d not enabled. hddEnabledIDs: %02x", id, hddEnabledIDs);
            return false;
        }
    } else if(interfaceType == INTERFACE_TYPE_SCSI) {   // for SCSI
        id = 0xff;                                      // mark that ID hasn't been found yet

        for(i=0; i<8; i++) {
            int bitMask = (1 << i);

            if((cmd[0] & bitMask) && (hddEnabledIDs & bitMask)) {   // if ID bit is one and it's in enabled ID, this ID is selected 
                id = i;                                 // store this ID and quit loop
                break;
            }
        }

        if(id < 0 || id > 7) {                          // ID not found or ID invalid? quit
            return false;
        }

        storeIdx = 0;                                   // store next cmd byte to index 0, as this cmd[0] was selection bits, not really cmd[0]
    } else {                                            // other interfaces? not handling it
        return false;
    }

    //-----------------------------------
    // id now contains found ID of enabled device

    // we want to set that we should read additional 1 cmd byte, after which we can determine the real length of the command
    fpgaAddressSet(FPGA_ADDR_MODE_DIR_CNT);                     // set address MODE-DIR-CNT config reg
    fpgaDataWrite(MODE_PIO | DIR_WRITE | SIZE_IN_BYTES | 0);    // PIO WRITE 1 BYTE (set 0 bytes because of internal cnt+1)

    fpgaAddressSet(FPGA_ADDR_STATUS);                       // set status register address

    while(true) {                                           // while app is running
        if(sigintReceived) {                                // app terminated, quit now, no action
            return false;
        }

        now = Utils::getCurrentMs();

        if((now - start) >= 1000) {                         // if his is taking too long
            //Debug::out(LOG_DEBUG, "timeout on cmd[1]");
            fpgaResetAndSetConfig(true, false);             // reset HDD part
            return false;                                   // fail with no action needed
        }

        status = fpgaDataRead();                            // read the status

        if((status & STATUS_HDD_WRITE_FIFO_EMPTY) == 0) {   // if WRITE FIFO is not empty (flag is 0), then ST has sent us CMD byte
            break;
        }
    }

    fpgaAddressSet(FPGA_ADDR_WRITE_FIFO_DATA);          // set address of WRITE FIFO
    cmd[storeIdx] = fpgaDataRead();                     // read next cmd byte

    int cmdLen = 6;
    cmdLen = getCmdLengthFromCmdBytes(cmd);             // figure out the cmd length

    //Debug::out(LOG_DEBUG, "cmd: %02X %02X - cmdLen: %d", cmd[0], cmd[1], cmdLen);

    int gotBytes;                                       // how many bytes we already have

    if(interfaceType == INTERFACE_TYPE_ACSI) {          // on ACSI interface
        storeIdx++;                                     // store to next index

        gotBytes = 2;                                   // already have 2 cmd bytes (0th and 1st)
    } else if(interfaceType == INTERFACE_TYPE_SCSI) {   // on SCSI interface
        if(cmdLen > 6) {                                // for longer command we need to fake the ICD long format command
            cmd[1] = cmd[0];                            // 0th cmd byte to 1st index
            cmd[0] = 0x1f | (id << 5);                  // add ID on the top 3 bits
            storeIdx = 2;                               // store to index 2
        } else {                                        // SCSI short command
            cmd[0] = cmd[0] | (id << 5);                // add SCSI ID on top of 0th cmd byte
            storeIdx++;                                 // store to next index
        }

        gotBytes = 1;                                   // only got 1 cmd byte (0th)
    }

    int needBytes = cmdLen - gotBytes;                  // how many more bytes we need - e.g. 6 - 2 = 4, or 12 - 1 = 11

    //Debug::out(LOG_DEBUG, "needBytes: %d", needBytes);

    fpgaAddressSet(FPGA_ADDR_MODE_DIR_CNT);                                 // set address MODE-DIR-CNT config reg
    fpgaDataWrite(MODE_PIO | DIR_WRITE | SIZE_IN_BYTES | (needBytes - 1));  // PIO WRITE needBytes-1 BYTEs (set to 1 less bytes because of internal cnt+1)

    //---------------------------------------
    // now the FPGA should read all the needed bytes into FIFO
    // wait until the WRITE FIFO has all the remaining cmd bytes

    fpgaAddressSet(FPGA_ADDR_WRITE_FIFO_CNT);           // set address of byte count in WRITE FIFO

    while(true) {                                       // while app is running
        if(sigintReceived) {                            // app terminated, quit now, no action
            return false;
        }

        now = Utils::getCurrentMs();

        if((now - start) >= 1000) {                     // if his is taking too long
            //Debug::out(LOG_DEBUG, "timeout on waiting for rest of cmd");
            fpgaResetAndSetConfig(true, false);         // reset HDD part
            return false;                               // fail with no action needed
        }

        int gotCount = fpgaDataRead();                  // read how many bytes we already got

        if(gotCount == needBytes) {                     // if got all the needed bytes, quit this waiting loop
            break;
        }
    }

    //---------------------------------------
    // read out the cmd bytes from WRITE FIFO
    fpgaAddressSet(FPGA_ADDR_WRITE_FIFO_DATA);          // set address of WRITE FIFO

    for(i=gotBytes; i<cmdLen; i++) {                    //
        cmd[storeIdx] = fpgaDataRead();                 // read another cmd byte from FIFO
        storeIdx++;
    }

    //Debug::out(LOG_DEBUG, "cmd: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9], cmd[10], cmd[11]);

    inBuf[3] = ATN_ACSI_COMMAND;
    return true;                                        // this command was for enabled ID, so it needs handling
}

void ChipInterface3::readRequestedTrackAndSideFromFPGA(void)
{
    fpgaAddressSet(FPGA_ADDR_SIDE_TRACK);               // set REQUESTED SITE/TRACK register address - this also resets the internal READ MFM RAM address to 0, so we can then just write the new READ MFM RAM data from address 0
    BYTE sideTrack = fpgaDataRead();                    // read the status

    fddNewTrack.side = (sideTrack >> 7) & 1;            // FDD requested side (0/1) - bit 7
    fddNewTrack.track = sideTrack & 0x7f;               // FDD requested track - bits 6..0

    //Debug::out(LOG_DEBUG, "ChipInterface3::readRequestedTrackAndSideFromFPGA() - requested new track data - track %d, side: %d", fddNewTrack.track, fddNewTrack.side);
}

void ChipInterface3::storeRequestedTrackAndSideToInBuf(BYTE *inBuf)
{
    //Debug::out(LOG_DEBUG, "ChipInterface3::storeRequestedTrackAndSideToInBuf() - storing track  request into inBuf - track %d, side: %d", fddNewTrack.track, fddNewTrack.side);

    // NOTE: even if requested track and side are the same as last time, don't ignore that request as it might be an attempt to re-read the sector, which failed to read a while ago

    inBuf[3] = ATN_SEND_TRACK;          // attention code
    inBuf[8] = fddNewTrack.side;        // fdd side
    inBuf[9] = fddNewTrack.track;       // fdd track
}

// Call this when there are some bytes written to floppy. When it's not enough data for whole sector, then this just stores the bytes and returns false.
// If enough data was received for the whole floppy sector, this returns true and later the data will be retrieved using fdd_sectorWritten() function.
bool ChipInterface3::handleFloppyWriteBytes(BYTE *inBuf)
{
    #if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)

    static WORD header = 0;                             // read new byte in the lower part, got header if it contains 0xCAFE

    fpgaAddressSet(FPGA_ADDR_WRITE_MFM_FIFO);           // set address of MFM WRITE FIFO data register
    fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_INPT);   // address + PIN_RW will configure PIN_STATUS as WRITE_MFM_FIFO EMPTY pin

    while(true) {
        int status = bcm2835_gpio_lev(PIN_STATUS);      // read the status bit - MFM WRITE FIFO EMPTY

        if(status == HIGH) {                            // FIFO empty? quit
            return false;
        }

        BYTE val = fpgaDataRead();                  // read first value in FIFO

        switch(fddWrittenSector.state) {            // do stuff depending on FSM state
            case FDD_WRITE_FIND_HEADER: {           // state: find 0xCAFE header
                header = header << 8;
                header |= val;                      // add new byte to bottom

                if(header == 0xCAFE) {              // if header found
                    fddWrittenSector.index = 0;     // where the next data should go

                    fddWrittenSector.state = FDD_WRITE_GET_SIDE_TRACK;  // go to next state
                    header = 0;                     // clear the header so it won't get falsy detected next time
                }

                break;
            };
            //------------------------------
            case FDD_WRITE_GET_SIDE_TRACK: {                        // state: store side and track number
                fddWrittenSector.side = (val >> 7) & 1;             // highest bit is side
                fddWrittenSector.track = val       & 0x7f;          // bits 6..0 are track number

                fddWrittenSector.state = FDD_WRITE_GET_SECTOR_NO;   // go to next state
                break;
            };
            //------------------------------
            case FDD_WRITE_GET_SECTOR_NO: {                         // state: store sector number
                fddWrittenSector.sector = val & 0x0f;               // bits 3..0 are sector number

                fddWrittenSector.state = FDD_WRITE_WRITTEN_DATA;    // go to next state
                break;
            };
            //------------------------------
            case FDD_WRITE_WRITTEN_DATA: {                          // state: store written data until end found
                if(val != 0) {          // if it's not the end of stream, store value, advance in array
                    fddWrittenSector.data[ fddWrittenSector.index ] = val;
                    fddWrittenSector.index++;
                } else {                // end of stream found!
                    fddWrittenSector.state = FDD_WRITE_FIND_HEADER; // next state - start of the FSM all over again to find next written sector

                    // got whole sector, request action. Data will be accessed via fdd_sectorWritten() function.
                    inBuf[3] = ATN_SECTOR_WRITTEN;
                    return true;                                    // action needs handling
                }

                break;
            };
        }
    }

    #endif

    // if somehow magically ended up here, we don't have the full written sector yet
    return false;
}

void ChipInterface3::setHDDconfig(BYTE hddEnabledIDs, BYTE sdCardId, BYTE fddEnabledSlots, bool setNewFloppyImageLed, BYTE newFloppyImageLed)
{
    // store only HDD enabled IDs, don't care about the rest
    this->hddEnabledIDs = hddEnabledIDs;
}

void ChipInterface3::setFDDconfig(bool setFloppyConfig, bool fddEnabled, int id, int writeProtected, bool setDiskChanged, bool diskChanged)
{
    bool configChanged = false;                                 // nothing changed yet

    if(setFloppyConfig) {
        configChanged |= (fddConfig.enabled != fddEnabled);           // if this config changed then set configChanged flag
        fddConfig.enabled = fddEnabled;

        bool newFddId1 = (id == 1);
        configChanged |= (fddConfig.id1 != newFddId1);                // if this config changed then set configChanged flag
        fddConfig.id1 = newFddId1;

        configChanged |= (fddConfig.writeProtected != writeProtected); // if this config changed then set configChanged flag
        fddConfig.writeProtected = writeProtected;
    }

    if(setDiskChanged) {
        configChanged |= (fddConfig.diskChanged != diskChanged);      // if this config changed then set configChanged flag
        fddConfig.diskChanged = diskChanged;
    }

    if(configChanged) {                                 // set new config only if something changed
        fpgaResetAndSetConfig(false, false);            // not doing reset of HDD or FDD, but changing config
    }
}

BYTE ChipInterface3::intToBcd(int integer)
{
    int a, b;
    a = integer / 10;
    b = integer % 10;

    BYTE bcd = (a << 4) | b;
    return bcd;
}

bool ChipInterface3::readFwVersionFromFpga(BYTE *inFwVer)
{
    static BYTE fwVer[4];                           // this should hold FW version prepared to be copied to inFwVer
    static bool good = true;
    static bool didReadFirmwareVersion = false;     // true if at least once was the firmware version read from chip

    if(!didReadFirmwareVersion) {                   // if didn't read FW version before, do it now
        didReadFirmwareVersion = true;

        fpgaAddressSet(FPGA_ADDR_FW_VERSION1);      // set addr, get FW 1
        BYTE fw1 = fpgaDataRead();

        fpgaAddressSet(FPGA_ADDR_FW_VERSION2);      // set addr, get FW 2
        BYTE fw2 = fpgaDataRead();

        int year = fw1 & 0x3F;                      // year on bits 5..0 (0..63)

        int month = (fw2 & 0xF0) >> 4;              // month on bits 7..4

        int day   = (fw2 & 0x0F);                   // day on bits 3..0 (and bit 4 of day is bit 7 in FW version number 1)

        if(fw1 & 0x80) {                            // bit 4 of day is bit 7 in FW version number 1
            day |= 0x10;
        }

        if(year < 20 || month < 1 || month > 12 || day < 1 || day > 31) {   // if year is less than 2020, or month not from <1, 12>, or day not from <1, 31>
            good = false;
            Debug::out(LOG_DEBUG, "FW1: %02X, FW2: %02X", fw1, fw2);
            Debug::out(LOG_ERROR, "FPGA version seems to be wrong: %d-%02d-%02d . Maybe FPGA needs to be reflashed?", year + 2000, month, day);
            printf("\nFPGA version seems to be wrong: %d-%02d-%02d\nMaybe FPGA needs to be reflashed?\n", year + 2000, month, day);
        }

        fwVer[0] = 0;
        fwVer[1] = intToBcd(year);
        fwVer[2] = intToBcd(month);
        fwVer[3] = intToBcd(day);
    }

    // the FW version was either just loaded in fwVer array, or it's there since some previous call
    memcpy(inFwVer, fwVer, 4);      // copy in the firmware version in provided buffer
    return good;                    // does the FPGA date look ok or not?
}

void ChipInterface3::getFWversion(bool hardNotFloppy, BYTE *inFwVer)
{
    readFwVersionFromFpga(inFwVer); // read or just copy FW version
}

int ChipInterface3::bitValueTo01(BYTE val)
{
    int res = val ? 1 : 0;                      // convert some bit at possibly higher position to simple 0 or 1 for debug purpose
    return res;
}

// waits until both HDD FIFOs are empty and then until handshake is idle
bool ChipInterface3::waitForBusIdle(DWORD maxWaitTime, BYTE bitsIgnoreMask)
{
    DWORD start = Utils::getCurrentMs();
    BYTE status;

    fpgaAddressSet(FPGA_ADDR_STATUS);           // set address of STATUS byte

    while(true) {                               // while app is running
        if(sigintReceived) {                    // app terminated, quit now, no action
            return false;
        }

        DWORD now = Utils::getCurrentMs();

        if((now - start) >= maxWaitTime) {      // if his is taking too long
//            Debug::out(LOG_DEBUG, "waitForBusIdle() timeout, status: %02X, STATUS_HDD_READ_FIFO_EMPTY: %d, STATUS_HDD_WRITE_FIFO_EMPTY: %d, STATUS_HDD_HANDSHAKE_IDLE: %d", 
//                    status, bitValueTo01(status & STATUS_HDD_READ_FIFO_EMPTY), bitValueTo01(status & STATUS_HDD_WRITE_FIFO_EMPTY), bitValueTo01(status & STATUS_HDD_HANDSHAKE_IDLE));
            return false;                       // fail with no action needed
        }

        status = fpgaDataRead();            // read status byte
        status |= bitsIgnoreMask;           // add these bits, it will pretend that they are always set and thus they will be ignored

        if((status & STATUS_HDD_READ_FIFO_EMPTY) == 0) {    // READ FIFO not empty, wait
            continue;
        }

        if((status & STATUS_HDD_WRITE_FIFO_EMPTY) == 0) {   // WRITE FIFO not empty, wait
            continue;
        }

        if((status & STATUS_HDD_HANDSHAKE_IDLE) == 0) {     // handshake not idle, wait
            continue;
        }

        break;          // everything idle, good, quit loop
    }

    return true;        // ok, everything idle
}

bool ChipInterface3::hdd_sendData_start(DWORD totalDataCount, BYTE scsiStatus, bool withStatus)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::sendData_start -- trying to send more than 16 MB, fail");
        return false;
    }

    // store the requested transfer info
    hddTransferInfo.totalDataCount = totalDataCount;
    hddTransferInfo.scsiStatus = scsiStatus;
    hddTransferInfo.withStatus = withStatus;

    return true;
}

bool ChipInterface3::hdd_sendData_transferBlock(BYTE *pData, DWORD dataCount)
{
    //Debug::out(LOG_DEBUG, "hdd_sendData_transferBlock() - starting with dataCount: %d", dataCount);

    if((dataCount & 1) != 0) {                      // odd number of bytes? make it even, we're sending words...
        dataCount++;
    }

    DWORD originalDataCount = dataCount;            // make a copy, it will be used at the end to subtract from totalDataCount

    DWORD timeout = timeoutForDataCount(dataCount); // calculate some timeout value based on data size
    DWORD start = Utils::getCurrentMs();

    while(dataCount > 0) {                          // while there's something to send
        bool res = waitForBusIdle(100);             // wait until bus gets idle before switching mode

        if(!res) {                                  // wait for bus idle failed?
            //Debug::out(LOG_DEBUG, "hdd_sendData_transferBlock() failed on waitForBusIdle() due to timeout, dataCount remaining: %d", dataCount);
            return false;
        }

        // Logic in FPGA allows us to set transfer size in either bytes (16 bytes maximum), or in sectors (16 sectors maximum, that's 8 kB).
        // If we use sector size, then the transfered data must be multiple of 512, so it's usefull only when there's at least 512 B of data.
        // If we use byte size, then maximum size is 16 bytes, so we can do smaller transfers, but if it's less than 512 bytes, it may require 
        // up to 32 times setting the transfer size (but allows us any transfer size).

        int sectorCount = dataCount / 512;          // calc remaining data size in sectors
        int byteCount;                              // this holds how many bytes we will transfer after setting the mode-dir-count register
        BYTE modeDirCnt;

        if(sectorCount > 0) {                       // is remaining data is at least 1 sector big? do transfer in whole sectors
            sectorCount = MIN(sectorCount, 16);     // limit sector count to 16
            byteCount = sectorCount * 512;          // convert sector count to byte count

            modeDirCnt = MODE_DMA | DIR_READ | SIZE_IN_SECTORS | (sectorCount - 1);

            //Debug::out(LOG_DEBUG, "hdd_sendData_transferBlock() will READ %d sector(s) (= %d bytes)", sectorCount, byteCount);
        } else {                                    // only remaining less than 1 sector of data - do transfer in bytes
            byteCount = MIN(dataCount, 16);         // limit byte count to 16

            modeDirCnt = MODE_DMA | DIR_READ | SIZE_IN_BYTES | (byteCount - 1);

            //Debug::out(LOG_DEBUG, "hdd_sendData_transferBlock() will READ %d bytes", byteCount);
        }

        fpgaAddressSet(FPGA_ADDR_MODE_DIR_CNT);     // set address of mode-dir-cnt register and set the new mode-dir-cnt value
        fpgaDataWrite(modeDirCnt);

        fpgaAddressSet(FPGA_ADDR_READ_FIFO_DATA);           // set address of READ FIFO so we can start move data 
        fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_OUTP);   // address + PIN_RW will configure PIN_STATUS as READ FIFO FULL pin

        while(byteCount > 0) {                          // while we still haven't transfered all of the data
            DWORD now = Utils::getCurrentMs();

            if((now - start) > timeout || sigintReceived) {    // if timeout happened or app should terminate, quit
                //Debug::out(LOG_DEBUG, "hdd_sendData_transferBlock() failed due to timeout, byteCount remaining: %d", byteCount);
                return false;
            }

            int status = bcm2835_gpio_lev(PIN_STATUS);  // read the status bit - READ FIFO FULL

            if(status == HIGH) {                        // if READ FIFO is full, can't tx anything now, wait
                continue;
            }

            fpgaDataWrite(*pData);                  // write data

            pData++;                                // advance data pointer
            dataCount--;                            // decrement data count
            byteCount--;
        }
    }

    // The data transfer has ended for now, it's time to update the total data count, and if status should be sent at the end of final transfer, then do that too.
    if(hddTransferInfo.totalDataCount >= originalDataCount) {       // if can safely subtract this data count from total data count
        hddTransferInfo.totalDataCount -= originalDataCount;
    } else {                                                        // if we transfered more than we should (shouldn't happen), set total data count to 0
        hddTransferInfo.totalDataCount = 0;
    }

    // nothing to send AND should send status? then just quit with status byte
    if(hddTransferInfo.totalDataCount == 0 && hddTransferInfo.withStatus) {
        return hdd_sendStatusToHans(hddTransferInfo.scsiStatus);
    }

    return true;
}

bool ChipInterface3::hdd_recvData_start(BYTE *recvBuffer, DWORD totalDataCount)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::recvData_start() -- trying to send more than 16 MB, fail");
        return false;
    }

    // store the requested transfer info
    hddTransferInfo.totalDataCount = totalDataCount;
    hddTransferInfo.scsiStatus = 0;                 // not used in hdd_recvData_transferBlock
    hddTransferInfo.withStatus = false;             // not used in hdd_recvData_transferBlock

    return true;
}

DWORD ChipInterface3::timeoutForDataCount(DWORD dataCount)
{
    int dataSizeInKB = (dataCount / 1024) + 1;      // calculate how many kBs this transfer is

    DWORD timeout = dataSizeInKB * 4;               // allow at least 4 ms to transfer 1 kB (that is 250 kB/s)
    timeout = MAX(timeout, 100);                    // let timeout be at least 100 ms, or more

    return timeout;
}

bool ChipInterface3::hdd_recvData_transferBlock(BYTE *pData, DWORD dataCount)
{
    #if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)

    DWORD originalDataCount = dataCount;            // make a copy, it will be used at the end to subtract from totalDataCount

    DWORD timeout = timeoutForDataCount(dataCount); // calculate some timeout value based on data size
    DWORD start = Utils::getCurrentMs();
    int loop = 0;

    while(dataCount > 0) {                          // while there's something to send
        loop++;

        bool res = waitForBusIdle(100, STATUS_HDD_WRITE_FIFO_EMPTY);    // wait until bus gets idle before switching mode

        if(!res) {                                  // wait for bus idle failed?
            //Debug::out(LOG_DEBUG, "[%d] hdd_recvData_transferBlock() failed on waitForBusIdle() due to timeout, on %d dataCount out of %d originalDataCount", loop, dataCount, originalDataCount);
            return false;
        }

        // Logic in FPGA allows us to set transfer size in either bytes (16 bytes maximum), or in sectors (16 sectors maximum, that's 8 kB).
        // If we use sector size, then the transfered data must be multiple of 512, so it's usefull only when there's at least 512 B of data.
        // If we use byte size, then maximum size is 16 bytes, so we can do smaller transfers, but if it's less than 512 bytes, it may require 
        // up to 32 times setting the transfer size (but allows us any transfer size).

        int sectorCount = dataCount / 512;          // calc remaining data size in sectors
        int byteCount;                              // this holds how many bytes we will transfer after setting the mode-dir-count register
        BYTE modeDirCnt;

        if(sectorCount > 0) {                       // is remaining data is at least 1 sector big? do transfer in whole sectors
            sectorCount = MIN(sectorCount, 16);     // limit sector count to 16
            byteCount = sectorCount * 512;          // convert sector count to byte count

            modeDirCnt = MODE_DMA | DIR_WRITE | SIZE_IN_SECTORS | (sectorCount - 1);

            //Debug::out(LOG_DEBUG, "[%d] hdd_recvData_transferBlock() will WRITE %d sector(s) (= %d bytes), modeDirCnt: %02X", loop, sectorCount, byteCount, modeDirCnt);
        } else {                                    // only remaining less than 1 sector of data - do transfer in bytes
            byteCount = MIN(dataCount, 16);         // limit byte count to 16

            modeDirCnt = MODE_DMA | DIR_WRITE | SIZE_IN_BYTES | (byteCount - 1);

            //Debug::out(LOG_DEBUG, "[%d] hdd_recvData_transferBlock() will WRITE %d bytes. modeDirCnt: %02X", loop, byteCount, modeDirCnt);
        }

        fpgaAddressSet(FPGA_ADDR_MODE_DIR_CNT);     // set address of mode-dir-cnt register and set the new mode-dir-cnt value
        fpgaDataWrite(modeDirCnt);

        fpgaAddressSet(FPGA_ADDR_WRITE_FIFO_DATA2);         // set address of WITE FIFO so we can start to move data 
        fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_INPT);   // address + PIN_RW will configure PIN_STATUS as WRITE FIFO EMPTY pin

        while(byteCount > 0) {                          // while we still haven't transfered all of the data
            DWORD now = Utils::getCurrentMs();

            if((now - start) > timeout || sigintReceived) {    // if timeout happened or app should terminate, quit
                //Debug::out(LOG_DEBUG, "[%d] hdd_recvData_transferBlock() failed due to timeout, on %d dataCount out of %d originalDataCount", loop, dataCount, originalDataCount);
                return false;
            }

            int status = bcm2835_gpio_lev(PIN_STATUS);  // read the status bit - WRITE FIFO EMPTY

            if(status == HIGH) {                        // if WRITE FIFO is EMPTY, can't tx anything now, wait
                continue;
            }

            *pData = fpgaDataRead();                    // get data

            pData++;            // advance data pointer
            dataCount--;        // decrement data count
            byteCount--;
        }
    }

    // The data transfer has ended for now, it's time to update the total data count, and if status should be sent at the end of final transfer, then do that too.
    if(hddTransferInfo.totalDataCount >= originalDataCount) {       // if can safely subtract this data count from total data count
        hddTransferInfo.totalDataCount -= originalDataCount;
    } else {                                                        // if we transfered more than we should (shouldn't happen), set total data count to 0
        hddTransferInfo.totalDataCount = 0;
    }

    #endif

    return true;
}

bool ChipInterface3::hdd_sendStatusToHans(BYTE statusByte)
{
    bool res;

    res = waitForBusIdle(100);      // wait until bus gets idle before switching mode

    if(!res) {                      // wait for bus idle failed?
        //Debug::out(LOG_DEBUG, "hdd_sendStatusToHans() [1] failed on waitForBusIdle() due to timeout");
        return false;
    }

    // set address of mode-dir-cnt register and PIO READ BYTE (read status byte)
    fpgaAddressSet(FPGA_ADDR_MODE_DIR_CNT);
    fpgaDataWrite(MODE_PIO | DIR_READ | SIZE_IN_BYTES | 0);

    // write of data to address 3 (FPGA_ADDR_MODE_DIR_CNT) changes the address to 4 (FPGA_ADDR_READ_FIFO_DATA), so we can write to READ FIFO without fpgaAddressSet()
    fpgaDataWrite(statusByte);

    // wait until bus gets idle before proceeding further
    res = waitForBusIdle(100);

    if(!res) {                      // wait for bus idle failed?
        //Debug::out(LOG_DEBUG, "hdd_sendStatusToHans() [2] failed on waitForBusIdle() due to timeout");
        return false;
    }

    if(interfaceType == INTERFACE_TYPE_SCSI) {  // on SCSI interface we also need to send MESSAGE IN byte
        fpgaAddressSet(FPGA_ADDR_MODE_DIR_CNT);                     // set address of mode-dir-cnt register and MSG READ BYTE (MSG IN byte)
        fpgaDataWrite(MODE_MSG | DIR_READ | SIZE_IN_BYTES | 0);

        // automatic address change, write to READ FIFO without fpgaAddressSet()
        fpgaDataWrite(0);

        // wait until bus gets idle before proceeding further
        res = waitForBusIdle(100);

        if(!res) {                      // wait for bus idle failed?
            //Debug::out(LOG_DEBUG, "hdd_sendStatusToHans() [3] failed on waitForBusIdle() due to timeout");
            return false;
        }
    }

    // set address of mode-dir-cnt register and IDLE mode for another cmd[0] byte receiving
    fpgaAddressSet(FPGA_ADDR_MODE_DIR_CNT);
    fpgaDataWrite(MODE_IDLE | DIR_WRITE | SIZE_IN_BYTES | 0);

    return true;
}

void ChipInterface3::fdd_sendTrackToChip(int byteCount, BYTE *encodedTrack)
{
    // the internal READ MFM RAM address was reset to 0, when we did read the requested track + side from FPGA_ADDR_SIDE_TRACK address
    fpgaAddressSet(FPGA_ADDR_READ_MFM_RAM);     // select address - storing data into MFM RAM - MFM READ stream halted when this address is selected!

    for(int i=0; i<byteCount; i++) {            // write all the data from encoded track int MFM READ RAM
        fpgaDataWrite(encodedTrack[i]);
    }

    fpgaAddressSet(FPGA_ADDR_STATUS);           // select some other address so resume MFM READ stream
}

// this function will be called once the other function collected whole written sector
BYTE* ChipInterface3::fdd_sectorWritten(int &side, int &track, int &sector, int &byteCount)
{
    // store written sector address (side, track, sector) and byte count
    side = fddWrittenSector.side;
    track = fddWrittenSector.track;
    sector = fddWrittenSector.sector;
    byteCount = fddWrittenSector.index;

    // return pointer to received written sector
    return fddWrittenSector.data;
}

////////////////////////////////////////////////////////////////////////////
// low level functions for talking to FPGA

void ChipInterface3::fpgaDataPortSetDirection(int pinDirection)      // pin direction is BCM2835_GPIO_FSEL_INPT or BCM2835_GPIO_FSEL_OUTP
{
    #if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)

    static int currentPinDir = -123;               // holds which pin direction is the currently set direction, to avoid setting the same again

    //-----------
    // keep the track of pin direction we have, so in case this gets called and the pin direction is already set as needed, then just quit and don't fiddle with the directions
    if(currentPinDir == pinDirection) {     // if already we have the right pin direction, just quit
        return;
    }

    currentPinDir = pinDirection;           // store the current pin direction

    //-----------
    int dataPins[8] = {PIN_DATA0, PIN_DATA1, PIN_DATA2, PIN_DATA3, PIN_DATA4, PIN_DATA5, PIN_DATA6, PIN_DATA7};

    if(pinDirection == BCM2835_GPIO_FSEL_OUTP) {       // if OUTPUT, R/W to L, so the FPGA stops driving pins
        bcm2835_gpio_write(PIN_RW, LOW);
    }

    for(int i=0; i<8; i++) {    // set direction of data pins
        bcm2835_gpio_fsel(dataPins[i], pinDirection);
    }

    if(pinDirection == BCM2835_GPIO_FSEL_INPT) {        // if INPUT, R/W to H, so the FPGA starts driving pins
        bcm2835_gpio_write(PIN_RW, HIGH);
    }

    #endif
}

// set OUTPUT direction, output address to data port, switch to ADDRESS, TRIG the operation
void ChipInterface3::fpgaAddressSet(BYTE addr, bool force)
{
    static BYTE prevAddress = 0xff;                     // holds last set address - used to skip setting the same address over and over again, if not really needed

    if(prevAddress == addr && !force) {                 // if address didn't change since the last time and not forcing set, just quit
        return;
    }

    #if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)

    prevAddress = addr;                                 // remember that we've set this address

    fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_OUTP);   // pins as output
    fpgaDataOutput(addr);                               // put address on pins, PIN_RW into right level

    bcm2835_gpio_write(PIN_DATA_ADDR, LOW);             // H for data register, L for address register

    bcm2835_gpio_write(PIN_TRIG,      HIGH);            // PIN_TRIG will be low-high-low to trigger the operation
    bcm2835_gpio_write(PIN_TRIG,      LOW);

    #endif
}

// set OUTPUT direction, output data to data port, switch to DATA, TRIG the operation
void ChipInterface3::fpgaDataWrite(BYTE data)
{
    #if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)

    fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_OUTP);   // pins as output
    fpgaDataOutput(data);                               // put address on pins, PIN_RW into right level

    bcm2835_gpio_write(PIN_DATA_ADDR, HIGH);            // H for data register, L for address register

    bcm2835_gpio_write(PIN_TRIG,      HIGH);            // PIN_TRIG will be low-high-low to trigger the operation
    bcm2835_gpio_write(PIN_TRIG,      LOW);

    #endif
}

// set INPUT direction, switch to DATA, TRIG the operation, get the data from data port
BYTE ChipInterface3::fpgaDataRead(void)
{
    #if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)

    fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_INPT);   // pins as input

    bcm2835_gpio_write(PIN_DATA_ADDR, HIGH);            // H for data register, L for address register

    bcm2835_gpio_write(PIN_TRIG,      HIGH);            // PIN_TRIG will be low-high-low to trigger the operation
    bcm2835_gpio_write(PIN_TRIG,      LOW);

    bcm2835_gpio_lev(PIN_TRIG);                         // this line is useless, but it serves as a delay between TRIG going down and reading data from FPGA

    #endif

    BYTE val = fpgaDataInput();                         // get the data from port and return it
    return val;
}

// output data on data port
void ChipInterface3::fpgaDataOutput(BYTE val)
{
    #if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)

    DWORD value = ((((DWORD) val) & 0xe0) << 19) | ((((DWORD) val) & 0x1f) << 18);
    bcm2835_gpio_write_mask(value, DATA_MASK);

    #endif
}

// read data from data port
BYTE ChipInterface3::fpgaDataInput(void)
{
    #if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)

    // get whole gpio by single read -- taken from bcm library
    volatile DWORD* paddr = bcm2835_gpio + BCM2835_GPLEV0/4;
    DWORD value = bcm2835_peri_read(paddr);

    // GPIO 26-24 + 22-18 of RPi are used as data port, split and shift data into right position
    DWORD upper = (value >> 19) & 0xe0;
    DWORD lower = (value >> 18) & 0x1f;

    BYTE val = upper | lower;   // combine upper and lower part together
    return val;

    #else

    return 0;

    #endif
}
////////////////////////////////////////////////////////////////////////////

