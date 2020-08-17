#include <string.h>

#if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)
    #include <bcm2835.h>
#endif

#include "chipinterface3.h"
#include "gpio.h"
#include "../utils.h"
#include "../debug.h"


ChipInterface3::ChipInterface3()
{
    // set these vars to some not valid value, so we'll do the actual setting of pin direction or address on the first time
    currentPinDir = -123;

    ikbdEnabled = false;        // ikbd is not enabled by default, so we need to enable it

    fddEnabled = true;          // if true, floppy part is enabled
    fddId1 = false;             // FDD ID0 on false, FDD ID1 on true
    fddWriteProtected = false;  // if true, writes to floppy are ignored
    fddDiskChanged = false;     // if true, disk change is happening

    newTrackRequest = false;    // there was no track request yet
    fddReqSide = 0;             // FDD requested side (0/1)
    fddReqTrack = 0;            // FDD requested track
    fddTrackRequestTime = 0;    // when was FDD track requested last time (not yet)

    fddWrittenSector.data = new BYTE[WRITTENMFMSECTOR_SIZE];    // holds the written sector data until it's all and ready to be processed
    fddWrittenSector.byteCount = 0;                             // count of bytes in the fddWrittenSector buffer

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
        return false;
    }

    // try to init the GPIO library
    if (!bcm2835_init()) {
        Debug::out(LOG_ERROR, "bcm2835_init failed, can't use GPIO.");
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

    bcm2835_gpio_write(PIN_DATA_ADDR, LOW);
    bcm2835_gpio_write(PIN_RW,        LOW);
    bcm2835_gpio_write(PIN_TRIG,      LOW);

    // FPGA data pins as inputs
    fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_INPT);

    #endif

    return true;
}

void ChipInterface3::close(void)
{
    #if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)

    fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_INPT);  // FPGA data pins as inputs

    bcm2835_close();            // close the GPIO library and finish

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

    config |= fddEnabled        ? (1 << 5) : 0;         // if true, floppy part is enabled
    config |= fddId1            ? (1 << 4) : 0;         // FDD ID0 on false, FDD ID1 on true
    config |= fddWriteProtected ? (1 << 3) : 0;         // if true, writes to floppy are ignored
    config |= fddDiskChanged    ? (1 << 2) : 0;         // if true, disk change is happening

    config |= ikbdEnabled       ? 0        : (1 << 0);  // for disabling IKDB - set bit 0

    fpgaAddressSet(FPGA_ADDR_CONFIG);                   // set the address
    fpgaDataWrite(config);                              // write config data to config register
}

bool ChipInterface3::actionNeeded(bool &hardNotFloppy, BYTE *inBuf)
{
    static DWORD lastFirmwareVersionReportTime = 0;         // this holds the time when we last checked for selected interface type. We don't need to do this often.
    static DWORD lastInterfaceTypeCheckTime = 0;            // we didn't check for interface type yet
    static bool reportHddVersion = false;                   // when true, will report HDD FW version
    static bool reportFddVersion = false;                   // when true, will report HDD FW version

    BYTE status;
    DWORD now = Utils::getCurrentMs();

    if((now - lastFirmwareVersionReportTime) >= 1000) {     // if at least 1 second passed since we've pretended we got firmware version from chip
        lastFirmwareVersionReportTime = now;

        reportHddVersion = true;                            // report both HDD and FDD FW versions
        reportFddVersion = true;
    }

    if(reportHddVersion) {                                  // if should report FW version
        reportHddVersion = false;
        inBuff[3] = ATN_FW_VERSION;
        return true;                                        // action needs handling, FW version will be retrieved via getFWversion() function
    }

    if(reportFddVersion) {                                  // if should report FW version
        reportFddVersion = false;
        inBuff[3] = ATN_FW_VERSION;
        return true;                                        // action needs handling, FW version will be retrieved via getFWversion() function
    }

    if((now - lastInterfaceTypeCheckTime) >= 1000) {        // if at least 1 second passed since we checked interface type, we should check now
        lastInterfaceTypeCheckTime = now;

        fpgaAddressSet(FPGA_ADDR_STATUS2);                      // set status 2 register address
        status = fpgaDataRead();                                // read the status 2

        interfaceType = status & STATUS2_HDD_INTERFACE_TYPE;    // get interface type bits
    }

    //------------------------------------
    fpgaAddressSet(FPGA_ADDR_STATUS);                   // set status register address
    status = fpgaDataRead();                            // read the status

    if((status & STATUS_HDD_WRITE_FIFO_EMPTY) == 0) {   // if WRITE FIFO is not empty (flag is 0), then ST has sent us CMD byte
        return getHddCommand(inBuf);                    // if this command was for one of our enabled HDD IDs, then this will return true and will get handled
    }

    if(status & STATUS_FDD_SIDE_TRACK_CHANGED) {        // if floppy track or side changed, we need to send new track data
        handleTrackChanged();
    }

    bool actionNeeded = trackChangedNeedsAction(inBuf); // if track change happened a while ago, we will need to set new track data

    if(actionNeeded) {                                  // if should send track data, quit and request action, otherwise continue with the rest
        return true;
    }

    if((status & STATUS_FDD_WRITE_FIFO_EMPTY) == 0) {   // WRITE FIFO NOT EMPTY, so some floppy data was written
        actionNeeded = handleFloppyWriteBytes(inBuf);   // get written data and store them in buffer

        if(actionNeeded) {                              // if got whole written sector, quit and request action, otherwise continue with the rest
            return true;
        }
    }

    // no action needed
    return false;
}

