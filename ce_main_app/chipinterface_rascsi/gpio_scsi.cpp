#include <cstring>

#include "../global.h"
#include "../utils.h"
#include "../debug.h"
#include "gpio_scsi.h"
#include "../native/scsi.h"

extern THwConfig hwConfig;
extern TFlags    flags;                 // global flags from command line

GpioScsi::GpioScsi()
{
    hddEnabledIDs = 0;      // nothing enabled yet
    sdCardId = 0xff;        // SD card not enabled

    generateParityTable();
}

GpioScsi::~GpioScsi()
{

}

void GpioScsi::generateParityTable(void)
{
    for(int i=0; i<256; i++) {
        parityTable[i] = parityOfByte((uint8_t) i);
    }
}

uint8_t GpioScsi::parityOfByte(uint8_t val)
{
    int bitsCount = 0;
    for(int i=0; i<8; i++) {    // check all bits
        if((val >> i) & 1) {    // if bit at position i is set, increment bit count
            bitsCount++;
        }
    }

    return (bitsCount & 1) ? 1 : 0; // return 1 if odd count of bits, return 0 on even count of bits
}

void GpioScsi::init(uint8_t hddEnabledIDs, uint8_t sdCardId)
{
    setConfig(hddEnabledIDs, sdCardId);     // set enabled IDs
    setPhase(SCSI_PHASE_BUSFREE);
}

void GpioScsi::initPins(void)
{
    bcm2835_gpio_write(PIN_IND, LOW);   // PIN_IND always LOW, because SEL, RES, ACK, ATN will always come from initiator and we want just to read them
    bcm2835_gpio_write(PIN_BSY, HIGH);  // start with BSY high
    bcm2835_gpio_write(PIN_TAD, LOW);   // TAD low - phase signals are not driven by this device (yet)
    bcm2835_gpio_write(PIN_ACT, LOW);   // ACT low - activity led not on
    setPhase(SCSI_PHASE_BUSFREE);
}

void GpioScsi::setConfig(uint8_t hddEnabledIDs, uint8_t sdCardId)
{
    Debug::out(LOG_DEBUG, "GpioScsi::setConfig - setting hddEnabledIDs: %02x, sdCardId: %d", hddEnabledIDs, sdCardId);
    this->hddEnabledIDs = hddEnabledIDs;
    this->sdCardId = sdCardId;
}

uint8_t GpioScsi::getXilinxByte(void)
{
    return 0x82;                            // RaSCSI - SCSI only
}

