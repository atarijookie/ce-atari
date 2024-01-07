#include "stm32f10x.h"                       // STM32F103 definitions
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_usart.h"
#include "misc.h"

#include "defs.h"
#include "timers.h"
#include "main.h"
#include "floppyhelpers.h"
#include "init.h"
#include "global_vars.h"
#include "ikbd.h"

/* Franz to host communication:
A) send   : ATN_SEND_TRACK with the track # and side # -- 2 WORDs + zeros = 3 WORDs
   receive: track data with sector start marks, up to 15 kB -- 12 sectors + the marks

B) send   : ATN_SECTOR_WRITTEN with the track, side, sector # + captured data, up to 1500 B
   receive: nothing (or don't care)

C) send   : ATN_FW_VERSION with the FW version + empty bytes == 3 WORD for FW + empty WORDs
   receive: commands possibly received -- receive 6 WORDs (3 empty, 3 with possible commands)
*/

//--------------
// circular buffer
// watch out, these macros take 0.73 us for _add, and 0.83 us for _get operation!

//#define     wrBuffer_add(X)                 { if(wrNow->count  < WRITEBUFFER_SIZE) { wrNow->buffer[wrNow->count] = X; wrNow->count++; } }

#define     readTrackData_goToStart()       { inIndexGet = STREAM_START_OFFSET; }
#define     readTrackData_get(X)            { X = readTrackData[inIndexGet];                inIndexGet++;       if(inIndexGet >= READTRACKDATA_SIZE_BYTES) {  inIndexGet = STREAM_START_OFFSET; }; }
#define     readTrackData_get_noMove(X)     { X = readTrackData[inIndexGet];                                                                                                                                    }
#define     readTrackData_justMove()        {                                                                                       inIndexGet++;       if(inIndexGet >= READTRACKDATA_SIZE_BYTES) { inIndexGet = STREAM_START_OFFSET; };   }
//--------------

void setupDriveSelect(void);
void setupDiskChangeWriteProtect(void);

void fillReadStreamBufferWithDummyData(void);
void fillMfmTimesWithDummy(void);

void updateStreamPositionByFloppyPosition(void);

void handleFloppyWrite(void);

BYTE franzMode = FRANZ_MODE_V1_V2;      // starting in the v1/v2 mode
BYTE soundOn   = FALSE;                 // not using sound by default

BYTE buttonPressed = FALSE;
BYTE buttonPressedPrev = FALSE;
WORD buttonTimeDown = 0;

BYTE sendShutdownRequest = FALSE;
WORD shutdownStart = 0;

void trackRequest(BYTE forceRequest) 
{
    next.track = now.track;
    next.side = now.side;
    trackStreamedCount = 0; 

    if(forceRequest) {  // force request - do it immediatelly, don't cancel even if we asked for the same track last time (e.g. we want this after written sectors)
        lastRequestTime -= 35;
        lastRequested.track = 0xff;
        lastRequested.side = 0xff;
    } else {            // normal request - add small delay before really talking to host, as there might be another request incomming
        lastRequestTime = TIM4->CNT;

        // if what we're asking next is what we were asking last time, cancel track request - we have it already
        if(next.track == lastRequested.track && next.side == lastRequested.side) {
            sendTrackRequest = FALSE;
            outFlags.weAreReceivingTrack = FALSE;
            return;
        }
    }

    sendTrackRequest = TRUE;

    fillReadStreamBufferWithDummyData();        // fill MFM stream with dummy data so we won't stream any junk
    outFlags.weAreReceivingTrack = TRUE;        // mark that we are receiving TRACK data, and thus shouldn't stream
}

void beeperOff(void)
{
    if(franzMode == FRANZ_MODE_V4) {            // touch pin only in Franz mode v4
        GPIOB->BRR = BEEPER;
    }
}

void beeperToggle(BYTE isFloppySound)
{
    WORD beeperNow;

    if(franzMode != FRANZ_MODE_V4) {            // touch pin only in Franz mode v4
        return;
    }

    beeperNow = GPIOB->ODR & BEEPER;

    if(beeperNow) {     // beeper ON? always allow turning OFF
        GPIOB->BRR = BEEPER;
    } else {            // beeper OFF? turn on only if it's not from floppy or if it's from floppy but floppy seek sound enabled
        if(!isFloppySound || (isFloppySound && soundOn)) {
            GPIOB->BSRR = BEEPER;
        }
    }
}

