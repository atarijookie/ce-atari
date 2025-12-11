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

uint16_t byteWithParity[256];

// prefill the table with bytes inverted, extended with parity
void prefillBytesWithParityTable(void)
{
    for(int i=0; i<256; i++) {
        uint16_t valPar = i;
        uint8_t parity = __builtin_parity(i);

        if(!parity) {           // if odd parity, then set parity bit
            valPar |= 0x100;
        }

        byteWithParity[i] = (~valPar) & 0x1ff;
    }
}

int getAtnReset(void)
{
    // get pin directions
    int dirAtn = gpio_get_dir(PIN_ATN_MSG);
    int dirRst = gpio_get_dir(PIN_RST_CD_REQ_SCL);

    // get the current output level of PIN_IN_OE
    int lvlInOe = gpio_get_out_level(PIN_IN_OE);

    // get gpio function, set to SIO
    gpio_function_t funcRst = gpio_get_function(PIN_RST_CD_REQ_SCL);
    gpio_set_function(PIN_RST_CD_REQ_SCL, GPIO_FUNC_SIO);

    // set as inputs
    gpio_set_dir(PIN_ATN_MSG, 0);
    gpio_set_dir(PIN_RST_CD_REQ_SCL, 0);

    // enable input chip
    gpio_put(PIN_IN_OE, 1);

    // read pins, set bits in result
    int res = 0;

    if(BIT_IS_L(PIN_ATN_MSG)) {
        res |= BIT_ATN;
    }

    if(BIT_IS_L(PIN_RST_CD_REQ_SCL)) {
        res |= BIT_RESET;
    }

    // restore gpio function
    gpio_set_function(PIN_RST_CD_REQ_SCL, funcRst);

    // restore directions
    gpio_set_dir(PIN_ATN_MSG, dirAtn);
    gpio_set_dir(PIN_RST_CD_REQ_SCL, dirRst);

    // restore previous output level of PIN_IN_OE
    gpio_put(PIN_IN_OE, lvlInOe);

    return res;
}