bool GpioScsi::getCmd(uint8_t* cmd)
{
    setPhase(SCSI_PHASE_BUSFREE);

#ifndef ONPC
    // check for selection
    if(bcm2835_gpio_lev(PIN_RST) == LOW || bcm2835_gpio_lev(PIN_SEL) == HIGH) {   // SCSI RESET L or SEL not L? Selection not happening
        return false;
    }

    timeoutStart(1000);
    setPhase(SCSI_PHASE_SELECTION);

    if(!waitForTwoPinLevels(PIN_SEL, LOW, PIN_BSY, HIGH)) {
        Debug::out(LOG_DEBUG, "GpioScsi::getCmd - failed to wait for selection with BSY released");
        return false;
    }

    // get data - it will contain initiator and target bits set
    uint8_t ids = dataIn();

    memset(cmd, 0, 16);                 // clear the command buffer

    if((ids & hddEnabledIDs) == 0) {    // if after removing disabled IDs from received IDs there's nothing left, this command is not for us
        Debug::out(LOG_DEBUG, "GpioScsi::getCmd - hddEnabledIDs: %02x, ids: %02x, requested ID not enabled, ignoring", hddEnabledIDs, ids);
        Debug::cmdStart(cmd, "DEV OFF");
        setPhase(SCSI_PHASE_BUSFREE);

        waitForTwoPinLevels(PIN_SEL, HIGH, PIN_BSY, HIGH);  // wait for selection to end
        return false;
    }

    uint8_t i;
    uint8_t id = 0xff;
    for(i=0; i<8; i++) {
        if((ids & (1 << i)) != 0) {         // if bit is one, this ID is selected 
            id = i;                         // store this ID and quit loop
            break;
        }
    }

    Debug::cmdMarkStartTime();
    timeoutStart(100);                      // start timeout - 100 ms to get whole command (only up to 13 bytes, so 100 ms is enough)
    
    // check for ATN, if asserted, do MSGOUT
    while(bcm2835_gpio_lev(PIN_ATN) == LOW) {      // ATN pin asserted?
        setPhase(SCSI_PHASE_MSGOUT);
        uint8_t msg = recvByte();

        // TODO: handle MSG OUT
        Debug::out(LOG_DEBUG, "GpioScsi::getCmd - received MSGOUT byte: %02x", msg);
    }

    setPhase(SCSI_PHASE_COMMAND);           // enable output of phase bits, sets BSY to L

    uint8_t cmdLen = 6;                     // maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()

    for(i=0; i<cmdLen; i++) {               // receive the next command bytes
        cmd[i] = recvByte();

        if(isTimeout()) {                   // if something was wrong, quit, failed
            Debug::out(LOG_DEBUG, "GpioScsi::getCmd - timeout on cmd byte %d", i);
            Debug::cmdStart(cmd, "CMD T/O");
            setPhase(SCSI_PHASE_BUSFREE);
            return false;
        }

        if(i == 0) {                        // if we got also the 2nd byte, get actual cmd length
            cmdLen = Scsi::getCmdLengthFromCmdBytesScsi(cmd);
            Debug::out(LOG_DEBUG, "GpioScsi::getCmd - for cmd: %02x -> cmdLen: %d", cmd[0], cmdLen);
        }             
    }

    // For the scsi commands to work with the rest of the acsi handling code,
    // alter the scsi command if the length is more than 6 bytes for it to look like ICD command
    // and also add fake ID to the command
    if(cmdLen > 6) {
        for(i=17; i>0; i--) {                       // move the cmd one byte further (to make cmd[0] unused)
            cmd[i] = cmd[i - 1];
        }
        cmd[0] = 0x1f;                              // store ICD command marker

        cmdLen++;                                   // now the command is one byte longer
    }

    // for all commands add fake ACSI ID on top of the 0th byte
    cmd[0] = cmd[0] | (id << 5);                    // add ID on the top 3 bits

    if(cmdLen > 6) {
        Debug::out(LOG_DEBUG, "GpioScsi::getCmd - got cmd: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", 
                                cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9]);
    } else {
        Debug::out(LOG_DEBUG, "GpioScsi::getCmd - got cmd: %02x %02x %02x %02x %02x %02x", 
                                cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
    }

    timeoutStart(1000);     // update timeout to 1 second after we've got the command (it will be updated in startTransfer() later)

    return true;
#else
    return false;
#endif
}

void GpioScsi::startTransfer(uint8_t sendNotRecv, uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus)
{
    if(sendNotRecv) {   // send == data in
        setPhase(SCSI_PHASE_DATAIN);
    } else {            // recv == data out
        setPhase(SCSI_PHASE_DATAOUT);
    }

    this->sendNotRecv = sendNotRecv;
    this->totalDataCount = totalDataCount;
    this->scsiStatus = scsiStatus;
    this->withStatus = withStatus;

    #define BLOCK_SIZE (128 * 1024)
    uint32_t blocks = totalDataCount / BLOCK_SIZE;  // how many 128 kB blocks this is going to be? 
    
    if(totalDataCount % BLOCK_SIZE != 0) {          // data size not exactly block size? one more (incomplete) block
        blocks++;
    }

    uint32_t timeout = blocks * 1000;   // allow each block to take up to 1 second
    timeout = MIN(timeout, 15000);      // cap the timeout to 15 s

    timeoutStart(timeout);              // start this longer timeout for data
}

