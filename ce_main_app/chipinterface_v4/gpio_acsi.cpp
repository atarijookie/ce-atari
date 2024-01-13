#include "../utils.h"
#include "../debug.h"
#include "gpio_acsi.h"

GpioAcsi::GpioAcsi()
{
    hddEnabledIDs = 1;      // ID 0 enabled
    sdCardId = 0xff;        // SD card not enabled
    sendNotRecvNow = 0xff;  // no data direction set yet
}

GpioAcsi::~GpioAcsi()
{

}

void GpioAcsi::init(void)
{
#ifndef ONPC
    // set RECV direction (reading data into RPi)
    setDataDirection(0);

    // set these as inputs
    int inputs[3] = {CMD1ST, EOT, PIN_ATN_FRANZ};
    for(int i=0; i<3; i++) {
        bcm2835_gpio_fsel(inputs[i],  BCM2835_GPIO_FSEL_INPT);
    }

    // configure those as outputs
    int outputs[5] = {INT_TRIG, DRQ_TRIG, FF12D, IN_OE, OUT_OE};
    int outVals[5] = {LOW,      LOW,      LOW,   HIGH,  HIGH  };
    for(int i=0; i<5; i++) {
        bcm2835_gpio_fsel(outputs[i],  BCM2835_GPIO_FSEL_OUTP);
        bcm2835_gpio_write(outputs[i], outVals[i]);
    }

    // reset INT and DRQ so they won't block ACSI bus
    reset();
#endif
}

void GpioAcsi::reset(void)
{
#ifndef ONPC
    // disable all data driving (in and out), also reset CMD1ST
    bcm2835_gpio_write(IN_OE,    LOW);      // IN_OE to L - resets CMD1ST
    bcm2835_gpio_write(OUT_OE,   HIGH);     // don't drive output data
    bcm2835_gpio_write(IN_OE,    HIGH);     // don't drive input data

    // reset INT and DRQ if needed
    if(bcm2835_gpio_lev(EOT) == LOW) {          // if DRQ or INT is L
        bcm2835_gpio_write(FF12D,    HIGH);     // we want the signals to go H
        Utils::sleepMs(1);
        bcm2835_gpio_write(INT_TRIG, HIGH);     // do CLK pulse
        bcm2835_gpio_write(DRQ_TRIG, HIGH);

        waitForEOTlevel(HIGH);                  // wait a while until INT and DRQ come back to H again
    }

    bcm2835_gpio_write(FF12D,    LOW);      // FF12D must be L for normal usage
    bcm2835_gpio_write(INT_TRIG, LOW);      // CLK back to L
    bcm2835_gpio_write(DRQ_TRIG, LOW);
#endif
}

void GpioAcsi::setConfig(uint8_t hddEnabledIDs, uint8_t sdCardId)
{
    this->hddEnabledIDs = hddEnabledIDs;
    this->sdCardId = sdCardId;
}

uint8_t GpioAcsi::getXilinxByte(void)
{
    return 0x41;
}

bool GpioAcsi::getCmd(uint8_t* cmd)
{
    setDataDirection(0);                    // start with recv cmd

#ifndef ONPC
    if(bcm2835_gpio_lev(CMD1ST) == LOW) {   // CMD1ST not H? no cmd waiting
        return false;
    }

    // if CMD1ST is H, then 1st cmd byte is waiting
    cmd[0] = dataIn();
    uint8_t id = (cmd[0] >> 5) & 0x07;      // get only device ID

    Debug::out(LOG_DEBUG, "GpioAcsi::getCmd - got 0th cmd byte: %02x -> id: %d", cmd[0], id);

    //----------------------
    if(!(hddEnabledIDs & (1 << id))) {      // if this ID is not enabled, quit
        Debug::out(LOG_DEBUG, "GpioAcsi::getCmd - hddEnabledIDs: %02x -> id: %d not enabled, ignoring", hddEnabledIDs, id);
        reset();
        return false;
    }
    
    uint8_t cmdLen = 6;                     // maximum 6 bytes at start, but this might change in getCmdLengthFromCmdBytes()
            
    for(uint8_t i=1; i<cmdLen; i++) {       // receive the next command bytes
        cmd[i] = getCmdByte();

        if(isTimeout()) {                   // if something was wrong, quit, failed
            Debug::out(LOG_DEBUG, "GpioAcsi::getCmd - timeout on cmd byte %d", i);
            reset();
            return false;
        }
        
        if(i == 1) {                        // if we got also the 2nd byte, get actual cmd length
            cmdLen = getCmdLengthFromCmdBytesAcsi(cmd);
            Debug::out(LOG_DEBUG, "GpioAcsi::getCmd - for cmd: %02x %02x -> cmdLen: %d", cmd[0], cmd[1], cmdLen);
        }             
    }

    if(cmdLen > 6) {
        Debug::out(LOG_DEBUG, "GpioAcsi::getCmd - got cmd: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", 
                                cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9]);
    } else {
        Debug::out(LOG_DEBUG, "GpioAcsi::getCmd - got cmd: %02x %02x %02x %02x %02x %02x", 
                                cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5]);
    }

    return true;