void setScsiPhase(int newPhase, bool force)
{
    static int currentPhase = MODE_UNKNOWN;

    if(!force && newPhase == currentPhase) {    // no mode change? just quit
        return;
    }
    currentPhase = newPhase;

    // I/O, C/D, MSG must be controlled by SIO for setting the phase
    gpio_set_function(PIN_SEL_IO_DP_SDA, GPIO_FUNC_SIO);
    gpio_set_function(PIN_RST_CD_REQ_SCL, GPIO_FUNC_SIO);
    gpio_set_function(PIN_ATN_MSG, GPIO_FUNC_SIO);

    uint32_t bits = 0;
    switch(newPhase)
    {
        case MODE_RESET:
        case MODE_SCSI_SELECTION:   bits = (1 << PIN_SEL_IO_DP_SDA) | (1 << PIN_RST_CD_REQ_SCL) | (1 << PIN_ATN_MSG); break;
        case MODE_CMD:              bits = (1 << PIN_SEL_IO_DP_SDA) |                             (1 << PIN_ATN_MSG); break;
        case MODE_MSG_OUT:          bits = (1 << PIN_SEL_IO_DP_SDA)                                                 ; break;
        case MODE_DMA_READ:         bits =                            (1 << PIN_RST_CD_REQ_SCL) | (1 << PIN_ATN_MSG); break;
        case MODE_DMA_WRITE:        bits = (1 << PIN_SEL_IO_DP_SDA) | (1 << PIN_RST_CD_REQ_SCL) | (1 << PIN_ATN_MSG); break;
        case MODE_STATUS:           bits =                                                        (1 << PIN_ATN_MSG); break;
        case MODE_MSG_IN:           bits =                                                                         0; break;
    }

    gpio_set_dir_out_masked((1 << PIN_SEL_IO_DP_SDA) | (1 << PIN_RST_CD_REQ_SCL) | (1 << PIN_ATN_MSG));     // I/O, C/D, MSG as outputs
    gpio_put_masked((1 << PIN_SEL_IO_DP_SDA) | (1 << PIN_RST_CD_REQ_SCL) | (1 << PIN_ATN_MSG), bits);       // set the bits L or H
    gpio_put(PIN_OUT_LE1, 1);       // store I/O, C/D, MSG from D to Q
    busy_wait_at_least_cycles(10);
    gpio_put(PIN_OUT_LE1, 0);       // latch enable, that means hold the signals

    bool driveControls = (newPhase != MODE_RESET) && (newPhase != MODE_SCSI_SELECTION);
    gpio_put(PIN_OUT_OE, driveControls ? 0 : 1);    // when we're not in selection or reset phase, drive the output control pins (device is responsing, BSY is L)
    gpio_put(PIN_IN_OE, driveControls ? 1 : 0);     // when we're in selection or reset phase, set IN_OE to L, so we can read the SEL, RST, BSY signals

    gpio_set_dir_out_masked((1 << PIN_SEL_IO_DP_SDA) | (1 << PIN_RST_CD_REQ_SCL));  // DP and REQ as outputs
    gpio_put_masked((1 << PIN_SEL_IO_DP_SDA) | (1 << PIN_RST_CD_REQ_SCL), (1 << PIN_SEL_IO_DP_SDA) | (1 << PIN_RST_CD_REQ_SCL));    // DP and REQ to H
    gpio_put(PIN_OUT_LE2, 1);       // store DP and REQ from D to Q
    busy_wait_at_least_cycles(10);
    gpio_put(PIN_OUT_LE2, 0);       // latch enable, that means hold the signals

    if(newPhase == MODE_RESET || newPhase == MODE_SCSI_SELECTION) {     // RESET / SELECTION?
        gpio_set_dir_in_masked((1 << PIN_SEL_IO_DP_SDA) | (1 << PIN_RST_CD_REQ_SCL) | (1 << PIN_BSY));  // SEL, RST, BSY are inputs
    } else {        // other phases
        gpio_set_dir_in_masked((1 << PIN_ACK));                                         // ACK as input
        gpio_set_dir_out_masked((1 << PIN_SEL_IO_DP_SDA) | (1 << PIN_RST_CD_REQ_SCL));  // DP and REQ as outputs
    }
}

void setDataDirection(uint8_t sendNotRecv, PIO pio, uint sm)
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

    if(pio != NULL) {
        int dirs = (sendNotRecv == DIR_SEND) ? DATA_PINS_MASK : 0;
        pio_sm_set_pindirs_with_mask64(pio, sm, dirs, DATA_PINS_MASK);
    }

    int pio_data_pins[10] = {PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7, PIN_SEL_IO_DP_SDA, PIN_RST_CD_REQ_SCL};
    for (int i = 0; i < 10; i++) {
        if(pio != NULL) {           // got pio, pin handled by PIO
            pio_gpio_init(pio, pio_data_pins[i]);
        } else {                    // no pio, pin handled by SIO
            gpio_set_function(pio_data_pins[i], GPIO_FUNC_SIO);
        }
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

    prefillBytesWithParityTable();
}