bool GpioScsi::sendBlock(uint8_t *pData, uint32_t dataCount)
{
    setPhase(SCSI_PHASE_DATAIN);

    for(uint32_t i=0; i<dataCount; i++) {
        bool ok = sendByte(pData[i]);

        if(!ok) {        // failed to send?
            Debug::out(LOG_WARNING, "GpioScsi::sendBlock failed on byte %d out of %d.", i, dataCount);
            setPhase(SCSI_PHASE_BUSFREE);
            return false;
        }
    }

    if(withStatus) {        // if block transfer should end with status, send it now
        bool ok = sendStatus(scsiStatus);

        if(!ok) {
            Debug::out(LOG_WARNING, "GpioScsi::sendBlock failed on sendStatus (dataCount: %d)", dataCount);
        }

        return ok;
    }

    return true;            // all OK
}

bool GpioScsi::recvBlock(uint8_t *pData, uint32_t dataCount)
{
    setPhase(SCSI_PHASE_DATAOUT);

    for(uint32_t i=0; i<dataCount; i++) {
        pData[i] = recvByte();

        if(isTimeout()) {
            Debug::out(LOG_WARNING, "GpioScsi::recvBlock failed on byte %d out of %d.", i, dataCount);
            setPhase(SCSI_PHASE_BUSFREE);
            return false;
        }
    }

    if(withStatus) {        // if block transfer should end with status, send it now
        bool ok = sendStatus(scsiStatus);

        if(!ok) {
            Debug::out(LOG_WARNING, "GpioScsi::recvBlock failed on sendStatus (dataCount: %d)", dataCount);
        }

        return ok;
    }

    return true;            // all OK
}

uint8_t GpioScsi::recvByte(void)
{
    uint8_t data = 0;

#ifndef ONPC
    bcm2835_gpio_write(PIN_REQ, LOW);       // REQ to L

    if(!waitForAckLevel(LOW)) {             // wait for ACK being L, return 0 if didn't come
        return 0;
    }

    data = dataIn();                        // read data after ACK is L

    bcm2835_gpio_write(PIN_REQ, HIGH);      // REQ back to H

    if(!waitForAckLevel(HIGH)) {            // wait for ACK being H, return 0 if didn't come
        return 0;
    }

#endif

    return data;
}

bool GpioScsi::sendByte(uint8_t data)
{
#ifndef ONPC
    dataOut(data);                          // set data and parity to data bus

    bcm2835_gpio_write(PIN_REQ, LOW);       // REQ to L

    if(!waitForAckLevel(LOW)) {             // wait for ACK being L, return false if didn't come
        Debug::out(LOG_WARNING, "GpioScsi::sendByte - failed to wait for ACK=L");
        return false;
    }

    bcm2835_gpio_write(PIN_REQ, HIGH);      // REQ back to H

    if(!waitForAckLevel(HIGH)) {            // wait for ACK being H, return 0 if didn't come
        Debug::out(LOG_WARNING, "GpioScsi::sendByte - failed to wait for ACK=H");
        return false;
    }

#endif

    return true;
}

bool GpioScsi::sendStatus(uint8_t scsiStatus)
{
    bool ok;

    // send status
    setPhase(SCSI_PHASE_STATUS);
    ok = sendByte(scsiStatus);

    if(!ok) {
        setPhase(SCSI_PHASE_BUSFREE);
        Debug::out(LOG_WARNING, "GpioScsi::sendStatus failed on sending STATUS byte");
        return false;
    }

    // send message
    setPhase(SCSI_PHASE_MSGIN);
    ok = sendByte(0);

    if(!ok) {
        Debug::out(LOG_WARNING, "GpioScsi::sendStatus failed on sending MSGIN byte");
    }

    // release bus, return ok/fail
    setPhase(SCSI_PHASE_BUSFREE);
    return ok;
}

bool GpioScsi::waitForTwoPinLevels(int pin1, int level1, int pin2, int level2)
{
    while(true) {
#ifndef ONPC
        if(bcm2835_gpio_lev(pin1) == level1 && bcm2835_gpio_lev(pin2) == level2) {     // pins have expected level? success
            return true;
        }
#endif

        if(isTimeout()) {   // timeout? fail
            return false;
        }
    }
}

bool GpioScsi::waitForPinLevel(int pin, int level)
{
    while(true) {
#ifndef ONPC
        if(bcm2835_gpio_lev(pin) == level) {     // pin has expected level? success
            return true;
        }
#endif

        if(isTimeout()) {   // timeout? fail
            return false;
        }
    }
}