void beeperOn(BYTE isFloppySound)
{
    if(franzMode != FRANZ_MODE_V4) {            // touch pin only in Franz mode v4
        return;
    }

    //  turn on only if it's not from floppy or if it's from floppy but floppy seek sound enabled
    if(!isFloppySound || (isFloppySound && soundOn)) {
        GPIOB->BSRR = BEEPER;
    }
}

void handleButton(void)
{
    static WORD lastHandleTime = 0;
    WORD timeSinceShutdownRequest;
    BYTE isInShutdownTimeInterval = 0;
    WORD timeNow = TIM4->CNT;
    WORD diff;
    WORD pressDuration;

    // handle button only every 100 ms (don't handle too often)
    diff = timeNow - lastHandleTime;
    if(diff < 200) {            // less than 100 ms passed? just quit
        return;
    }
    lastHandleTime = timeNow;   // we're handling it now

    pressDuration = timeNow - buttonTimeDown;  // how long the button is pressed
    pressDuration = pressDuration >> 1;             // 1 tick is 0.5 ms, convert them to ms by dividing by 2
    isInShutdownTimeInterval = (pressDuration >= PWR_OFF_PRESS_TIME_MIN) && (pressDuration < PWR_OFF_PRESS_TIME_MAX);  // if press length is in the 'power off' interval

    buttonPressed = (GPIOA->IDR & BTN) == BTN;      // get button bit, pressed if H

    // button press changed == press event or release event
    if(buttonPressedPrev != buttonPressed) {
        buttonPressedPrev = buttonPressed;

        if(buttonPressed) {                     // on press - store time and send report
            buttonTimeDown = timeNow;
            sendFwVersion = TRUE;
        } else {                                // on button released
            if(isInShutdownTimeInterval) {      // if in shutdown time interval, should send shutdown request
                sendShutdownRequest = TRUE;
                shutdownStart = timeNow;
            }
            sendFwVersion = TRUE;
            beeperOff();
        }
    }

    // button pressed - handle long press
    if(buttonPressed && isInShutdownTimeInterval) { // pressed and in shutdown interval?
        beeperToggle(0);
    }

    // we've requested the shutdown, now if the host doesn't shut this down, then we will after some time
    if(sendShutdownRequest) {
        timeSinceShutdownRequest = timeNow - shutdownStart;         // ticks since shutdown start
        timeSinceShutdownRequest = timeSinceShutdownRequest / 2000; // seconds since shutdown start

        if(timeSinceShutdownRequest >= PWR_OFF_AFTER_REQUEST) {     // enough time passed since shutdown request?
            GPIOB->CRL &= ~(0x0000000f);                            // remove bits from GPIOB for GPIOB0 (DEVICE_OFF_H)
            GPIOB->CRL |=   0x00000003;                             // set DEVICE_OFF_H (GPIO0) in push-pull output
            GPIOB->BSRR = DEVICE_OFF_H;                             // set DEVICE_OFF_H to H - to turn off device now
        }
    }
}

