#include "defs.h"
#include "bridge.h"
#include "utils.h"

#include "acsi_first.pio.h"
#include "acsi_write.pio.h"
#include "acsi_read.pio.h"

extern uint8_t brStat; // status from bridge
extern uint8_t isAcsiNotScsi;
extern uint8_t lastScsiStatusByte;
extern uint8_t busIdle;

static PIO pioAcsiFirst, pioAcsiCmdWrite, pioAcsiDataWrite, pioAcsiDataRead, pioAcsiStatusRead;
static uint smAcsiFirst, smAcsiCmdWrite, smAcsiDataWrite, smAcsiDataRead, smAcsiStatusRead;

int readInProgressCount = 0;

void pioConfigAll(void)
{
    bool success;
    uint offset;

    success = pio_claim_free_sm_and_add_program_for_gpio_range(&acsi_first_program, &pioAcsiFirst, &smAcsiFirst, &offset, PIN_D0, 26, true);
    if(!success) { debug("Failed to claim PIO SM 1\n"); while(1); }
    acsi_first_program_init(pioAcsiFirst, smAcsiFirst, offset);

    success = pio_claim_free_sm_and_add_program_for_gpio_range(&acsi_write_program, &pioAcsiCmdWrite, &smAcsiCmdWrite, &offset, PIN_D0, 26, true);
    if(!success) { debug("Failed to claim PIO SM 2\n"); while(1); }
    acsi_write_program_init(pioAcsiCmdWrite, smAcsiCmdWrite, offset, PIN_CS, PIN_INT);

    success = pio_claim_free_sm_and_add_program_for_gpio_range(&acsi_write_program, &pioAcsiDataWrite, &smAcsiDataWrite, &offset, PIN_D0, 26, true);
    if(!success) { debug("Failed to claim PIO SM 3\n"); while(1); }
    acsi_write_program_init(pioAcsiDataWrite, smAcsiDataWrite, offset, PIN_ACK, PIN_DRQ);

    success = pio_claim_free_sm_and_add_program_for_gpio_range(&acsi_read_program, &pioAcsiDataRead, &smAcsiDataRead, &offset, PIN_D0, 26, true);
    if(!success) { debug("Failed to claim PIO SM 4\n"); while(1); }
    acsi_read_program_init(pioAcsiDataRead, smAcsiDataRead, offset, PIN_ACK, PIN_DRQ);

    success = pio_claim_free_sm_and_add_program_for_gpio_range(&acsi_read_program, &pioAcsiStatusRead, &smAcsiStatusRead, &offset, PIN_D0, 26, true);
    if(!success) { debug("Failed to claim PIO SM 5\n"); while(1); }
    acsi_read_program_init(pioAcsiStatusRead, smAcsiStatusRead, offset, PIN_CS, PIN_INT);
}

