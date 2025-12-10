#include "defs.h"
#include "bridge.h"
#include "utils.h"

#include "scsi_write.pio.h"
#include "scsi_read.pio.h"

extern uint8_t brStat; // status from bridge
extern uint8_t busIdle;

static PIO pioScsiWrite, pioScsiRead;
static uint smScsiWrite, smScsiRead;

int readInProgressCount = 0;

void setDataDirection(uint8_t sendNotRecv, PIO pio)
{
    static uint8_t sendNotRecvNow = 0xff; // init with no data direction set yet
    static PIO pioNow = nullptr;

    if (sendNotRecvNow == sendNotRecv && pioNow == pio) { // direction not changed since last time? quit
        return;
    }

    sendNotRecvNow = sendNotRecv; // remember what we're just setting
    pioNow = pio;

    // if output from mcu, set OUT_OE to L before switching mcu pins directions
    if (sendNotRecv == DIR_SEND) {
        BIT_SET(PIN_DATA_DIR);
    }

    if(sendNotRecv == DIR_SEND) {   // outputs?
        gpio_set_dir_out_masked(DATA_PINS_MASK);
    } else {                        // inputs!
        gpio_set_dir_in_masked(DATA_PINS_MASK);
    }

    int pio_data_pins[8] = {PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7};
    for (int i = 0; i < 8; i++) {
        pio_gpio_init(pio, pio_data_pins[i]);
    }

    // if input to mcu, set OUT_OE to H after switching mcu pins directions
    if (sendNotRecv == DIR_RECV) {
        BIT_CLR(PIN_DATA_DIR);
    }
}

void pioConfigAll(void)
{
    bool success;
    uint offset;

    success = pio_claim_free_sm_and_add_program_for_gpio_range(&scsi_write_program, &pioScsiWrite, &smScsiWrite, &offset, PIN_D0, 26, true);
    if(!success) { debug("Failed to claim PIO SM 1\n"); while(1); }
    scsi_write_program_init(pioScsiWrite, smScsiWrite, offset, PIN_ACK, PIN_RST_CD_REQ_SCL);

    success = pio_claim_free_sm_and_add_program_for_gpio_range(&scsi_read_program, &pioScsiRead, &smScsiRead, &offset, PIN_D0, 26, true);
    if(!success) { debug("Failed to claim PIO SM 2\n"); while(1); }
    scsi_write_program_init(pioScsiRead, smScsiRead, offset, PIN_ACK, PIN_RST_CD_REQ_SCL);
}

void pioConfig(int newMode, bool force)
{
    static int currentMode = MODE_UNKNOWN;

    if(!force && newMode == currentMode) {    // no mode change? just quit
        return;
    }
    currentMode = newMode;

    // all state machines to halt
    pio_sm_set_enabled(pioScsiWrite, smScsiWrite, false);
    pio_sm_set_enabled(pioScsiRead, smScsiRead, false);

    gpio_set_function(PIN_RST_CD_REQ_SCL, GPIO_FUNC_SIO);    // DRQ is controlled by gpio, always driving H
    gpio_put(PIN_RST_CD_REQ_SCL, 1);

    pio_sm_set_pins_with_mask(pioScsiWrite, smScsiWrite, (1 << PIN_RST_CD_REQ_SCL), (1 << PIN_RST_CD_REQ_SCL));     // INT and DRQ to H in PIO SM output
    pio_sm_set_pins_with_mask(pioScsiRead, smScsiRead, (1 << PIN_RST_CD_REQ_SCL), (1 << PIN_RST_CD_REQ_SCL));     // INT and DRQ to H in PIO SM output

    PIO whichPio;
    uint whichSm;

    switch(newMode)
    {
        case MODE_RESET:
        case MODE_SCSI_SELECTION:
            // configAcsiFirst();

            // whichPio = pioAcsiFirst;
            // whichSm = smAcsiFirst;
            break;

        case MODE_CMD:
            // pio_gpio_init(pioScsiWrite, PIN_INT);   // INT is controlled by PIO

            whichPio = pioScsiWrite;
            whichSm = smScsiWrite;
            break;

        case MODE_DMA_READ:
            pio_gpio_init(pioScsiRead, PIN_RST_CD_REQ_SCL);   // DRQ is controlled by PIO
            pio_sm_set_pindirs_with_mask64(pioScsiRead, smScsiRead, DATA_PINS_MASK, DATA_PINS_MASK);

            readInProgressCount = 0;        // no read bytes in progress

            whichPio = pioScsiRead;
            whichSm = smScsiRead;
            break;

        case MODE_DMA_WRITE:
            pio_gpio_init(pioScsiWrite, PIN_RST_CD_REQ_SCL);   // DRQ is controlled by PIO

            whichPio = pioScsiWrite;
            whichSm = smScsiWrite;
            break;

        case MODE_STATUS:
            // pio_gpio_init(pioScsiRead, PIN_INT);   // INT is controlled by PIO
            // pio_sm_set_pindirs_with_mask64(pioScsiRead, smScsiRead, DATA_PINS_MASK, DATA_PINS_MASK);

            whichPio = pioScsiRead;
            whichSm = smScsiRead;
            break;
    }

    // data direction RECV for CMD and WRITE, data direction SEND for READ and STATUS
    uint8_t sendNotRecv = (newMode == MODE_DMA_READ || newMode == MODE_STATUS) ? DIR_SEND : DIR_RECV;
    setDataDirection(sendNotRecv, whichPio);

    // if we're in the reset mode, don't enable any PIO SM
    if(newMode == MODE_RESET || newMode == MODE_SCSI_SELECTION) {
        return;
    }

    // restart state machine, clear FIFOs, enable state machine
    pio_sm_restart(whichPio, whichSm);
    pio_sm_clear_fifos(whichPio, whichSm);
    pio_sm_set_enabled(whichPio, whichSm, true);
}

