#include "defs.h"
#include "bridge.h"
#include "utils.h"

#include "scsi_write.pio.h"
#include "scsi_read.pio.h"

extern uint8_t brStat; // status from bridge

static PIO pioScsiWrite, pioScsiRead;
static uint smScsiWrite, smScsiRead;

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

bool isAtnAsserted(void)
{
    int lvlOutLe2 = gpio_get_out_level(PIN_OUT_LE2);    // get the current output level of PIN_OUT_LE2
    gpio_put(PIN_OUT_LE2, 0);                           // put LE2 to L, so changing shared phase / handshake pins doesn't put out false req out

    int dirAtn = gpio_get_dir(PIN_ATN_MSG);             // get pin direction
    gpio_set_dir(PIN_ATN_MSG, GPIO_IN);                 // set as input (so it won't drive the pin to some last level, but read actual ATN instead)

    int lvlInOe = gpio_get_out_level(PIN_IN_OE);        // get the current output level of PIN_IN_OE
    gpio_put(PIN_IN_OE, 0);                             // enable input chip

    bool res = BIT_IS_L(PIN_ATN_MSG);                   // see if ATN is L

    gpio_put(PIN_IN_OE, lvlInOe);                       // restore previous output level of PIN_IN_OE
    gpio_set_dir(PIN_ATN_MSG, dirAtn);                  // restore directions
    gpio_put(PIN_OUT_LE2, lvlOutLe2);                   // restore previous output level of PIN_OUT_LE2

    return res;
}

// for reset / selection phases
void setScsiPhaseForSelection(void)
{
    // don't drive control signals
    gpio_put(PIN_OUT_OE, 1);

    // SEL, RST, ATN as inputs
    gpio_set_dir_in_masked((1 << PIN_SEL_IO_DP_DS) | (1 << PIN_RST_CD_REQ_CP) | (1 << PIN_ATN_MSG));

    // SEL, RST, ATN controlled by SIO
    gpio_set_function(PIN_SEL_IO_DP_DS, GPIO_FUNC_SIO);
    gpio_set_function(PIN_RST_CD_REQ_CP, GPIO_FUNC_SIO);
    gpio_set_function(PIN_ATN_MSG, GPIO_FUNC_SIO);

    // now we can enable input chip
    gpio_put(PIN_IN_OE, 0);
}