void pioConfig(int newMode, bool force)
{
    static int currentMode = MODE_UNKNOWN;

    if(!force && newMode == currentMode) {    // no mode change? just quit
        return;
    }
    currentMode = newMode;

    // all state machines to halt
    pio_sm_set_enabled(pioAcsiFirst, smAcsiFirst, false);
    pio_sm_set_enabled(pioAcsiCmdWrite, smAcsiCmdWrite, false);
    pio_sm_set_enabled(pioAcsiDataWrite, smAcsiDataWrite, false);
    pio_sm_set_enabled(pioAcsiDataRead, smAcsiDataRead, false);
    pio_sm_set_enabled(pioAcsiStatusRead, smAcsiStatusRead, false);

    gpio_set_function(PIN_DRQ, GPIO_FUNC_SIO);    // DRQ is controlled by gpio, always driving H
    gpio_put(PIN_DRQ, 1);

    gpio_set_function(PIN_INT, GPIO_FUNC_SIO);    // INT is controlled by gpio, always driving H
    gpio_put(PIN_INT, 1);

    pio_sm_set_pins_with_mask(pioAcsiCmdWrite, smAcsiCmdWrite, HANDSHAKE_OUT_PINS, HANDSHAKE_OUT_PINS);     // INT and DRQ to H in PIO SM output
    pio_sm_set_pins_with_mask(pioAcsiDataWrite, smAcsiDataWrite, HANDSHAKE_OUT_PINS, HANDSHAKE_OUT_PINS);   // INT and DRQ to H in PIO SM output
    pio_sm_set_pins_with_mask(pioAcsiDataRead, smAcsiDataRead, HANDSHAKE_OUT_PINS, HANDSHAKE_OUT_PINS);     // INT and DRQ to H in PIO SM output
    pio_sm_set_pins_with_mask(pioAcsiStatusRead, smAcsiStatusRead, HANDSHAKE_OUT_PINS, HANDSHAKE_OUT_PINS);       // INT and DRQ to H in PIO SM output

    // data direction RECV for CMD and WRITE, data direction SEND for READ and STATUS
    uint8_t sendNotRecv = (newMode == MODE_DMA_READ || newMode == MODE_STATUS) ? DIR_SEND : DIR_RECV;
    setDataDirection(sendNotRecv);

    // if we're in the reset mode, don't enabble any PIO SM
    if(newMode == MODE_RESET) {
        return;
    }

    PIO whichPio;
    uint whichSm;

    switch(newMode)
    {
        case MODE_ACSI_FIRST:
            configAcsiFirst();

            whichPio = pioAcsiFirst;
            whichSm = smAcsiFirst;
            break;

        case MODE_CMD_REST:
            pio_gpio_init(pioAcsiCmdWrite, PIN_INT);   // INT is controlled by PIO

            whichPio = pioAcsiCmdWrite;
            whichSm = smAcsiCmdWrite;
            break;

        case MODE_DMA_READ:
            pio_gpio_init(pioAcsiDataRead, PIN_DRQ);   // DRQ is controlled by PIO

            readInProgressCount = 0;        // no read bytes in progress

            whichPio = pioAcsiDataRead;
            whichSm = smAcsiDataRead;
            break;

        case MODE_DMA_WRITE:
            pio_gpio_init(pioAcsiDataWrite, PIN_DRQ);   // DRQ is controlled by PIO

            whichPio = pioAcsiDataWrite;
            whichSm = smAcsiDataWrite;
            break;

        case MODE_STATUS:
            pio_gpio_init(pioAcsiStatusRead, PIN_INT);   // INT is controlled by PIO

            whichPio = pioAcsiStatusRead;
            whichSm = smAcsiStatusRead;
            break;
    }

    // restart state machine, clear FIFOs, enable state machine
    pio_sm_restart(whichPio, whichSm);
    pio_sm_clear_fifos(whichPio, whichSm);
    pio_sm_set_enabled(whichPio, whichSm, true);
}

uint8_t PIO_gotFirstCmdByte(void)
{
    if(BIT_IS_L(PIN_RESET)) {   // no new cmd bytes when ACSI RESET is L
        return false;
    }

    pioConfig(MODE_ACSI_FIRST);
    return !pio_sm_is_rx_fifo_empty(pioAcsiFirst, smAcsiFirst);
}

// get 1st CMD byte from ST  -- without setting INT
uint8_t PIO_writeFirst(void)
{
    uint8_t val;

    timeoutStart();             // start the timeout timer

    brStat = E_OK;              // init bridge status to E_OK
    return ((uint8_t) (pio_sm_get(pioAcsiFirst, smAcsiFirst) >> 24));
}

// get next CMD byte from ST -- with setting INT to LOW and waiting for CS
uint8_t PIO_write(void)
{
    pio_sm_put(pioAcsiCmdWrite, smAcsiCmdWrite, 0);      // write N-1 count of bytes to write here

    while(1)
    {
        if(hasTimedOut) {       // on timeout
            brStat = E_TimeOut; // set the bridge status
            return 0;
        }

        // on rx fifo has data
        if(!pio_sm_is_rx_fifo_empty(pioAcsiCmdWrite, smAcsiCmdWrite)) {
            break;
        }
    }

    return ((uint8_t) (pio_sm_get(pioAcsiCmdWrite, smAcsiCmdWrite) >> 24));
}