int pioConfig(int newMode, bool force)
{
    static int currentMode = MODE_UNKNOWN;
    int oldMode = currentMode;

    if(!force && newMode == currentMode) {    // no mode change? just quit
        return oldMode;
    }
    currentMode = newMode;

    // all state machines to halt
    pio_sm_set_enabled(pioScsiWrite, smScsiWrite, false);
    pio_sm_set_enabled(pioScsiRead, smScsiRead, false);

    setScsiPhase(newMode, force);

    PIO whichPio = NULL;
    uint whichSm = 0;

    switch(newMode)
    {
        // pasive modes (just watching bus)
        case MODE_RESET:
        case MODE_SCSI_SELECTION:
            // SEL, RST, ATN, BSY are controlled by SIO and are inputs
            gpio_set_function(PIN_SEL_IO_DP_SDA, GPIO_FUNC_SIO);
            gpio_set_function(PIN_RST_CD_REQ_SCL, GPIO_FUNC_SIO);
            gpio_set_function(PIN_ATN_MSG, GPIO_FUNC_SIO);
            gpio_set_function(PIN_BSY, GPIO_FUNC_SIO);
            break;

        // write modes
        case MODE_CMD:
        case MODE_DMA_WRITE:
        case MODE_MSG_OUT:
            whichPio = pioScsiWrite;
            whichSm = smScsiWrite;
            break;

        // read modes
        case MODE_DMA_READ:
        case MODE_STATUS:
        case MODE_MSG_IN:
            readInProgressCount = 0;        // no read bytes in progress

            whichPio = pioScsiRead;
            whichSm = smScsiRead;
            break;
    }

    // data direction RECV for CMD and WRITE, data direction SEND for READ and STATUS
    uint8_t sendNotRecv = (newMode == MODE_DMA_READ || newMode == MODE_STATUS || newMode == MODE_MSG_IN) ? DIR_SEND : DIR_RECV;
    setDataDirection(sendNotRecv, whichPio, whichSm);

    // if we're in the reset mode, don't enable any PIO SM
    if(newMode == MODE_RESET || newMode == MODE_SCSI_SELECTION) {
        return oldMode;
    }

    // restart state machine, clear FIFOs, enable state machine
    pio_sm_restart(whichPio, whichSm);
    pio_sm_clear_fifos(whichPio, whichSm);
    pio_sm_set_enabled(whichPio, whichSm, true);

    return oldMode;
}

uint8_t isSelectionHappening(void)
{
    pioConfig(MODE_SCSI_SELECTION);

    if(BIT_IS_L(PIN_RST_CD_REQ_SCL)) {      // no new cmd bytes when RESET is L
        return false;
    }

    return BIT_IS_L(PIN_SEL_IO_DP_SDA);     // the SELECTION is happening, when SEL is L
}

// get selection byte
uint8_t getSelectionByte(void)
{
    uint8_t val;

    timeoutStart();             // start the timeout timer

    brStat = E_OK;              // init bridge status to E_OK
    return ((uint8_t) ~(gpio_get_all()));
}

// get next CMD byte from ST -- with setting INT to LOW and waiting for CS
uint8_t PIO_write(void)
{
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

    uint8_t val = ((uint8_t) (pio_sm_get(pioScsiWrite, smScsiWrite) >> 24));
    val = ~val;     // invert data
    return val;
}

// send status byte to host, and on SCSI also to MSG IN byte
void statusAndMsgRead(uint8_t scsiStatusByte)
{
    // if we didn't have bridge timeout, we can try to send STATUS byte
    if (brStat != E_TimeOut)
    {
        pioConfig(MODE_STATUS);
        PIO_read(scsiStatusByte);
    }

    // if we didn't have bridge timeout, we can try to send MSG IN
    if (brStat != E_TimeOut)
    {
        pioConfig(MODE_MSG_IN);
        PIO_read(0);
    }

    if (brStat != E_OK)
    { // if some timeout occured, then failed

    }

    resetBridge();               // put BSY, C/D, I/O in released states
}

void PIO_read(uint8_t val)
{
    uint16_t valPar = byteWithParity[val];
    pio_sm_put(pioScsiRead, smScsiRead, valPar);     // write status byte to TX FIFO, the transfer will start

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
    uint16_t valPar = byteWithParity[val];

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
           pio_sm_put(pioScsiRead, smScsiRead, valPar);
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

    uint8_t val = ((uint8_t) (pio_sm_get(pioScsiWrite, smScsiWrite) >> 24));
    val = ~val;
    return val;
}

void resetBridge(void)
{
    pioConfig(MODE_SCSI_SELECTION, true);
    brStat = E_OK; // set bridge status to OK
}