uint8_t PIO_gotFirstCmdByte(void)
{
    if(BIT_IS_L(PIN_RST_CD_REQ_SCL)) {   // no new cmd bytes when ACSI RESET is L
        return false;
    }

    return false;
    // pioConfig(MODE_SCSI_SELECTION);
    // return !pio_sm_is_rx_fifo_empty(pioAcsiFirst, smAcsiFirst);
}

// get 1st CMD byte from ST  -- without setting INT
uint8_t PIO_writeFirst(void)
{
    uint8_t val;

    timeoutStart();             // start the timeout timer

    brStat = E_OK;              // init bridge status to E_OK
    return 0;
    // return ((uint8_t) (pio_sm_get(pioAcsiFirst, smAcsiFirst) >> 24));
}

// get next CMD byte from ST -- with setting INT to LOW and waiting for CS
uint8_t PIO_write(void)
{
    pioConfig(MODE_CMD);
    pio_sm_put(pioScsiWrite, smScsiWrite, 0);      // write N-1 count of bytes to write here

    while(1)
    {
        if(hasTimedOut) {       // on timeout
            brStat = E_TimeOut; // set the bridge status
            return 0;
        }

        // on rx fifo has data
        if(!pio_sm_is_rx_fifo_empty(pioScsiWrite, smScsiWrite)) {
            break;
        }
    }

    return ((uint8_t) (pio_sm_get(pioScsiWrite, smScsiWrite) >> 24));
}

// send status byte to host, and on SCSI also to MSG IN byte
void PIO_read(uint8_t scsiStatusByte)
{
    if (brStat != E_TimeOut)
    {                                        // if we didn't have bridge timeout, we can try to send STATUS byte
        PIO_read_solely(scsiStatusByte);     // this sends only STATUS byte to host
    }

    // if we didn't have bridge timeout, we can try to send MSG IN
    if (brStat != E_TimeOut)
    {
        MSG_read(0);
    }

    if (brStat != E_OK)
    { // if some timeout occured, then failed

    }

    // resetBridge();               // reset XILINX - put BSY, C/D, I/O in released states - needed for SCSI
}

void PIO_read_solely(uint8_t val)
{
    pioConfig(MODE_STATUS);

    pio_sm_put(pioScsiRead, smScsiRead, val);     // write status byte to TX FIFO, the transfer will start

    // wait for data to be transfered
    while(1)
    {
        if(hasTimedOut) {       // on timeout
            brStat = E_TimeOut; // set the bridge status
            return;
        }

        // on rx fifo has data, this means that transfer has finished with success
        if(!pio_sm_is_rx_fifo_empty(pioScsiRead, smScsiRead)) {
            uint32_t tmp = pio_sm_get(pioScsiRead, smScsiRead);
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
        if(!pio_sm_is_rx_fifo_empty(pioScsiRead, smScsiRead)) {
            uint32_t tmp = pio_sm_get(pioScsiRead, smScsiRead);
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
        if(!pio_sm_is_rx_fifo_empty(pioScsiRead, smScsiRead)) {
            uint32_t tmp = pio_sm_get(pioScsiRead, smScsiRead);
            readInProgressCount--;
        }

        // READ TX FIFO not full, we can push to fifo
        if(!pio_sm_is_tx_fifo_full(pioScsiRead, smScsiRead)) {
           // put current byte in TX FIFO, increment readInProgressCount
           pio_sm_put(pioScsiRead, smScsiRead, val);
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
    pio_sm_put(pioScsiWrite, smScsiWrite, transfersCount - 1);        // write N-1 count of bytes to write here
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
        if(!pio_sm_is_rx_fifo_empty(pioScsiWrite, smScsiWrite)) {
            break;
        }
    }

    return ((uint8_t) (pio_sm_get(pioScsiWrite, smScsiWrite) >> 24));
}

void resetBridge(void)
{
    pioConfig(MODE_SCSI_SELECTION, true);
    brStat = E_OK; // set bridge status to OK
}

