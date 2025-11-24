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

static PIO pioCmdFirst, pioCmdWrite, pioCmdRead;
static uint smCmdFirst, smCmdWrite, smCmdRead;
static uint offsetCmdFirst, offsetCmdWrite, offsetCmdRead;

io_rw_8* cmdFirstFifo;      // first cmd byte will be stored here

io_rw_32* cmdWriteTxFifo;   // write N-1 count of bytes to write here
io_rw_8*  cmdWriteRxFifo;   // read N count of bytes from here

io_rw_8* cmdReadTxFifo;     // write bytes here to make them to be read by ST

void configCmdWriteForPIO(void);
void configCmdWriteForDMA(void);
void configCmdReadForPIO(void);
void configCmdReadForDMA(void);

void pioConfigAll(void)
{
    bool success;

    success = pio_claim_free_sm_and_add_program_for_gpio_range(&cmd_first_program, &pioCmdFirst, &smCmdFirst, &offsetCmdFirst, PIN_D0, 26, true);
    if(!success) { debug("Failed to claim PIO SM 1\n"); while(1); }
    cmd_first_program_init(pioCmdFirst, smCmdFirst, offsetCmdFirst, PIN_D0, PIN_A1);

    success = pio_claim_free_sm_and_add_program_for_gpio_range(&cmd_write_program, &pioCmdWrite, &smCmdWrite, &offsetCmdWrite, PIN_D0, 26, true);
    if(!success) { debug("Failed to claim PIO SM 2\n"); while(1); }
    cmd_write_program_init(pioCmdWrite, smCmdWrite, offsetCmdWrite, PIN_D0, PIN_INT, PIN_CS);

    success = pio_claim_free_sm_and_add_program_for_gpio_range(&cmd_read_program, &pioCmdRead, &smCmdRead, &offsetCmdRead, PIN_D0, 26, true);
    if(!success) { debug("Failed to claim PIO SM 3\n"); while(1); }
    cmd_read_program_init(pioCmdRead, smCmdRead, offsetCmdRead, PIN_D0, PIN_INT, PIN_CS);

    cmdFirstFifo = (io_rw_8*) &pioCmdFirst->rxf[smCmdFirst] + 3;
    cmdWriteTxFifo = (io_rw_32*) &pioCmdWrite->txf[smCmdWrite];
    cmdWriteRxFifo = (io_rw_8*) &pioCmdWrite->rxf[smCmdWrite] + 3;
    cmdReadTxFifo = (io_rw_8*) &pioCmdRead->txf[smCmdRead] + 3;
}

void pioConfig(int newMode, bool force)
{
    static int currentMode = MODE_UNKNOWN;

    if(!force && newMode == currentMode) {    // no mode change? just quit
        return;
    }
    currentMode = newMode;

    // all state machines to halt
    pio_sm_set_enabled(pioCmdFirst, smCmdFirst, false);
    pio_sm_set_enabled(pioCmdRead, smCmdRead, false);
    pio_sm_set_enabled(pioCmdWrite, smCmdWrite, false);

    // data direction RECV for CMD and WRITE, data direction SEND for READ and STATUS
    uint8_t sendNotRecv = (newMode == MODE_CMD_FIRST || newMode == MODE_CMD_REST || newMode == MODE_DMA_WRITE) ? DIR_RECV : DIR_SEND;
    setDataDirection(sendNotRecv);

    PIO whichPio;
    uint whichSm;

    switch(newMode)
    {
        case MODE_CMD_FIRST:
            whichPio = pioCmdFirst;
            whichSm = smCmdFirst;
            break;

        case MODE_CMD_REST:
            configCmdWriteForPIO();
            whichPio = pioCmdWrite;
            whichSm = smCmdWrite;
            break;

        case MODE_DMA_READ:
            configCmdReadForDMA();
            whichPio = pioCmdRead;
            whichSm = smCmdRead;
            break;

        case MODE_DMA_WRITE:
            configCmdWriteForDMA();
            whichPio = pioCmdWrite;
            whichSm = smCmdWrite;
            break;

        case MODE_STATUS:
            configCmdReadForPIO();
            whichPio = pioCmdRead;
            whichSm = smCmdRead;
            break;
    }

    // restart state machine, clear FIFOs, enable state machine
    pio_sm_restart(whichPio, whichSm);
    pio_sm_clear_fifos(whichPio, whichSm);
    pio_sm_set_enabled(whichPio, whichSm, true);
}