int main(void)
{
    BYTE indexCount = 0;
    WORD WGate;
    spiDmaIsIdle = TRUE;
    readTrackData = (BYTE *) readTrackDataBfr;

    prevIntTime = 0;
    sectorsWritten = 0;         // nothing written yet

    sendFwVersion       = FALSE;
    sendTrackRequest    = FALSE;

    SystemCoreClockUpdate();        // get and calculate SystemCoreClock based on current configuration - we're running at 72 MHz or 64 MHz

    setupAtnBuffers();
    init_hw_sw();                                   // init GPIO pins, timers, DMA, global variables

    circularInit(&buff0);
    circularInit(&buff1);

    setupDriveSelect();                             // the drive select bits which should be LOW if the drive is selected

    // init floppy signals
    GPIOB->BSRR = (WR_PROTECT | DISK_CHANGE);       // not write protected
    GPIOB->BRR = TRACK0 | DISK_CHANGE;              // TRACK 0 signal to L, DISK_CHANGE to LOW
    GPIOB->BRR = ATN;                               // ATTENTION bit low - nothing to read

    setupDiskChangeWriteProtect();                  // init write-protect and disk-change pins

    trackRequest(TRUE);                             // request track 0, side 0

    while(1) {
        WORD inputs;

        if(franzMode == FRANZ_MODE_V4) {                // if we're in Franz mode v4, handle the button
            handleButton();
        }

        if(outFlags.updatePosition) {
            outFlags.updatePosition = FALSE;
            updateStreamPositionByFloppyPosition();     // place the read marker on the right place in the stream
        }

        // ST wants the stream and we are not receiving TRACK data? ENABLE stream
        if(outFlags.stWantsTheStream && !outFlags.weAreReceivingTrack) {
            if(!outFlags.outputsAreEnabled) {           // the outputs are not enabled yet?
                FloppyOut_Enable();                     // enable them
                outFlags.outputsAreEnabled = TRUE;      // mark that we enabled them
            }
        } else {    // other cases? DISABLE stream
            if(outFlags.outputsAreEnabled) {            // the outputs are enabled?
                FloppyOut_Disable();                    // disable them
                outFlags.outputsAreEnabled = FALSE;     // mark that we disabled them
            }
        }

        // handle not finished SPI transfers
        if(!spiDmaIsIdle && timeout()) {            // SPI is still not idle, but timeout already occured? (RPi didn't finish the transfer)
            GPIOB->BRR = ATN;                       // ATTENTION bit low  - nothing to read

            // disable both TX and RX channels
            DMA1_Channel3->CCR &= 0xfffffffe;
            DMA1_Channel2->CCR &= 0xfffffffe;

            SPI_Cmd(SPI1, DISABLE);         // disable SPI1

            // reset SPI1 peripheral - the way like it's done in USART_DeInit() using 2x RCC_APB2PeriphResetCmd()
            RCC->APB2RSTR |= RCC_APB2RSTR_SPI1RST;  // enable
            RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI1RST; // disable

            spi_init();                     // init SPI interface

            if(outFlags.weAreReceivingTrack) {                          // if we were receiving a track
                outFlags.weAreReceivingTrack    = FALSE;                // mark that we're not receiving it anymore
                outFlags.updatePosition         = TRUE;
            }

            spiDmaIsIdle = TRUE;            // SPI DMA is now idle
        }

        // sending and receiving data over SPI using DMA
        if(spiDmaIsIdle) {                                                                      // SPI DMA: nothing to Tx and nothing to Rx?
            if(sendFwVersion) {                                                                 // should send FW version? this is a window for receiving commands
                WORD ifaceReport;
                WORD buttonReport;

                if(isDiskChanged) {                                                             // if it was held for a while, take it down
                    isDiskChanged = FALSE;
                    setupDiskChangeWriteProtect();
                }

                ifaceReport = ((GPIOB->IDR & IFACE_DETECT) == IFACE_DETECT) ? IFACE_SCSI : IFACE_ACSI;  // bit H means SCSI, bit L means ACSI

                if(sendShutdownRequest) {       // if shutdown should be requested, send it
                    buttonReport = BTN_SHUTDOWN;
                } else {                        // not shutdown? send button state
                    buttonReport = buttonPressed ? BTN_PRESSED : BTN_RELEASED;
                }
                atnSendFwVersion[6] = MAKEWORD(ifaceReport, buttonReport);

                spiDma_txRx(ATN_SENDFWVERSION_LEN_TX, atnSendFwVersion, ATN_SENDFWVERSION_LEN_RX, cmdBuffer);
                sendFwVersion = FALSE;
            } else if(sendTrackRequest) {                                                       // if should send track request
                // check how much time passed since the request was created
                WORD timeNow    = TIM4->CNT;
                WORD diff       = timeNow - lastRequestTime;

                if(diff >= 30) {                                                                // and at least 15 ms passed since the request (30 / 2000 s)
                    beeperOff();

                    sendTrackRequest = FALSE;

                    // first check if this isn't what we've requested last time
                    lastRequested.track = next.track;                                       // mark what we've requested last time
                    lastRequested.side  = next.side;

                    atnSendTrackRequest[4] = MAKEWORD(next.side, next.track);
                    spiDma_txRx(ATN_SENDTRACK_REQ_LEN_TX, atnSendTrackRequest, ATN_SENDTRACK_REQ_LEN_RX, readTrackDataBfr);

                    outFlags.weAreReceivingTrack = TRUE;                                    // mark that we started to receive TRACK data
                }
            } else if(wrNow->readyToSend) {                                                     // not sending any ATN right now? and current write buffer has something?
                spiDma_txRx(wrNow->count, wrNow->buffer, 1, &fakeBuffer);

                wrNow->readyToSend  = FALSE;                                                    // mark the current buffer as not ready to send (so we won't send this one again)

                wrNow               = wrNow->next;                                              // and now we will select the next buffer as current
                wrNow->readyToSend  = FALSE;                                                    // the next buffer is not ready to send (yet)
                wrNow->count        = 4;                                                        // at the start we already have 4 WORDs in buffer - SYNC, ATN code, TX len, RX len
            }
        }

        //-------------------------------------------------
        // if we got something in the cmd buffer, we should process it
        if(spiDmaIsIdle == TRUE && cmdBuffer[0] != CMD_MARK_READ_BYTE) {                        // if we're not sending (receiving) and the cmd buffer is not read
            int i;

            for(i=0; i<CMD_BUFFER_SIZE; i++) {
                BYTE cmd1, cmd2;
                cmd1 = cmdBuffer[i] >> 8;       // upper byte
                cmd2 = cmdBuffer[i] & 0xff;     // lower byte

                processHostCommand(cmd1);
                processHostCommand(cmd2);
            }

            cmdBuffer[0] = CMD_MARK_READ_BYTE;                                                  // mark that this cmd buffer is already read
        }

        //-------------------------------------------------
        inputs = GPIOB->IDR;                                        // read floppy inputs

        // now check if the drive is ON or OFF and handle it
        outFlags.stWantsTheStream = ((inputs & drive_select) == 0); // if motor is enabled and drive is selected, ST wants the stream

        //-------------------------------------------------

        WGate = inputs & WGATE;                                         // get current WGATE value

        if(WGate == 0) {                                                // when write gate is low, the data is written to floppy
            handleFloppyWrite();
            sectorsWritten++;                                       // one sector was written, request updated track at the end of stream
        }

        // fillMfmTimesForDMA -- execution time: 7 us - 16 us (16 us rarely, at the start / end)
        // times between two calls: 16 us - 53 us (16 us rarely, probably start / end of track)
        if((DMA1->ISR & (DMA1_IT_TC5 | DMA1_IT_HT5)) != 0) {        // MFM read stream: TC or HT interrupt? we've streamed half of circular buffer!
            fillMfmTimesForDMA();                                   // fill the circular DMA buffer with mfm times
        }

        //------------
        // check INDEX pulse as needed
        if((TIM2->SR & 0x0001) != 0) {          // overflow of TIM1 occured?
            TIM2->SR = 0xfffe;                  // clear UIF flag

            readTrackData_goToStart();          // move the pointer in the track stream to start

            //-----------
            if(sectorsWritten > 0) {        // if some sectors were written to floppy, we need to get the new stream now
                sectorsWritten = 0;         // nothing written now
                trackRequest(TRUE);         // ask for the changed track data, but force it - get it immediatelly
            }

            //-----------
            // the following section of code should request track again if even after 2 rotations of floppy we're not streaming what we should
            trackStreamedCount++;               // increment the count of how many times we've streamed this track

            if(trackStreamedCount >= 2) {       // if since the last request 2 rotations happened
                if(streamed.track != now.track || streamed.side != now.side) {  // and we're not streaming what we really want to stream
                    trackRequest(FALSE);        // ask for track data (again?)
                }
            }
            streamed.track  = (BYTE) -1;        // after the end of track mark that we're not streaming anything
            streamed.side   = (BYTE) -1;
            //-----------

            fillReadStreamBufferWithDummyData();

            // the following few lines send the FW version to host every 5 index pulses, this is used for transfer of commands from host to Franz
            indexCount++;

            if(indexCount == 5) {
                indexCount = 0;
                sendFwVersion = TRUE;
            }
        }

        //--------
        // NOTE! Handling of STEP and SIDE only when MOTOR is ON, but the drive doesn't have to be selected and it must handle the control anyway
        if((inputs & MOTOR_ENABLE) != 0) {      // motor not enabled? Skip the following code.
            continue;
        }

        //------------
        // update SIDE var
        now.side = (inputs & SIDE1) ? 0 : 1;    // get the current SIDE
        if(prev.side != now.side) {             // side changed?
            trackRequest(FALSE);                // we need track from the right side
            prev.side = now.side;
        }
    }
}