bool ChipInterface3::getHddCommand(BYTE *inBuf)
{
    // TODO



    inBuff[3] = ATN_ACSI_COMMAND;
    return true;                                            // this command was for enabled ID, so it needs handling
}

// This function should read which track and side was wanted, mark the time when this happened, but don't send the track just yet....
void ChipInterface3::handleTrackChanged(void)
{
    fpgaAddressSet(FPGA_ADDR_SIDE_TRACK);               // set REQUESTED SITE/TRACK register address - this also resets the internal READ MFM RAM address to 0, so we can then just write the new READ MFM RAM data from address 0
    BYTE sideTrack = fpgaDataRead();                    // read the status

    fddReqSide = (sideTrack >> 7) & 1;                  // FDD requested side (0/1) - bit 7
    fddReqTrack = sideTrack & 0x7f;                     // FDD requested track - bits 6..0

    fddTrackRequestTime = Utils::getCurrentMs();        // when was FDD track requested last time
    newTrackRequest = true;                             // this track request is new, it's pending, but shouldn't be handled sooner than when floppy seek ends
}

// This function takes a look at the timestamp when the last track change was requested, and if it was at least X ms ago, then we can really handle this
// (The seek rate can be 2 / 3 / 6 / 12 ms, so if we send track data very soon, the floppy still might be doing seek and we might need to send it again,
// so this delay before sending data should make sure that the seek has finished and we really need to send just the last track requested).
bool ChipInterface3::trackChangedNeedsAction(BYTE *inBuf)
{
    if(!newTrackRequest) {          // if there isn't a track request which wasn't handled
        return false;               // no action to be handled
    }

    DWORD now = Utils::getCurrentMs();

    if((now - fddTrackRequestTime) < 12) {  // there was some track request, but it was less than X ms ago, so floppy seek still migt be going
        return false;                       // no action to be handled (yet)
    }

    // enough time passed to make sure that floppy seek has ended
    newTrackRequest = false;        // mark that we've just handled this, don't handle it again

    inBuf[3] = ATN_SEND_TRACK;      // attention code
    inBuf[8] = fddReqSide;          // fdd side
    inBuf[9] = fddReqTrack;         // fdd track
    return true;                    // action needs handling
}

// Call this when there are some bytes written to floppy. When it's not enough data for whole sector, then this just stores the bytes and returns false.
// If enough data was received for the whole floppy sector, this returns true and later the data will be retrieved using fdd_sectorWritten() function.
bool ChipInterface::handleFloppyWriteBytes(BYTE *inBuf)
{
    // TODO


//    fddWrittenSector.data = new BYTE[WRITTENMFMSECTOR_SIZE];    // holds the written sector data until it's all and ready to be processed
//    fddWrittenSector.byteCount = 0;                             // count of bytes in the fddWrittenSector buffer

    // if can still can store data


    // find 0xCAFE header


    // extract SIDE, TRACK, SECTOR


    // keep reading and storing until a ZERO is read from FIFO

    // store the byteCount of written data

    // got whole sector, request action. Data will be accessed via fdd_sectorWritten() function.
    inBuff[3] = ATN_SECTOR_WRITTEN;
    return true;                                        // action needs handling
}

void ChipInterface3::setHDDconfig(BYTE hddEnabledIDs, BYTE sdCardId, BYTE fddEnabledSlots, bool setNewFloppyImageLed, BYTE newFloppyImageLed)
{
    // store only HDD enabled IDs, don't care about the rest
    this->hddEnabledIDs = hddEnabledIDs;
}