uint8_t PIO_gotFirstCmdByte(void)
{
    pioConfig(MODE_CMD_FIRST);
    return !pio_sm_is_rx_fifo_empty(pioCmdFirst, smCmdFirst);
}

// get 1st CMD byte from ST  -- without setting INT
uint8_t PIO_writeFirst(void)
{
    uint8_t val;

    timeoutStart();             // start the timeout timer

    brStat = E_OK;              // init bridge status to E_OK
    return ((uint8_t) *cmdFirstFifo);
}

// get next CMD byte from ST -- with setting INT to LOW and waiting for CS
uint8_t PIO_write(void)
{
    *cmdWriteTxFifo = 0;        // write N-1 count of bytes to write here

    while(1)
    {
        if(hasTimedOut) {       // on timeout
            brStat = E_TimeOut; // set the bridge status
            return 0;
        }

        // on rx fifo has data
        if(!pio_sm_is_rx_fifo_empty(pioCmdWrite, smCmdWrite)) {
            break;
        }
    }

    return ((uint8_t) *cmdWriteRxFifo);
}

// send status byte to host, and on SCSI also to MSG IN byte
void PIO_read(uint8_t scsiStatusByte)
{
    pioReadFailed = FALSE; // didn't fail (yet)

    if (brStat != E_TimeOut)
    {                                        // if we didn't have bridge timeout, we can try to send STATUS byte
        lastScsiStatusByte = scsiStatusByte; // store last SCSI status byte - for debugging purpose
        PIO_read_solely(scsiStatusByte);     // this sends only STATUS byte to host - both in ACSI and SCSI
    }

    // if (!isAcsiNotScsi)
    // { // if it's SCSI, send also MSG IN to host
    //     if (brStat != E_TimeOut)
    //     { // if we didn't have bridge timeout, we can try to send MSG IN
    //         MSG_read(0);
    //     }
    // }

    if (brStat != E_OK)
    { // if some timeout occured, then failed
        pioReadFailed = TRUE;
    }

    // resetBridge();               // reset XILINX - put BSY, C/D, I/O in released states - needed for SCSI, doesn't harm anything in ACSI
    setDataDirection(DIR_RECV);     // data as inputs (write)
}

void PIO_read_solely(uint8_t val)
{
    pioConfig(MODE_STATUS);

    *cmdReadTxFifo = val;

    // TODO: wait for data to be transfered

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
    pioConfig(MODE_CMD_FIRST, true);
    brStat = E_OK; // set bridge status to OK
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

    if (sendNotRecvNow == sendNotRecv) { // direction not changed since last time? quit
        return;
    }

    sendNotRecvNow = sendNotRecv; // remember what we're just setting

    // if output from mcu, set OUT_OE to L before switching mcu pins directions
    if (sendNotRecv == DIR_SEND) {
        BIT_SET(PIN_DATA_DIR);
    }

    if(sendNotRecv == DIR_SEND) {   // outputs?
        gpio_set_dir_out_masked(DATA_PINS_MASK);
    } else {                        // inputs!
        gpio_set_dir_in_masked(DATA_PINS_MASK);
    }

    // if input to mcu, set OUT_OE to H after switching mcu pins directions
    if (sendNotRecv == DIR_RECV) {
        BIT_CLR(PIN_DATA_DIR);
    }
}