#else
    return false;
#endif
}

void GpioAcsi::startTransfer(uint8_t sendNotRecv, uint32_t totalDataCount, uint8_t scsiStatus, bool withStatus)
{
    this->sendNotRecv = sendNotRecv;
    this->totalDataCount = totalDataCount;
    this->scsiStatus = scsiStatus;
    this->withStatus = withStatus;

    timeoutStart(1000);     // start timeout
}

bool GpioAcsi::sendBlock(uint8_t *pData, uint32_t dataCount)
{
    setDataDirection(1);    // send data

    for(uint32_t i=0; i<dataCount; i++) {
        dataOut(pData[i]);  // output data to RPi GPIO pins

#ifndef ONPC
        bcm2835_gpio_write(DRQ_TRIG, HIGH);     // do CLK pulse
        waitForEOTlevel(LOW);                   // wait until DRQ is L
        bcm2835_gpio_write(DRQ_TRIG, LOW);      // CLK back to L
#endif

        if(!waitForEOT()) { // failed to get EOT?
            return false;
        }
    }

    if(withStatus) {        // if block transfer should end with status, send it now
        return sendStatus(scsiStatus);
    }

    return true;            // all OK
}

bool GpioAcsi::recvBlock(uint8_t *pData, uint32_t dataCount)
{
    setDataDirection(0);    // recv data

    for(uint32_t i=0; i<dataCount; i++) {
#ifndef ONPC
        bcm2835_gpio_write(DRQ_TRIG, HIGH);     // do CLK pulse
        waitForEOTlevel(LOW);                   // wait until DRQ is L
        bcm2835_gpio_write(DRQ_TRIG, LOW);      // CLK back to L
#endif

        if(!waitForEOT()) { // failed to get EOT?
            return false;
        }

        pData[i] = dataIn(); // get data from GPIO pins
    }

    if(withStatus) {        // if block transfer should end with status, send it now
        return sendStatus(scsiStatus);
    }

    return true;            // all OK
}

uint8_t GpioAcsi::getCmdByte(void)
{
#ifndef ONPC
    bcm2835_gpio_write(INT_TRIG, HIGH);     // do CLK pulse
    waitForEOTlevel(LOW);                   // wait until INT is L
    bcm2835_gpio_write(INT_TRIG, LOW);      // CLK back to L
#endif

    if(!waitForEOT()) {     // EOT didn't come?
        return 0;
    }

    return dataIn();        // read data after EOT
}

bool GpioAcsi::sendStatus(uint8_t scsiStatus)
{
    setDataDirection(1);    // finish with send status
    dataOut(scsiStatus);    // output data to RPi GPIO pins

#ifndef ONPC
    bcm2835_gpio_write(INT_TRIG, HIGH);     // do CLK pulse
    waitForEOTlevel(LOW);                   // wait until INT is L
    bcm2835_gpio_write(INT_TRIG, LOW);      // CLK back to L
#endif

    return waitForEOT();    // try to wait for EOT and return success / failure
}

/*
    This method waits for EOT (end-of-transfer) to become H, after it's been set to L by RPi and
    will be reset back to H by Atari after each transfered byte. It's waiting up to the timeout time
    and can fail if Atari doesn't transfer the current byte.
*/
bool GpioAcsi::waitForEOT(void)
{
    while(true) {
#ifndef ONPC
        if(bcm2835_gpio_lev(EOT) == HIGH) {     // EOT is H? success
            return true;
        }
#endif

        if(isTimeout()) {   // timeout? fail
            return false;
        }
    }
}