void ChipInterface3::setFDDconfig(bool setFloppyConfig, bool fddEnabled, int id, int writeProtected, bool setDiskChanged, bool diskChanged)
{
    bool configChanged = false;                         // nothing changed yet

    if(setFloppyConfig) {
        configChanged |= (this->fddEnabled != fddEnabled);      // if this config changed then set configChanged flag
        this->fddEnabled = fddEnabled;

        bool newFddId1 = (id == 1);
        configChanged |= (fddId1 != newFddId1);                 // if this config changed then set configChanged flag
        fddId1 = newFddId1;

        configChanged |= (fddWriteProtected != writeProtected); // if this config changed then set configChanged flag
        fddWriteProtected = writeProtected;
    }

    if(setDiskChanged) {
        configChanged |= (fddDiskChanged != diskChanged);       // if this config changed then set configChanged flag
        fddDiskChanged = diskChanged;
    }

    if(configChanged) {                                 // set new config only if something changed
        fpgaResetAndSetConfig(false, false);            // not doing reset of HDD or FDD, but changing config
    }
}

bYTE ChipInterface3::intToBcd(int integer)
{
    int a, b;
    a = integer / 10;
    b = integer % 10;

    BYTE bcd = (a << 4) | b;
    return bcd;
}

void ChipInterface3::getFWversion(bool hardNotFloppy, BYTE *inFwVer)
{
    static bool didReadFirmwareVersion = false;     // true if at least once was the firmware version read from chip
    static BYTE fwVer[4];                           // this should hold FW version prepared to be copied to inFwVer

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

        fwVer[0] = 0;
        fwVer[1] = intToBcd(year);
        fwVer[2] = intToBcd(month);
        fwVer[3] = intToBcd(day);
    }

    memcpy(inFwVer, fwVer, 4);                      // copy in the firmware version in provided buffer
}

bool ChipInterface3::hdd_sendData_start(DWORD totalDataCount, BYTE scsiStatus, bool withStatus)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::sendData_start -- trying to send more than 16 MB, fail");
        return false;
    }

    // TODO


    return true;
}

bool ChipInterface3::hdd_sendData_transferBlock(BYTE *pData, DWORD dataCount)
{
    if((dataCount & 1) != 0) {                                      // odd number of bytes? make it even, we're sending words...
        dataCount++;
    }

    // TODO


    return true;
}

bool ChipInterface3::hdd_recvData_start(BYTE *recvBuffer, DWORD totalDataCount)
{
    if(totalDataCount > 0xffffff) {
        Debug::out(LOG_ERROR, "AcsiDataTrans::recvData_start() -- trying to send more than 16 MB, fail");
        return false;
    }

    // TODO


    return true;
}

bool ChipInterface3::hdd_recvData_transferBlock(BYTE *pData, DWORD dataCount)
{
    // TODO


    return true;
}

bool ChipInterface3::hdd_sendStatusToHans(BYTE statusByte)
{
    // TODO


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
    byteCount = fddWrittenSector.byteCount;

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
void ChipInterface3::fpgaAddressSet(BYTE addr)
{
    static BYTE prevAddress = 0xff;                     // holds last set address - used to skip setting the same address over and over again, if not really needed

    if(prevAddress == addr) {                           // if address didn't change since the last time, just quit
        return;
    }

    prevAddress = addr;                                 // remember that we've set this address

    fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_OUTP);   // pins as output
    fpgaDataOutput(addr);                               // put address on pins, PIN_RW into right level

    bcm2835_gpio_write(PIN_DATA_ADDR, LOW);             // H for data register, L for address register

    bcm2835_gpio_write(PIN_TRIG,      HIGH);            // PIN_TRIG will be low-high-low to trigger the operation
    bcm2835_gpio_write(PIN_TRIG,      LOW);
}

// set OUTPUT direction, output data to data port, switch to DATA, TRIG the operation
void ChipInterface3::fpgaDataWrite(BYTE data)
{
    fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_OUTP);   // pins as output
    fpgaDataOutput(data);                               // put address on pins, PIN_RW into right level

    bcm2835_gpio_write(PIN_DATA_ADDR, HIGH);            // H for data register, L for address register

    bcm2835_gpio_write(PIN_TRIG,      HIGH);            // PIN_TRIG will be low-high-low to trigger the operation
    bcm2835_gpio_write(PIN_TRIG,      LOW);
}

// set INPUT direction, switch to DATA, TRIG the operation, get the data from data port
BYTE ChipInterface3::fpgaDataRead(void)
{
    fpgaDataPortSetDirection(BCM2835_GPIO_FSEL_INPT);   // pins as input

    bcm2835_gpio_write(PIN_DATA_ADDR, HIGH);            // H for data register, L for address register

    bcm2835_gpio_write(PIN_TRIG,      HIGH);            // PIN_TRIG will be low-high-low to trigger the operation
    bcm2835_gpio_write(PIN_TRIG,      LOW);

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