// for most of the phases (except reset / selection)
void setScsiPhaseForTransfer(int newPhase)
{
    gpio_put(PIN_IN_OE, 1);     // disable input chip
    gpio_put(PIN_OUT_LE2, 0);   // put LE2 to L, so changing shared phase / handshake pins doesn't put out false req out

    // I/O, C/D, MSG controlled by SIO to set these phase controls
    gpio_set_function(PIN_SEL_IO_DP_DS, GPIO_FUNC_SIO);
    gpio_set_function(PIN_RST_CD_REQ_CP, GPIO_FUNC_SIO);
    gpio_set_function(PIN_ATN_MSG, GPIO_FUNC_SIO);

    // I/O, C/D, MSG as outputs
    gpio_set_dir_out_masked((1 << PIN_SEL_IO_DP_DS) | (1 << PIN_RST_CD_REQ_CP) | (1 << PIN_ATN_MSG));

    // put DP and REQ to H, capture them in L using latch (by LE2 going to L afterwards)
    gpio_put_masked((1 << PIN_SEL_IO_DP_DS) | (1 << PIN_RST_CD_REQ_CP), (1 << PIN_SEL_IO_DP_DS) | (1 << PIN_RST_CD_REQ_CP));    // DP and REQ to H
    gpio_put(PIN_OUT_LE2, 1);       // store DP and REQ from D to Q
    busy_wait_at_least_cycles(10);
    gpio_put(PIN_OUT_LE2, 0);       // latch enable, that means hold the signals

    uint32_t bits = 0;
    switch(newPhase)
    {
        // case MODE_RESET:     // same as SELECTION
        case MODE_SCSI_SELECTION:   bits = (1 << PIN_SEL_IO_DP_DS) | (1 << PIN_RST_CD_REQ_CP) | (1 << PIN_ATN_MSG); LED_OFF;  break;
        case MODE_CMD:              bits = (1 << PIN_SEL_IO_DP_DS) |                             (1 << PIN_ATN_MSG); LED_ON;   break;
        case MODE_MSG_OUT:          bits = (1 << PIN_SEL_IO_DP_DS)                                                 ; LED_ON;   break;
        case MODE_DMA_READ:         bits =                            (1 << PIN_RST_CD_REQ_CP) | (1 << PIN_ATN_MSG);           break;
        case MODE_DMA_WRITE:        bits = (1 << PIN_SEL_IO_DP_DS) | (1 << PIN_RST_CD_REQ_CP) | (1 << PIN_ATN_MSG);           break;
        case MODE_STATUS:           bits =                                                        (1 << PIN_ATN_MSG);           break;
        case MODE_MSG_IN:           bits =                                                                         0;           break;
    }

    gpio_put_masked((1 << PIN_SEL_IO_DP_DS) | (1 << PIN_RST_CD_REQ_CP) | (1 << PIN_ATN_MSG), bits);       // set the bits L or H
    gpio_put(PIN_OUT_LE1, 1);       // store I/O, C/D, MSG from D to Q
    busy_wait_at_least_cycles(10);
    gpio_put(PIN_OUT_LE1, 0);       // latch enable, that means hold the signals

    // put DP and REQ to H after using them for phase setting, but no need to latch them (no LE2 cycle)
    gpio_put_masked((1 << PIN_SEL_IO_DP_DS) | (1 << PIN_RST_CD_REQ_CP), (1 << PIN_SEL_IO_DP_DS) | (1 << PIN_RST_CD_REQ_CP));    // DP and REQ to H

    gpio_put(PIN_OUT_OE, 0);        // enable output chip
}

void setScsiPhase(int newPhase, bool force)
{
    static int currentPhase = MODE_UNKNOWN;

    if(!force && newPhase == currentPhase) {    // no mode change? just quit
        return;
    }
    currentPhase = newPhase;

    bool driveControls = (newPhase != MODE_RESET) && (newPhase != MODE_SCSI_SELECTION);

    // for reset / selection, turn off outputs, change pins to inputs, enable input chip and quit
    if(!driveControls) {
        setScsiPhaseForSelection();
    } else {
        setScsiPhaseForTransfer(newPhase);
    }
}

void setDataDirection(uint8_t sendNotRecv, PIO pio, uint sm)
{
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
        int dirs = (sendNotRecv == DIR_SEND) ? (DATA_PINS_MASK | (1 << PIN_RST_CD_REQ_CP)) : (1 << PIN_RST_CD_REQ_CP);
        pio_sm_set_pins_with_mask(pio, sm, (1 << PIN_RST_CD_REQ_CP), (1 << PIN_RST_CD_REQ_CP));
        pio_sm_set_pindirs_with_mask(pio, sm, dirs, DATA_PINS_MASK | (1 << PIN_RST_CD_REQ_CP));
    }

    int pio_data_pins[10] = {PIN_D0, PIN_D1, PIN_D2, PIN_D3, PIN_D4, PIN_D5, PIN_D6, PIN_D7, PIN_SEL_IO_DP_DS, PIN_RST_CD_REQ_CP};
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

    success = pio_claim_free_sm_and_add_program_for_gpio_range(&scsi_write_program, &pioScsiWrite, &smScsiWrite, &offset, PIN_D0, 11, true);
    if(!success) { debug("Failed to claim PIO SM 1\n"); while(1); }
    scsi_write_program_init(pioScsiWrite, smScsiWrite, offset);

    success = pio_claim_free_sm_and_add_program_for_gpio_range(&scsi_read_program, &pioScsiRead, &smScsiRead, &offset, PIN_D0, 11, true);
    if(!success) { debug("Failed to claim PIO SM 2\n"); while(1); }
    scsi_read_program_init(pioScsiRead, smScsiRead, offset);

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
        // case MODE_RESET:         // same as SELECTION
        case MODE_SCSI_SELECTION:
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
            // readInProgressCount = 0;        // no read bytes in progress

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

    // for other modes we need to put LE2 to H, so that the DP + REQ + ACK signals are transparently passed out of device without latch
    gpio_put(PIN_OUT_LE2, 1);

    // restart state machine, clear FIFOs, enable state machine
    pio_sm_restart(whichPio, whichSm);
    pio_sm_clear_fifos(whichPio, whichSm);
    pio_sm_set_enabled(whichPio, whichSm, true);

    return oldMode;
}