void fillReadStreamBufferWithDummyData(void)
{
    int i=0;

    for(i=0; i<16; i++) {               // copy 'all 4 us' pulses into current streaming buffer to allow shortest possible switch to start of track
        mfmReadStreamBuffer[i] = 7;
    }
}

void spiDma_txRx(WORD txCount, WORD *txBfr, WORD rxCount, WORD *rxBfr)
{
    // store TX and RX count so the host will know how much he should transfer
    txBfr[2] = txCount;
    txBfr[3] = rxCount;

    hostIsUp = !timeout();              // host is up when we didn't arrive here with time out

    //--------------
    // at this place the previous SPI transfer must have ended - either by success, or by timeout
    // disable both TX and RX channels
    DMA1_Channel3->CCR &= 0xfffffffe;
    DMA1_Channel2->CCR &= 0xfffffffe;

    //-------------------
    // The next simple 'if' is here to help the last word of block loss (first word of block not present),
    // it doesn't do much (just gets a byte from RX register if there is one waiting), but it helps the situation -
    // without it the problem occures, with it it seems to be gone (can't reproduce). This might be caused just
    // by the adding the delay between disabling and enabling DMA by this extra code.

    if((SPI1->SR & SPI_SR_RXNE) != 0) {                                 // if there's something still in SPI DR, read it
        WORD dummy = SPI1->DR;
    }
    //-------------------

    // set the software flags of SPI DMA being idle
    spiDmaTXidle = (txCount == 0) ? TRUE : FALSE;                       // if nothing to send, then IDLE; if something to send, then FALSE
    spiDmaRXidle = (rxCount == 0) ? TRUE : FALSE;                       // if nothing to receive, then IDLE; if something to receive, then FALSE
    spiDmaIsIdle = FALSE;                                               // SPI DMA is busy

    // config SPI1_TX -- DMA1_CH3
    DMA1_Channel3->CMAR     = (uint32_t) txBfr;                         // from this buffer located in memory
    DMA1_Channel3->CNDTR    = txCount;                                  // this much data

    // config SPI1_RX -- DMA1_CH2
    DMA1_Channel2->CMAR     = (uint32_t) rxBfr;                         // to this buffer located in memory
    DMA1_Channel2->CNDTR    = rxCount;                                  // this much data

    // enable both TX and RX channels
    DMA1_Channel3->CCR      |= 1;                                       // enable  DMA1 Channel transfer
    DMA1_Channel2->CCR      |= 1;                                       // enable  DMA1 Channel transfer

    // now set the ATN pin accordingly
    if(txCount != 0) {                                                  // something to send over SPI?
        GPIOB->BSRR = ATN;                                              // ATTENTION bit high - got something to read
    }

    timeoutStart();                                                     // start a time-out timer -- give whole timeout time for transfer
}

