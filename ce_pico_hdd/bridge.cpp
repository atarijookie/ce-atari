#include "defs.h"
#include "bridge.h"
#include "utils.h"

#include "cmd_first.pio.h"
#include "cmd_write.pio.h"
#include "cmd_read.pio.h"

extern uint8_t brStat; // status from bridge
extern uint8_t isAcsiNotScsi;
extern uint8_t lastScsiStatusByte;
extern uint8_t busIdle;

uint8_t pioReadFailed;

const int dataPins[8] = {PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7};

uint8_t PIO_gotFirstCmdByte(void)
{
    return false;
    // return BIT_IS_H(PIN_CMD1ST);
}

// get 1st CMD byte from ST  -- without setting INT
uint8_t PIO_writeFirst(void)
{
    uint8_t val;

    // BIT_CLR(PIN_FF12D);         // FF12D must be L for generating INT / DRQ signals

    // timeoutStart();             // start the timeout timer
    // setDataDirection(DIR_RECV); // data as inputs (write)

    // init vars,
    brStat = E_OK;   // init bridge status to E_OK
    return dataIn(); // read data after EOT
}

// get next CMD byte from ST -- with setting INT to LOW and waiting for CS
uint8_t PIO_write(void)
{
    // BIT_SET(PIN_INT_TRIG);          // do CLK pulse
    // DELAY_NS;
    // BIT_CLR(PIN_INT_TRIG);          // CLK back to L

    // if (!waitForEOT())
    // {                       // EOT didn't come?
    //     brStat = E_TimeOut; // set the bridge status
    //     return 0;
    // }

    // return dataIn(); // read data after EOT
    return 0;
}

// send status byte to host, and on SCSI also to MSG IN byte
void PIO_read(uint8_t scsiStatusByte)
{
    // pioReadFailed = FALSE; // didn't fail (yet)

    // if (brStat != E_TimeOut)
    // {                                        // if we didn't have bridge timeout, we can try to send STATUS byte
    //     lastScsiStatusByte = scsiStatusByte; // store last SCSI status byte - for debugging purpose
    //     PIO_read_solely(scsiStatusByte);     // this sends only STATUS byte to host - both in ACSI and SCSI
    // }

    // if (!isAcsiNotScsi)
    // { // if it's SCSI, send also MSG IN to host
    //     if (brStat != E_TimeOut)
    //     { // if we didn't have bridge timeout, we can try to send MSG IN
    //         MSG_read(0);
    //     }
    // }

    // if (brStat != E_OK)
    // { // if some timeout occured, then failed
    //     pioReadFailed = TRUE;
    // }

    // resetBridge();              // reset XILINX - put BSY, C/D, I/O in released states - needed for SCSI, doesn't harm anything in ACSI
    // setDataDirection(DIR_RECV); // data as inputs (write)
}

void PIO_read_solely(uint8_t val)
{
    // setDataDirection(DIR_SEND); // finish with send status
    // dataOut(val);               // output data to GPIO pins

    // BIT_SET(PIN_INT_TRIG);      // do CLK pulse
    // DELAY_NS;
    // BIT_CLR(PIN_INT_TRIG);      // CLK back to L

    // uint8_t ok = waitForEOT(); // try to wait for EOT and return success / failure

    // if (!ok)
    // {
    //     brStat = E_TimeOut; // set the bridge status
    // }

    // resetBridge();
}

// send MESSAGE IN byte to ST
void MSG_read(uint8_t val)
{
}

void DMA_read(uint8_t val)
{
    // dataOut(val);               // output data to GPIO pins

    // BIT_SET(PIN_DRQ_TRIG);      // do CLK pulse
    // DELAY_NS;
    // BIT_CLR(PIN_DRQ_TRIG);      // CLK back to L

    // uint8_t ok = waitForEOT(); // try to wait for EOT and return success / failure

    // if (!ok)
    // {
    //     brStat = E_TimeOut; // set the bridge status
    // }
}

uint8_t DMA_write(void)
{
    // BIT_SET(PIN_DRQ_TRIG);      // do CLK pulse
    // DELAY_NS;
    // BIT_CLR(PIN_DRQ_TRIG);      // CLK back to L

    // if (!waitForEOT())
    // {                       // EOT didn't come?
    //     brStat = E_TimeOut; // set the bridge status
    //     return 0;
    // }

    // return dataIn(); // read data after EOT
    return 0;
}