bool GpioScsi::waitForAckLevel(int level)
{
    return waitForPinLevel(PIN_ACK, level);
}

void GpioScsi::setDataDirection(uint8_t sendNotRecv)
{
    static uint8_t sendNotRecvNow = 0xff;   // init with no data direction set yet

    if(sendNotRecvNow == sendNotRecv) {     // direction not changed since last time? quit
        return;
    }

    sendNotRecvNow = sendNotRecv;           // remember what we're just setting

#ifndef ONPC
    // if output from RPi, set PIN_DTD to L before switching RPi pins directions
    if(sendNotRecv == DIR_SEND) {
        bcm2835_gpio_write(PIN_DTD, LOW);
    }

    // set RPi GPIO pins as outputs / inputs
    uint32_t dir = (sendNotRecv == DIR_SEND) ? BCM2835_GPIO_FSEL_OUTP : BCM2835_GPIO_FSEL_INPT;
    int dataPins[9] = {DATA0, DATA1, DATA2, DATA3, DATA4, DATA5, DATA6, DATA7, DATAP};
    for(int i=0; i<9; i++) {
        bcm2835_gpio_fsel(dataPins[i],  dir);
    }

    // if input to RPi, set PIN_DTD to H after switching RPi pins directions
    if(sendNotRecv == DIR_RECV) {
        bcm2835_gpio_write(PIN_DTD, HIGH);
    }
#endif
}

uint8_t GpioScsi::dataIn(void)
{
#ifndef ONPC
    // data bits are at these GPIO pins:
    // D7 D6 D5 D4 D3 D2 D1 D0
    // 17 16 15 14 13 12 11 10

    // single GPIO read
    volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPLEV0/4;
    uint32_t value = bcm2835_peri_read(paddr);
    uint8_t data = (uint8_t) (value >> 10);
    data = ~data;                               // invert incomming data
#else
    uint8_t data = 0;
#endif

    return data;
}

void GpioScsi::dataOut(uint8_t data)
{
#ifndef ONPC
    // data bits are at these GPIO pins:
    // DP D7 D6 D5 D4 D3 D2 D1 D0
    // 18 17 16 15 14 13 12 11 10

    data = ~data;                                   // invert outgoing data
    uint32_t data32 = (uint32_t) data;
    data32 = data32 << 10;                          // shift data on their position

    uint32_t oddParity = parityTable[data];         // get parity bit of data byta
    oddParity = oddParity << 18;                    // shift parity bit to DATAP position
    data32 = data32 | oddParity;                    // add parity bit

    bcm2835_gpio_write_mask(data32, 0x0007FC00);    // write value with mask
#endif
}

void GpioScsi::setPhaseBits(uint8_t sendNotRecv, bool cmdNotData, bool MSG)
{
#ifndef ONPC
    uint32_t phaseBits = 0;
    static uint32_t prevPhaseBits = 0xffffffff;

    if(sendNotRecv == DIR_RECV) {       // on WRITE from computer == RECV direction -> I/O should be high
        phaseBits |= PIN_BIT_IO;
    }

    if(!cmdNotData) {                   // if this is DATA, set C/D to H
        phaseBits |= PIN_BIT_CD;
    }

    if(!MSG) {                          // if not message phase, MSG should be H
        phaseBits |= PIN_BIT_MSG;
    }
    
    if(prevPhaseBits == phaseBits) {    // phase bits not changed since last time? nothing to do
        return;
    }
    prevPhaseBits = phaseBits;

    bcm2835_gpio_write_mask(phaseBits, PIN_MASK_MSG_CD_IO);    // write value with mask
#endif
}