// the interrupt on DMA SPI TX finished should minimize the need for checking and reseting ATN pin
void DMA1_Channel3_IRQHandler(void)
{
    DMA_ClearITPendingBit(DMA1_IT_TC3);                                 // possibly DMA1_IT_GL3 | DMA1_IT_TC3

    GPIOB->BRR = ATN;                                                   // ATTENTION bit low  - nothing to read

    spiDmaTXidle = TRUE;                                                // SPI DMA TX now idle

    if(spiDmaRXidle == TRUE) {                                          // and if even the SPI DMA RX is idle, SPI is idle completely
        spiDmaIsIdle = TRUE;                                            // SPI DMA is busy

        if(outFlags.weAreReceivingTrack) {                              // if we were receiving a track
            outFlags.weAreReceivingTrack    = FALSE;                    // mark that we're not receiving it anymore
            outFlags.updatePosition         = TRUE;
        }
    }
}

// interrupt on Transfer Complete of SPI DMA RX channel
void DMA1_Channel2_IRQHandler(void)
{
    DMA_ClearITPendingBit(DMA1_IT_TC2);                                 // possibly DMA1_IT_GL2 | DMA1_IT_TC2

    spiDmaRXidle = TRUE;                                                // SPI DMA RX now idle

    if(spiDmaTXidle == TRUE) {                                          // and if even the SPI DMA TX is idle, SPI is idle completely
        spiDmaIsIdle = TRUE;                                            // SPI DMA is busy

        if(outFlags.weAreReceivingTrack) {                              // if we were receiving a track
            outFlags.weAreReceivingTrack    = FALSE;                    // mark that we're not receiving it anymore
            outFlags.updatePosition         = TRUE;
        }
    }
}