uint8_t isSelectionHappening(void)
{
    pioConfig(MODE_SCSI_SELECTION);

    if(BIT_IS_L(PIN_RST_CD_REQ_CP)) {      // no new cmd bytes when RESET is L
        return false;
    }

    return BIT_IS_L(PIN_SEL_IO_DP_DS);     // the SELECTION is happening, when SEL is L
}

// get selection byte
uint8_t getSelectionByte(void)
{
    uint8_t val;

    timeoutStart();             // start the timeout timer
    brStat = E_OK;              // init bridge status to E_OK

    while(BIT_IS_L(PIN_SEL_IO_DP_DS) && !hasTimedOut)          // while SEL pin is asserted and it's not timeout yet
    {
        uint8_t selectedIds = ((uint8_t) ~(gpio_get_all()));    // get all data bits

        for (int i = 0; i < 8; i++)
        {
            int bit = (1 << i);

            if ((selectedIds & bit) && (settings.enabledIDs & bit))    // if the bit #i set and is this ID enabled?
            {
                return i;   // return this ID
            }
        }
    }

    return 0xff;            // this device was not selected
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

        if(brStat == E_TimeOut) {
            debug("TO on STATUS\n");
        }
    }

    // if we didn't have bridge timeout, we can try to send MSG IN
    if (brStat != E_TimeOut)
    {
        pioConfig(MODE_MSG_IN);
        PIO_read(0);

        if(brStat == E_TimeOut) {
            debug("TO on MSG_IN\n");
        }
    }

    resetBridge();               // put BSY, C/D, I/O in released states
}

void PIO_read(uint8_t val)
{
    uint32_t valPar = byteWithParity[val];
    pio_sm_put_blocking(pioScsiRead, smScsiRead, valPar);     // write status byte to TX FIFO, the transfer will start

    // wait for data to be transfered
    while(1)
    {
        if(hasTimedOut) {       // on timeout
            brStat = E_TimeOut; // set the bridge status
            return;
        }

        // nothing in TX fifo? pins idle? we're done
        if(pio_sm_is_tx_fifo_empty(pioScsiRead, smScsiRead)) {
            uint32_t allPins = gpio_get_all();

            if((allPins & HANDSHAKE_PINS_MASK) == HANDSHAKE_PINS_MASK) {    // both handshake pins are H?
                break;
            }
        }
    }

    brStat = E_OK;
}

void DMA_read_waitForEnd(void)
{
    // wait while read is still in progress (from FIFO to Atari)
    while(1) {
        // nothing in TX fifo and handshake pins are idle? we're done
        if(pio_sm_is_tx_fifo_empty(pioScsiRead, smScsiRead)) {
            uint32_t allPins = gpio_get_all();

            if((allPins & HANDSHAKE_PINS_MASK) == HANDSHAKE_PINS_MASK) {    // both handshake pins are H?
                return;
            }
        }

        if(hasTimedOut) {       // on timeout
            debug("DMA_read_waitForEnd T/O\n");
            brStat = E_TimeOut; // set the bridge status
            return;
        }
    }
}

void DMA_read(uint8_t val)
{
    uint32_t valPar = byteWithParity[val];

    // wait for TX fifo not full, so we can put the current value in
    while(1) {
        // READ TX FIFO not full, we can push to fifo
        if(!pio_sm_is_tx_fifo_full(pioScsiRead, smScsiRead)) {
           pio_sm_put(pioScsiRead, smScsiRead, valPar);
           return;
        }

        if(hasTimedOut) {       // on timeout
            debug("DMA_read T/O\n");
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