void GpioScsi::setBsy(bool bsy)
{
#ifndef ONPC
    static bool prevBsy = false;
   
    if(prevBsy == bsy) {    // no change in BSY bit, ignore
        return;
    }
    prevBsy = bsy;

    int bsyLev = bsy ? LOW : HIGH;                  // if BSY asserted, set it to L, otherwise release to H
    int actTadLev = bsy ? HIGH : LOW;               // if BSY asserted, ACT and TAD need to be H, so we can output phase signals and bsy to bus

    // Debug::out(LOG_DEBUG, "setBsy - PIN_BSY: %d, PIN_TAD: %d, PIN_ACT: %d", bsyLev, actTadLev, actTadLev);
    bcm2835_gpio_write(PIN_BSY, bsyLev);
    bcm2835_gpio_write(PIN_TAD, actTadLev);
    bcm2835_gpio_write(PIN_ACT, actTadLev);
#endif
}

void GpioScsi::timeoutStart(uint32_t durationMs)
{
    timeoutCount = IS_TIMEOUT_CACHED_COUNT;     // set count to max expected value, so next call to isTimeout() will actually do the real time check
    timeoutTime = Utils::getEndTime(durationMs);
}

bool GpioScsi::isTimeout(void)
{
    static bool lastIsTimeout = false;

    // if not enough calls happened, just return the last isTimeout value
    if(timeoutCount < IS_TIMEOUT_CACHED_COUNT) {
        timeoutCount++;             // increment
        return lastIsTimeout;       // return cached value instead of time calculation
    }

    // we've used all the calls to avoid the system call, now we really have to to it
    timeoutCount = 0;               // restart counter
    lastIsTimeout = (Utils::getCurrentMs() >= timeoutTime); // check if timeout happened and cache it for future calls
    return lastIsTimeout;           // return this fresh value of isTimeout
}

const char* GpioScsi::getPhaseStr(int phase)
{
    switch(phase) {
        case SCSI_PHASE_BUSFREE:    return "BUSFREE";
        case SCSI_PHASE_SELECTION:  return "SELECTION";
        case SCSI_PHASE_COMMAND:    return "COMMAND";
        case SCSI_PHASE_DATAIN:     return "DATAIN";
        case SCSI_PHASE_DATAOUT:    return "DATAOU";
        case SCSI_PHASE_STATUS:     return "STATUS";
        case SCSI_PHASE_MSGIN:      return "MSGIN";
        case SCSI_PHASE_MSGOUT:     return "MSGOUT";
        default:                    return "???";
    }
}

void GpioScsi::setPhase(int phase) {
    static int prevPhase = -1;

    if(prevPhase == phase) {                // phase not changed? just quit
        return;
    }
    prevPhase = phase;                      // store this phase for next call
    // Debug::out(LOG_DEBUG, "GpioScsi::setPhase -> %d -> %s", phase, getPhaseStr(phase));

    switch(phase) {
        case SCSI_PHASE_BUSFREE:
            setBsy(false);                  // not busy when BUS FREE
            setDataDirection(DIR_RECV);
            setPhaseBits(DIR_RECV, false, false);
            break;

        case SCSI_PHASE_SELECTION:
            setBsy(false);                   // not busy when start of selection
            setPhaseBits(DIR_RECV, false, false);
            setDataDirection(DIR_RECV);
            break;

        case SCSI_PHASE_COMMAND:
            setBsy(true);                   // we're busy now!
            setPhaseBits(DIR_RECV, true, false);
            setDataDirection(DIR_RECV);
            break;

        case SCSI_PHASE_MSGOUT:             // from initiator to device
            setBsy(true);                   // we're busy now!
            setPhaseBits(DIR_RECV, true, true);
            setDataDirection(DIR_RECV);
            break;

        case SCSI_PHASE_DATAOUT:            // from initiator to device
            setPhaseBits(DIR_RECV, false, false);
            setDataDirection(DIR_RECV);
            break;

        case SCSI_PHASE_DATAIN:             // from device to initiator
            setPhaseBits(DIR_SEND, false, false);
            setDataDirection(DIR_SEND);
            break;

        case SCSI_PHASE_STATUS:
            setPhaseBits(DIR_SEND, true, false);
            setDataDirection(DIR_SEND);
            break;

        case SCSI_PHASE_MSGIN:              // from device to initiator
            setPhaseBits(DIR_SEND, true, true);
            setDataDirection(DIR_SEND);
            break;
    }
}

void GpioScsi::reset(void)
{
    setPhase(SCSI_PHASE_BUSFREE);
}