/* 
    This method waits for the EOT (which is INT & DRQ) to reach specified level, but only for a short while.
    It's used after doing rising CLK on TRIG_INT or TRIG_DRQ, and it's just waiting for the CLK to propagate
    the FF12D value through D flip-flow to Q output (either INT or DRQ), so we don't return TRIG to L too quickly.
    This doesn't wait for Atari response, but just for flip-flop response and should always succeed. 
*/
void GpioAcsi::waitForEOTlevel(int level)
{
#ifndef ONPC
    for(int i=0; i<1000; i++) {
        if(bcm2835_gpio_lev(EOT) == level) {
            break;
        }
    }
#endif
}

void GpioAcsi::setDataDirection(uint8_t sendNotRecv)
{
    if(sendNotRecvNow == sendNotRecv) {     // direction not changed since last time? quit
        return;
    }

    sendNotRecvNow = sendNotRecv;           // remember what we're just setting

#ifndef ONPC
    // stop output of both bus transcievers
    bcm2835_gpio_write(IN_OE, HIGH);
    bcm2835_gpio_write(OUT_OE, HIGH);

    // set RPi GPIO pins as outputs / inputs
    uint32_t dir = sendNotRecv ? BCM2835_GPIO_FSEL_OUTP : BCM2835_GPIO_FSEL_INPT;
    int dataPins[8] = {DATA0, DATA1, DATA2, DATA3, DATA4, DATA5, DATA6, DATA7};
    for(int i=0; i<8; i++) {
        bcm2835_gpio_fsel(dataPins[i],  dir);
    }

    // enable only 1 transciever
    bcm2835_gpio_write(sendNotRecv ? OUT_OE : IN_OE, LOW);
#endif
}

uint8_t GpioAcsi::dataIn(void)
{
#ifndef ONPC
    // single GPIO read
    volatile uint32_t* paddr = bcm2835_gpio + BCM2835_GPLEV0/4;
    uint32_t value = bcm2835_peri_read(paddr);
    value = value & 0x1BF0000;          // leave only data bits

    uint32_t bits5to0 = value >> 16;
    uint32_t bits76 = value >> 17;
    uint32_t data = bits76 | bits5to0;  // merge bits

/*
    // GPIO read by bits
    uint8_t data = 0;
    int dataPins[8] = {DATA0, DATA1, DATA2, DATA3, DATA4, DATA5, DATA6, DATA7};
    for(int i=0; i<8; i++) {
        if(bcm2835_gpio_lev(dataPins[i]) == HIGH) {
            data = data | (1 << i);
        }
    }
*/

#else
    uint8_t data = 0;
#endif

    return data;
}

void GpioAcsi::dataOut(uint8_t data)
{
#ifndef ONPC
    // data bits are at these GPIO pins:
    // D7 D6     D5 D4 D3 D2 D1 D0
    // 24 23     21 20 19 18 17 16

    uint32_t data32 = (uint32_t) data;
    data32 = ((data32 & 0xc0) << 17) | ((data32 & 0x3f) << 16);     // data bits now moved to expected positions 24-23 + 21-16
    uint32_t invData = (~data32) & 0x1BF0000;    // inverted data bits

    bcm2835_gpio_set_multi(data32);     // set bits which should be 1
    bcm2835_gpio_clr_multi(invData);    // clear bits which should be 0
#endif
}

uint8_t GpioAcsi::getCmdLengthFromCmdBytesAcsi(uint8_t* cmd)
{
    uint8_t cmdLen = 6;     // non-ICD commands have length of 6 bytes

    // now it's time to set up the receiver buffer and length
    if((cmd[0] & 0x1f)==0x1f)   {                           // if the command is '0x1f'
        switch((cmd[1] & 0xe0)>>5)                          // get the length of the command
        {
            case  0: cmdLen =  7; break;
            case  1: cmdLen = 11; break;
            case  2: cmdLen = 11; break;
            case  5: cmdLen = 13; break;
            default: cmdLen =  7; break;
        }
    }

    return cmdLen;
}

void GpioAcsi::timeoutStart(uint32_t durationMs)
{
    timeoutTime = Utils::getEndTime(durationMs);
}

bool GpioAcsi::isTimeout(void)
{
    return (Utils::getCurrentMs() >= timeoutTime);
}