void EXTI3_IRQHandler(void)
{
  if(EXTI_GetITStatus(EXTI_Line3) != RESET) {
        WORD inputs;
        WORD curIntTime, difIntTime;
        static WORD prevIntTime = 0;

        EXTI_ClearITPendingBit(EXTI_Line3);             // Clear the EXTI line pending bit
        inputs = GPIOB->IDR;

        //---------
        // now check if the step pulse isn't too soon after the previous one
        curIntTime = TIM4->CNT;                         // get current time -- 2 kHz timer
        difIntTime = curIntTime - prevIntTime;          // calc only difference

        if(difIntTime > 255) {
            difIntTime = 255;
        }

        if(difIntTime < 2) {                            // if the difference is less than 1 ms, quit
                return;
        }

        prevIntTime = curIntTime;                       // store as previous time
        //---------

        if((inputs & MOTOR_ENABLE) != 0) {              // motor not enabled? Skip the following code.
            return;
        }

        if(inputs & DIR) {                              // direction is High? track--
            if(now.track > 0) {
                now.track--;

                trackRequest(FALSE);
            }
        } else {                                        // direction is Low? track++
            if(now.track < 85) {
                now.track++;

                trackRequest(FALSE);
            }
        }

        beeperToggle(1);

        if(now.track == 0) {                            // if track is 0
            GPIOB->BRR = TRACK0;                        // TRACK 0 signal to L
        } else {                                                    // if track is not 0
            GPIOB->BSRR = TRACK0;                       // TRACK 0 signal is H
        }
  }
}

void fillMfmTimesForDMA(void)
{
    WORD ind = 0;
    BYTE times4, time, i;

    // code to ARR value:      ??, 4us, 6us, 8us
    static WORD mfmTimes[4] = { 7,   7,  11,  15};

    // check for half transfer or transfer complete IF
    if((DMA1->ISR & DMA1_IT_TC5) != 0) {            // TCIF5 -- Transfer Complete IF 5
        ind = 8;
    }

    DMA1->IFCR = DMA1_IT_TC5 | DMA1_IT_HT5;         // clear HT5 and TC5 flag

    for(i=0; i<8; i++) {                            // convert all 4 codes to ARR values
        if(i==0 || i==4) {
            times4 = getNextMFMbyte();              // get next BYTE
        }

        time        = times4 >> 6;                  // get bits 15,14 (and then 13,12 ... 1,0)
        time        = mfmTimes[time];               // convert to ARR value
        times4  = times4 << 2;                      // shift 2 bits higher so we would get lower bits next time

        mfmReadStreamBuffer[ind] = time;            // store and move to next one
        ind++;
    }
}

void fillMfmTimesWithDummy(void)
{
    DMA1->IFCR = DMA1_IT_TC5 | DMA1_IT_HT5;         // clear HT5 and TC5 flag
    fillReadStreamBufferWithDummyData();
}

BYTE getNextMFMbyte(void)
{
    BYTE val;

    WORD maxLoops = READTRACKDATA_SIZE_BYTES;

    while(1) {                                      // go through readTrackData to process commands and to find some data
        maxLoops--;

        if(maxLoops == 0) {                         // didn't quit the loop for 15k cycles? quit now!
            break;
        }

        readTrackData_get_noMove(val);              // get BYTE from buffer, but don't move

        if(val == CMD_TRACK_STREAM_END_BYTE) {      // we've hit the end of track stream? quit loop
            break;
        }

        readTrackData_justMove();                   // just move to the next position

        if(val == 0) {                              // skip empty WORDs
            continue;
        }

        // lower nibble == 0? it's a command from host - if we should turn on/off the write protect or disk change
        if(val == CMD_CURRENT_SECTOR) {             // it's a command?
            readTrackData_get(streamed.side);       // store side   #
            readTrackData_get(streamed.track);      // store track  #
            readTrackData_get(streamed.sector);     // store sector #
        } else {                                                        // not a command? return it
            return val;
        }
    }

    //---------
    // if we got here, we have no data to stream - just stream encoded zeros like in GAP 2 or GAP 3b
    return 0x55;
}