// send status byte to host, and on SCSI also to MSG IN byte
void PIO_read(uint8_t scsiStatusByte)
{
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

    }

    // resetBridge();               // reset XILINX - put BSY, C/D, I/O in released states - needed for SCSI, doesn't harm anything in ACSI
    setDataDirection(DIR_RECV);     // data as inputs (write)
}

void PIO_read_solely(uint8_t val)
{
    pioConfig(MODE_STATUS);

    pio_sm_put(pioAcsiStatusRead, smAcsiStatusRead, val);     // write status byte to TX FIFO, the transfer will start

    // wait for data to be transfered
    while(1)
    {
        if(hasTimedOut) {       // on timeout
            brStat = E_TimeOut; // set the bridge status
            return;
        }

        // on rx fifo has data, this means that transfer has finished with success
        if(!pio_sm_is_rx_fifo_empty(pioAcsiStatusRead, smAcsiStatusRead)) {
            uint32_t tmp = pio_sm_get(pioAcsiStatusRead, smAcsiStatusRead);
            break;
        }
    }

    brStat = E_OK;
}

// send MESSAGE IN byte to ST
void MSG_read(uint8_t val)
{
}

void DMA_read_waitForEnd(void)
{
    // wait while read is still in progress (from FIFO to Atari)
    while(1) {
        if(readInProgressCount <= 0) {  // nothing in progress? this the normal end
            readInProgressCount = 0;
            return;
        }

        // RX FIFO not empty? read it, decrement readInProgressCount
        if(!pio_sm_is_rx_fifo_empty(pioAcsiDataRead, smAcsiDataRead)) {
            uint32_t tmp = pio_sm_get(pioAcsiDataRead, smAcsiDataRead);
            readInProgressCount--;
        }

        if(hasTimedOut) {       // on timeout
            brStat = E_TimeOut; // set the bridge status
            return;
        }
    }
}

void DMA_read(uint8_t val)
{
    // wait for TX fifo not full, so we can put the current value in
    while(1) {
        // RX FIFO not empty? read it, decrement readInProgressCount
        if(!pio_sm_is_rx_fifo_empty(pioAcsiDataRead, smAcsiDataRead)) {
            uint32_t tmp = pio_sm_get(pioAcsiDataRead, smAcsiDataRead);
            readInProgressCount--;
        }

        // READ TX FIFO not full, we can push to fifo
        if(!pio_sm_is_tx_fifo_full(pioAcsiDataRead, smAcsiDataRead)) {
           // put current byte in TX FIFO, increment readInProgressCount
           pio_sm_put(pioAcsiDataRead, smAcsiDataRead, val);
           readInProgressCount++;
           return;
        }

        if(hasTimedOut) {       // on timeout
            brStat = E_TimeOut; // set the bridge status
            return;
        }
    }
}

void DMA_write_startWithCount(uint32_t transfersCount)
{
    pio_sm_put(pioAcsiDataWrite, smAcsiDataWrite, transfersCount - 1);        // write N-1 count of bytes to write here
}

uint8_t DMA_write(void)
{
    while(1)
    {
        if(hasTimedOut) {       // on timeout
            brStat = E_TimeOut; // set the bridge status
            return 0;
        }

        // on rx fifo has data
        if(!pio_sm_is_rx_fifo_empty(pioAcsiDataWrite, smAcsiDataWrite)) {
            break;
        }
    }

    return ((uint8_t) (pio_sm_get(pioAcsiDataWrite, smAcsiDataWrite) >> 24));
}

void resetBridge(void)
{
    pioConfig(MODE_ACSI_FIRST, true);
    brStat = E_OK; // set bridge status to OK
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