void resetBridge(void)
{
    // setDataDirection(DIR_RECV);
    // brStat = E_OK; // set bridge status to OK

    // // disable all data driving (in and out), also reset CMD1ST
    // BIT_SET(PIN_FF12D);             // FF12D must be H to allow capturing of CMD1ST

    // // reset INT and DRQ if needed
    // if (BIT_IS_L(PIN_EOT))
    // {                                     // if DRQ or INT is L
    //     BIT_SET(PIN_INT_TRIG);          // do CLK pulse
    //     BIT_SET(PIN_DRQ_TRIG);

    //     DELAY_NS;

    //     // const char* eotLevel = BIT_IS_L(PIN_EOT) ? "LOW" : "HIGH";
    //     // Debug::out(LOG_DEBUG, "GpioAcsi::reset - did RESET INT/DRQ, now it's %s", eotLevel);
    // }
    // else
    // {
    //     // Debug::out(LOG_DEBUG, "GpioAcsi::reset - skipped the RESET of INT/DRQ as it was HIGH");
    // }

    // BIT_CLR(PIN_INT_TRIG);      // CLK back to L
    // BIT_CLR(PIN_DRQ_TRIG);
}

void getBridgeStatus(void)
{
//     setDataDirection(DIR_RECV);

// #ifdef HDD_ACSI
//     // ACSI bus is idle if FF12D is 1, OUT_OE is 1 (== RECV, input to esp), INT_TRIG and DRQ_TRIG are L
//     busIdle = BIT_IS_H(PIN_FF12D) && BIT_IS_H(PIN_OUT_OE) && BIT_IS_L(PIN_INT_TRIG) && BIT_IS_H(PIN_DRQ_TRIG);
//     isAcsiNotScsi = 1;
// #else
//     busIdle = TRUE; // TODO: check if bus idle
//     isAcsiNotScsi = 0;
// #endif
}

uint8_t isBusIdle(void) // get if the SCSI bus is idle (BSY high, REQ high) or busy
{
    // getBridgeStatus();
    // return busIdle;
    return 0;
}

void setDataDirection(uint8_t sendNotRecv)
{
    static uint8_t sendNotRecvNow = 0xff; // init with no data direction set yet

    if (sendNotRecvNow == sendNotRecv)
    { // direction not changed since last time? quit
        return;
    }

    sendNotRecvNow = sendNotRecv; // remember what we're just setting

    // if output from esp, set OUT_OE to L before switching esp pins directions
    if (sendNotRecv == DIR_SEND)
    {
        BIT_SET(PIN_DATA_DIR);
    }

    // set esp GPIO pins as outputs / inputs
    uint32_t dir = (sendNotRecv == DIR_SEND) ? OUTPUT : INPUT;

    for (int i = 0; i < 8; i++)
    {
        pinMode(dataPins[i], dir);
    }

    // if input to esp, set OUT_OE to H after switching esp pins directions
    if (sendNotRecv == DIR_RECV)
    {
        BIT_CLR(PIN_DATA_DIR);
    }
}

/*
    This method waits for EOT (end-of-transfer) to become H, after it's been set to L by chip and
    will be reset back to H by Atari after each transfered byte. It's waiting up to the timeout time
    and can fail if Atari doesn't transfer the current byte.
*/
uint8_t waitForEOT(void)
{
    while(!hasTimedOut)
    {

    }

    return FALSE;               // timeout? fail
}

/*
    This method waits for the EOT (which is INT & DRQ) to reach specified level, but only for a short while.
    It's used after doing rising CLK on TRIG_INT or TRIG_DRQ, and it's just waiting for the CLK to propagate
    the FF12D value through D flip-flow to Q output (either INT or DRQ), so we don't return TRIG to L too quickly.
    This doesn't wait for Atari response, but just for flip-flop response and should always succeed.
*/
void waitForEOTlevel(int level)
{
    for (int i = 0; i < 1000; i++)
    {

    }
}

uint8_t dataIn(void)
{
    uint8_t data = 0;

    uint32_t gpioValue = gpio_get_all();    // read all GPIO pins
    data = (gpioValue >> 2);                // shift 2 bits down to get data in the right place

    return data;
}

void dataOut(uint8_t data)
{
    uint32_t data32 = ((uint32_t) data) << 2;
    gpio_put_masked(0x3fc, data32);
}

void dumpPinStates(void)
{
    // Serial.print("CMD1ST: ");
    // Serial.print(BIT_IS_H(PIN_CMD1ST));

    // Serial.print(", EOT: ");
    // Serial.print(BIT_IS_H(PIN_EOT));

    // Serial.print(", OUT_OE: ");
    // Serial.print(BIT_IS_H(PIN_DATA_DIR));

    // Serial.print(", DRQ_TRIG: ");
    // Serial.print(BIT_IS_H(PIN_DRQ_TRIG));

    // Serial.print(", FF12D: ");
    // Serial.print(BIT_IS_H(PIN_FF12D));

    // Serial.print(", INT_TRIG: ");
    // Serial.println(BIT_IS_H(PIN_INT_TRIG));
}