void updateStreamPositionByFloppyPosition(void)
{
    DWORD mediaPosition;
    DWORD streamSize = readTrackDataBfr[STREAM_TABLE_OFFSET];       // get stream size (in bytes) from stream table at index 0

    if(streamSize >= (READTRACKDATA_SIZE_BYTES - 1)) {              // if stream size is invalid (is bigger than where we store read track data)
        inIndexGet = STREAM_START_OFFSET;                           // just go to the start of stream
        return;
    }

    // read the current position - from 0 to 400
    mediaPosition = TIM2->CNT;

    // calculate index where we should place sream reading index -
    // current position is between 0 and 400, that is from 0 to 100%, so place it between 0 and LENGTH OF STREAM position
    inIndexGet = (streamSize * mediaPosition) / 400;
}

void setMode(BYTE newFranzMode, BYTE newSoundOn)
{
    if(newFranzMode == franzMode && newSoundOn == soundOn) {    // no change in mode and sound - just quit
        return;
    }

    // update mode, sound and dir bit
    franzMode = newFranzMode;
    soundOn = newSoundOn;
}

void processHostCommand(BYTE val)
{
    switch(val) {
        case CMD_WRITE_PROTECT_OFF: isWriteProtected    = FALSE;    setupDiskChangeWriteProtect();  break;  // not write protected
        case CMD_WRITE_PROTECT_ON:  isWriteProtected    = TRUE;     setupDiskChangeWriteProtect();  break;  // is write protected
        case CMD_DISK_CHANGE_OFF:   isDiskChanged       = FALSE;    setupDiskChangeWriteProtect();                          break;  // not changed
        case CMD_DISK_CHANGE_ON:    isDiskChanged       = TRUE;     setupDiskChangeWriteProtect();  trackRequest(TRUE);     break;  // has changed
        case CMD_GET_FW_VERSION:    sendFwVersion       = TRUE;                                     break;  // send FW version string and receive commands
        case CMD_SET_DRIVE_ID_0:    driveId             = 0;        setupDriveSelect();             break;  // set drive ID pins to check like this...
        case CMD_SET_DRIVE_ID_1:    driveId             = 1;        setupDriveSelect();             break;  // ...or that!
        case CMD_DRIVE_ENABLED:     driveEnabled        = TRUE;     setupDriveSelect();             break;  // drive is now enabled
        case CMD_DRIVE_DISABLED:    driveEnabled        = FALSE;    setupDriveSelect();             break;  // drive is now disabled

        case CMD_FRANZ_MODE_1:              setMode(FRANZ_MODE_V1_V2, FALSE);   break;  // set Franz in v1/v2 mode
        case CMD_FRANZ_MODE_4_SOUND_ON:     setMode(FRANZ_MODE_V4, TRUE);       break;  // set Franz in v4 mode with sound
        case CMD_FRANZ_MODE_4_SOUND_OFF:    setMode(FRANZ_MODE_V4, FALSE);      break;  // set Franz in v4 mode without sound
    }
}

void setupDriveSelect(void)
{
    if(driveEnabled == FALSE) {     // if drive not enabled, set drive_select mask to everything, so it will (probably) never find out that it's selected
        drive_select = 0xffff;
        return;
    }

    // if we got here, drive is enabled
    if(driveId == 0) {              // when drive ID is 0
        drive_select = MOTOR_ENABLE | DRIVE_SELECT0;
    } else {                        // when drive ID is 1
        drive_select = MOTOR_ENABLE | DRIVE_SELECT1;
    }
}

void setupDiskChangeWriteProtect(void)
{
    if(isDiskChanged) {                     // if disk HAS changed, write protect is inverted
        if(isWriteProtected) {
            GPIOB->BSRR     = WR_PROTECT;   // WR PROTECT to 1
        } else {
            GPIOB->BRR      = WR_PROTECT;   // WR PROTECT to 0
        }
        GPIOB->BRR          = DISK_CHANGE;  // DISK_CHANGE to 0
    } else {                                // if disk NOT changed, write protect behaves normally
        if(isWriteProtected) {
            GPIOB->BRR      = WR_PROTECT;   // WR PROTECT to 0
        } else {
            GPIOB->BSRR     = WR_PROTECT;   // WR PROTECT to 1
        }

        GPIOB->BSRR         = DISK_CHANGE;  // DISK_CHANGE to 1
    }
}

