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


// 1 sector with all headers, gaps, data and crc: 614 B of data needed to stream -> 4912 bits to stream -> with 4 bits encoded to 1 byte: 1228 of encoded data
// Each sector takes around 22ms to stream out, so you have this time to ask for new one and get it in... the SPI transfer of 1228 B should take 0,8 ms.

// gap byte 0x4e == 666446 us = 222112 times = 10'10'10'01'01'10 = 0xa96
// first gap (60 * 0x4e) takes 1920 us (1.9 ms)

// This ARM is little endian, e.g. 0x1234 is stored in RAM as 34 12, but when working with WORDs, it's OK

// maximum params from .ST images seem to be: 84 tracks (0-83), 10 sectors/track

TWriteBuffer wrBuffer[2];                           // two buffers for written sectors
TWriteBuffer *wrNow;

SStreamed streamed;

WORD mfmReadStreamBuffer[16];                           // 16 words - 16 mfm times. Half of buffer is 8 times - at least 32 us (8 * 4us),

WORD mfmWriteStreamBuffer[16];
//WORD lastMfmWriteTC;

// cycle measure: t1 = TIM3->CNT;   t2 = TIM3->CNT; dt = t2 - t1; -- subtrack 0x12 because that's how much measuring takes
WORD t1, t2, dt;

// digital osciloscope time measure on GPIO B0:
//          GPIOB->BSRR = 1;    // 1
//          GPIOB->BRR = 1;     // 0

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

#define     readTrackData_goToStart()       {                                                                                                           inIndexGet = 0;    }
#define     readTrackData_get(X)            { X = readTrackData[inIndexGet];                inIndexGet++;       if(inIndexGet >= READTRACKDATA_SIZE) {  inIndexGet = 0; }; }
#define     readTrackData_get_noMove(X)     { X = readTrackData[inIndexGet];                                                                                                                                    }
//#define       readTrackData_markAndmove() { readTrackData[inIndexGet] = CMD_MARK_READ;    inIndexGet++;       if(inIndexGet >= READTRACKDATA_SIZE) {  inIndexGet = 0; };   }
#define     readTrackData_justMove()        {                                                                                       inIndexGet++;       if(inIndexGet >= READTRACKDATA_SIZE) { inIndexGet = 0; };   }
//--------------

#define REQUEST_TRACK                       {   next.track = now.track; next.side = now.side; sendTrackRequest = TRUE; lastRequestTime = TIM4->CNT; trackStreamedCount = 0; }
#define FORCE_REQUEST_TRACK                 {   REQUEST_TRACK;      lastRequestTime -= 35;      lastRequested.track = 0xff;     lastRequested.side = 0xff;                  }

WORD version[2] = {0xf018, 0x1205};             // this means: Franz, 2018-12-05
WORD drive_select;

volatile BYTE sendFwVersion, sendTrackRequest;

WORD atnSendFwVersion       [ATN_SENDFWVERSION_LEN_TX];
BYTE cmdBuffer              [ATN_SENDFWVERSION_LEN_RX * 2];

WORD atnSendTrackRequest    [ATN_SENDTRACK_REQ_LEN_TX];
BYTE readTrackData          [READTRACKDATA_SIZE];
WORD inIndexGet;

WORD fakeBuffer;

volatile WORD prevIntTime;

volatile BYTE spiDmaIsIdle;
volatile BYTE spiDmaTXidle, spiDmaRXidle;       // flags set when the SPI DMA TX or RX is idle

volatile TDrivePosition now, next, lastRequested, prev;
volatile WORD lastRequestTime;

BYTE hostIsUp;                                  // used to just pass through IKBD until RPi is up

BYTE driveId;
BYTE driveEnabled;

BYTE isDiskChanged;
BYTE isWriteProtected;

WORD trackStreamedCount = 0;

volatile TCircBuffer buff0, buff1;
void circularInit(volatile TCircBuffer *cb);
void cicrularAdd(volatile TCircBuffer *cb, BYTE val);
BYTE cicrularGet(volatile TCircBuffer *cb);

void setupDriveSelect(void);
void setupDiskChangeWriteProtect(void);

void fillReadStreamBufferWithDummyData(void);
void fillMfmTimesWithDummy(void);
TOutputFlags outFlags;

void updateStreamPositionByFloppyPosition(void);

void handleFloppyWrite(void);

BYTE sectorsWritten;            // how many sectors were written during the last media rotation - if something was written, we need to get the re-encoded track
WORD wrPulseShort, wrPulseLong; // if write pulse is too short of too long, increment here

int main (void)
{
    BYTE indexCount     = 0;
    WORD WGate;
    BYTE spiDmaIsIdle   = TRUE;

    prevIntTime = 0;
    sectorsWritten = 0;         // nothing written yet

    sendFwVersion       = FALSE;
    sendTrackRequest    = FALSE;

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

    REQUEST_TRACK;                                  // request track 0, side 0

    while(1) {
        WORD inputs;

        if(sendTrackRequest) {                          // if we're already waiting for the new TRACK to be sent, consider this as we are already receiving new track data
            fillReadStreamBufferWithDummyData();        // fill MFM stream with dummy data so we won't stream any junk
            outFlags.weAreReceivingTrack = TRUE;        // mark that we are receiving TRACK data, and thus shouldn't stream
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

        // sending and receiving data over SPI using DMA
        if(spiDmaIsIdle == TRUE) {                                                              // SPI DMA: nothing to Tx and nothing to Rx?
            if(sendFwVersion) {                                                                 // should send FW version? this is a window for receiving commands
                if(isDiskChanged) {                                                             // if it was held for a while, take it down
                    isDiskChanged = FALSE;
                    setupDiskChangeWriteProtect();
                }

                timeoutStart();                                                                 // start a time-out timer

                spiDma_txRx(ATN_SENDFWVERSION_LEN_TX, (BYTE *) &atnSendFwVersion[0], ATN_SENDFWVERSION_LEN_RX, (BYTE *) &cmdBuffer[0]);

                sendFwVersion   = FALSE;
            } else if(sendTrackRequest) {                                                       // if should send track request
                // check how much time passed since the request was created
                WORD timeNow    = TIM4->CNT;
                WORD diff       = timeNow - lastRequestTime;

                if(diff >= 30) {                                                                // and at least 15 ms passed since the request (30 / 2000 s)
                    sendTrackRequest    = FALSE;

                    // first check if this isn't what we've requested last time
                    if(next.track != lastRequested.track || next.side != lastRequested.side) {  // if track or side changed -- same track request limiter
                        lastRequested.track = next.track;                                       // mark what we've requested last time
                        lastRequested.side  = next.side;

                        timeoutStart();                                                         // start a time-out timer

                        atnSendTrackRequest[4] = (((WORD)next.side) << 8) | (next.track);
                        spiDma_txRx(ATN_SENDTRACK_REQ_LEN_TX, (BYTE *) &atnSendTrackRequest[0], ATN_SENDTRACK_REQ_LEN_RX, (BYTE *) &readTrackData[0]);

                        outFlags.weAreReceivingTrack = TRUE;                                    // mark that we started to receive TRACK data
                    }
                }
            } else if(wrNow->readyToSend) {                                                     // not sending any ATN right now? and current write buffer has something?
                timeoutStart();                                                                 // start a time-out timer

                spiDma_txRx(wrNow->count, (BYTE *) &wrNow->buffer[0], 1, (BYTE *) &fakeBuffer);

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

            for(i=0; i<12; i++) {
                processHostCommand(cmdBuffer[i]);
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
                FORCE_REQUEST_TRACK;        // ask for the changed track data, but force it - get it immediatelly
            }

            //-----------
            // the following section of code should request track again if even after 2 rotations of floppy we're not streaming what we should
            trackStreamedCount++;               // increment the count of how many times we've streamed this track

            if(trackStreamedCount >= 2) {       // if since the last request 2 rotations happened
                if(streamed.track != now.track || streamed.side != now.side) {  // and we're not streaming what we really want to stream
                    REQUEST_TRACK;              // ask for track data (again?)
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
            REQUEST_TRACK;                      // we need track from the right side
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

void spiDma_txRx(WORD txCount, BYTE *txBfr, WORD rxCount, BYTE *rxBfr)
{
    static WORD ATNcodePrev = 0;

    WORD *pTxBfr = (WORD *) txBfr;

    // store TX and RX count so the host will know how much he should transfer
    pTxBfr[2] = txCount;
    pTxBfr[3] = rxCount;

    spiDma_waitForFinish();                                             // make sure that the last DMA transfer has finished

    waitForSPIidle();                                                   // and make sure that SPI has finished, too

    //--------------
    // at this place the previous SPI transfer must have ended - either by success, or by timeout
    if(ATNcodePrev == ATN_FW_VERSION) {                                 // if the last ATN code was FW version (Franz hearthbeat / RPi alive)
        if(DMA1_Channel3->CNDTR == 0 && DMA1_Channel2->CNDTR == 0) {    // if both SPI TX and SPI RX transmitted all the data, RPi is up
            hostIsUp = TRUE;                                            // mark that RPi retrieved the data
        } else {
            hostIsUp = FALSE;                                           // mark that RPi didn't retrieve the data
        }
    }

    ATNcodePrev = pTxBfr[1];                                            // store this current ATN code as previous code, so next time we know what ATN succeeded or failed

    //--------------

    // disable both TX and RX channels
    DMA1_Channel3->CCR      &= 0xfffffffe;                              // disable DMA3 Channel transfer
    DMA1_Channel2->CCR      &= 0xfffffffe;                              // disable DMA2 Channel transfer

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
}

void spiDma_waitForFinish(void)
{
    while(spiDmaIsIdle != TRUE) {                                       // wait until it will become idle
        if(timeout()) {                                                 // if timeout happened (and we got stuck here), quit
            spiDmaIsIdle = TRUE;

            if(outFlags.weAreReceivingTrack) {                          // if we were receiving a track
                outFlags.weAreReceivingTrack    = FALSE;                // mark that we're not receiving it anymore
                outFlags.updatePosition         = TRUE;
            }

            break;
        }
    }
}

void waitForSPIidle(void)
{
    while((SPI1->SR & SPI_SR_TXE) == 0) {                               // wait while TXE flag is 0 (TX is not empty)
        if(timeout()) {
            return;
        }
    }

    while((SPI1->SR & SPI_SR_BSY) != 0) {                               // wait while BSY flag is 1 (SPI is busy)
        if(timeout()) {
            return;
        }
    }
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

                REQUEST_TRACK;
            }
        } else {                                        // direction is Low? track++
            if(now.track < 85) {
                now.track++;

                REQUEST_TRACK;
            }
        }

        if(now.track == 0) {                            // if track is 0
            GPIOB->BRR = TRACK0;                        // TRACK 0 signal to L
        } else {                                                    // if track is not 0
            GPIOB->BSRR = TRACK0;                       // TRACK 0 signal is H
        }
  }
}

void moveReadIndexToNextSector(void)
{
    // move READ pointer further, as we weren't moving it while write was active
    BYTE *pStart = &readTrackData[0];                       // where the stream starts
    BYTE *pNow = &readTrackData[inIndexGet];                // where we are now
    BYTE *pEnd = &readTrackData[inIndexGet + 1200];         // where should we end the search
    BYTE *pMax = &readTrackData[READTRACKDATA_SIZE - 1];    // end of track - if we got here, nothing more to search through

    if(pEnd > pMax) {               // if we would be going out of track buffer, limit it here to end of track buffer
        pEnd = pMax;
    }

    while(pNow <= pEnd) {           // try to find next sector marker
        BYTE val = *pNow;

        if(val == CMD_TRACK_STREAM_END_BYTE) {  // didn't find next sector, but end of stream?
            inIndexGet = (pNow - pStart) - 8;   // store this as new index, let the other part of streaming find the end of track
            break;
        }

        if(val == CMD_CURRENT_SECTOR) {         // found start of new sector?
            inIndexGet = (pNow - pStart) - 30;  // start streaming again few bytes before new / next sector
            break;
        }

        pNow++;                     // move further
    }
}

#define SW_WRITE

#ifdef SW_WRITE
void handleFloppyWrite(void)
{
    WORD inputs = GPIOB->IDR;
    WORD times = 0, timesCount = 0;
    WORD wval;
    register WORD newTime, duration;
    WORD *pWriteNow, *pWriteEnd;

    // software write capturing
    WORD wData = inputs & WDATA;    // initialize wData and wDataPrev
    WORD wDataPrev = wData;

    __disable_irq();                // disable interrupts

    TIM3->CNT = 0;                  // initialize pulse width counter

    wrNow->readyToSend = FALSE;     // mark this buffer as not ready to be sent yet
    wrNow->count = 4;               // at the start we already have 4 WORDs in buffer - SYNC, ATN code, TX len, RX len

    pWriteNow = &wrNow->buffer[wrNow->count];           // where we want to store current written data (using pointer instead of index for speed reasons)
    pWriteEnd = &wrNow->buffer[WRITEBUFFER_SIZE - 4];   // end of write buffer - terminate write loop if we get here (minus some space at the end for additional data)

    wval = (streamed.track << 8) | streamed.sector; // next word (buffer[4]) is side, track, sector

    if(streamed.side != 0) {        // if side is not 0, set the highest bit
        wval |= 0x8000;
    }

    *pWriteNow = wval;              // add this side track sector WORD
    pWriteNow++;

    *pWriteNow = inIndexGet;        // store inIndexGet in the stream, so RPi can guess the right sector from our stream position when write happened
    pWriteNow++;

    while(1) {
        inputs = GPIOB->IDR;        // read inputs

        if(inputs & WGATE) {        // if WGATE isn't L, writing finished
            break;
        }

        // software WRITE capturing - might be prone to errors due to interrupts
        wDataPrev = wData;          // store previous state of WDATA
        wData = inputs & WDATA;     // get   current  state of WDATA

        if(wDataPrev != WDATA || wData != 0) {    // not falling edge or WDATA? try again (we want 1,0; anything else is ignored)
            continue;
        }

        duration = TIM3->CNT;       // get current pulse duration

        if(duration < 20) {         // if this pulse is too short
            wrPulseShort++;
            continue;
        }

        TIM3->CNT = 0;              // reset timer back to zero
        newTime = 0;

        if(duration >= 65) {        // if the pulse is too long
             wrPulseLong++;
             continue;
        }

        if(duration < 36) {         // 4 us?
            newTime = MFM_4US;
        } else if(duration < 50) {  // 6 us?
            newTime = MFM_6US;
        } else if(duration < 65) {  // 8 us?
            newTime = MFM_8US;
        }

        if(!newTime) {           // don't have valid new time? skip rest
            continue;
        }

        times = times << 2;
        times |= newTime;       // append new time

        timesCount++;
        if(timesCount >= 8) {   // if already got 8 times stored
            timesCount = 0;

            *pWriteNow = times; // add to write buffer
            pWriteNow++;

            if(pWriteNow >= pWriteEnd) {    // no more space in write buffer, send it to host
                break;
            }
        }
    }

    __enable_irq();                // enable interrupts

    // writing finished
    if(timesCount > 0) {        // if there was something captured but not stored at the end
        *pWriteNow = times;     // add to write buffer
        pWriteNow++;
    }

    *pWriteNow = 0;             // last word: 0
    pWriteNow++;

    wrNow->count = (pWriteNow - wrNow->buffer);         // calculate how many words we got in write buffer (pointer subtraction gives the number of elements between the two pointers)

    wrNow->readyToSend = TRUE;                          // mark this buffer as ready to be sent

    moveReadIndexToNextSector();
}
#else
void handleFloppyWrite(void)
{
    WORD inputs = GPIOB->IDR;
    WORD times = 0, timesCount = 0;
    WORD wval;
    register WORD newTime, duration;
    WORD *pWriteNow, *pWriteEnd;

    DWORD dval;
    WORD i, start;
    register WORD prevTime = 0;
    WORD *pMfmWriteStreamBuffer;

    DMA1->IFCR = DMA1_IT_HT6 | DMA1_IT_TC6; // clear HT & TC flags

    TIM3->CNT = 0;                  // initialize pulse width counter

    wrNow->readyToSend = FALSE;     // mark this buffer as not ready to be sent yet
    wrNow->count = 4;               // at the start we already have 4 WORDs in buffer - SYNC, ATN code, TX len, RX len

    pWriteNow = &wrNow->buffer[wrNow->count];           // where we want to store current written data (using pointer instead of index for speed reasons)
    pWriteEnd = &wrNow->buffer[WRITEBUFFER_SIZE - 4];   // end of write buffer - terminate write loop if we get here (minus some space at the end for additional data)

    wval = (streamed.track << 8) | streamed.sector; // next word (buffer[4]) is side, track, sector

    if(streamed.side != 0) {        // if side is not 0, set the highest bit
        wval |= 0x8000;
    }

    *pWriteNow = wval;              // add this side track sector WORD
    pWriteNow++;

    *pWriteNow = inIndexGet;        // store inIndexGet in the stream, so RPi can guess the right sector from our stream position when write happened
    pWriteNow++;

    while(1) {
        inputs = GPIOB->IDR;        // read inputs

        if(inputs & WGATE) {        // if WGATE isn't L, writing finished
            break;
        }

        if(pWriteNow >= pWriteEnd) {            // no more space in write buffer, send it to host
            break;
        }

        // hardware WRITE capturing
        // check for half transfer or transfer complete IF
        dval = DMA1->ISR & (DMA1_IT_HT6 | DMA1_IT_TC6);     // get only Half-Transfer and Transfer-Complete flags
        if(!dval) {     // no HT and TC flag set, try again later
            continue;
        }
        DMA1->IFCR = DMA1_IT_HT6 | DMA1_IT_TC6; // clear HTIF6 flags

        start = (dval & DMA1_IT_HT6) ? 0 : 8;   // HT is words 0..7, TC is words 8..15

        // using pointer to write buffer is faster than using index (twice):
        // duration + prevTime using index: 2.75 us, the same using pointer: 1.62 us, the sae using pointer acces only once: 1.25 us
        // whole loop: 3.51 us on 4 us pulse without storing, 3.87 us on 8 us pulse without storing, aditional 0.62 us us for storing
        pMfmWriteStreamBuffer = &mfmWriteStreamBuffer[start];

        for(i=0; i<8; i++) {             // go through the stored values (either 0-7 or 8-15), calculate duration
            wval     = *pMfmWriteStreamBuffer;
            duration = wval - prevTime;
            prevTime = wval;
            pMfmWriteStreamBuffer++;

            newTime = 0;                // convert timer time to pulse duration enum

            if(duration < 36) {         // 4 us?
                newTime = MFM_4US;
            } else if(duration < 50) {  // 6 us?
                newTime = MFM_6US;
            } else if(duration < 65) {  // 8 us?
                newTime = MFM_8US;
            }

            if(!newTime) {              // don't have valid new time? skip rest
                continue;
            }

            times = times << 2;
            times |= newTime;           // append new time

            timesCount++;

            // the following storing takes 0.62 us (was 7.5 us using the wrBuffer_add() macro)
            if(timesCount >= 8) {       // if already got 8 times stored
                timesCount = 0;

                *pWriteNow = times;     // add to write buffer
                pWriteNow++;
            }
        }
    }

    // writing finished
    if(timesCount > 0) {        // if there was something captured but not stored at the end
        *pWriteNow = times;     // add to write buffer
        pWriteNow++;
    }

    *pWriteNow = 0;             // last word: 0
    pWriteNow++;

    wrNow->count = (pWriteNow - wrNow->buffer);         // calculate how many words we got in write buffer (pointer subtraction gives the number of elements between the two pointers)

    wrNow->readyToSend = TRUE;                          // mark this buffer as ready to be sent

    moveReadIndexToNextSector();
}
#endif

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

    WORD maxLoops = READTRACKDATA_SIZE;

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
    DWORD steamSize, mediaPosition;

    BYTE *pStart = &readTrackData[0];                       // where the stream starts
    BYTE *pEnd = &readTrackData[READTRACKDATA_SIZE - 1];    // end of track - if we got here, nothing more to search through
    BYTE *pNow = pStart;                                    // where we will start the search

    // find end of stream, it will hold the last index where we can go (= LENGTH OF STREAM in bytes)
    while(pNow < pEnd) {
        if(*pNow == CMD_TRACK_STREAM_END_BYTE) {
            streamSize = pNow - pStart;      // calculate index of end of stream
            break;
        }

        pNow++;                     // advance to next position
    }

    // read the current position - from 0 to 400
    mediaPosition = TIM2->CNT;

    // calculate index where we should place sream reading index -
    // current position is between 0 and 400, that is from 0 to 100%, so place it between 0 and LENGTH OF STREAM position
    inIndexGet = (streamSize * mediaPosition) / 400;
}

void processHostCommand(BYTE val)
{
    switch(val) {
        case CMD_WRITE_PROTECT_OFF: isWriteProtected    = FALSE;    setupDiskChangeWriteProtect();  break;  // not write protected
        case CMD_WRITE_PROTECT_ON:  isWriteProtected    = TRUE;     setupDiskChangeWriteProtect();  break;  // is write protected
        case CMD_DISK_CHANGE_OFF:   isDiskChanged       = FALSE;    setupDiskChangeWriteProtect();                          break;  // not changed
        case CMD_DISK_CHANGE_ON:    isDiskChanged       = TRUE;     setupDiskChangeWriteProtect();  FORCE_REQUEST_TRACK;    break;  // has changed
        case CMD_GET_FW_VERSION:    sendFwVersion       = TRUE;                                     break;  // send FW version string and receive commands
        case CMD_SET_DRIVE_ID_0:    driveId             = 0;        setupDriveSelect();             break;  // set drive ID pins to check like this...
        case CMD_SET_DRIVE_ID_1:    driveId             = 1;        setupDriveSelect();             break;  // ...or that!
        case CMD_DRIVE_ENABLED:     driveEnabled        = TRUE;     setupDriveSelect();             break;  // drive is now enabled
        case CMD_DRIVE_DISABLED:    driveEnabled        = FALSE;    setupDriveSelect();             break;  // drive is now disabled
    }
}

// --------------------------------------------------
// UART 1 - TX and RX - connects Franz and RPi, 19200 baud
// UART 2 - TX - data from RPi     - through buff0 - to IKBD
// UART 2 - RX - data from IKBD    - through buff1 - to RPi (also connected with wire to ST keyb)
// UART 3 - RX - data from ST keyb - through buff1 - to RPi
//
// Flow with RPi:
// IKBD talks to ST keyboard (direct wire connection), and also talks through buff1 to RPi
// ST keyb talks through buff1 to RPi
// RPi talks to IKBD through buff0
//
// Flow without RPi:
// IKBD talks to ST keyboard - direct wire connection
// ST keyb talks to IKBD - through buff0
//
// --------------------------------------------------

void USART1_IRQHandler(void)                                            // USART1 is connected to RPi
{
    if((USART1->SR & USART_FLAG_RXNE) != 0) {                           // if something received
        BYTE val = USART1->DR;                                          // read received value

        if(buff0.count > 0 || (USART2->SR & USART_FLAG_TXE) == 0) {     // got data in buffer or usart2 can't TX right now?
            cicrularAdd(&buff0, val);                                   // add to buffer
            USART2->CR1 |= USART_FLAG_TXE;                              // enable interrupt on USART TXE
        } else {                                                        // if no data in buffer and usart2 can TX
            USART2->DR = val;                                           // send it to USART2
        }
    }

    if((USART1->SR & USART_FLAG_TXE) != 0) {                            // if can TX
        if(buff1.count > 0) {                                           // and there is something to TX
            BYTE val = cicrularGet(&buff1);
            USART1->DR = val;
        } else {                                                        // and there's nothing to TX
            USART1->CR1 &= ~USART_FLAG_TXE;                             // disable interrupt on USART2 TXE
        }
    }
}

#define UARTMARK_STCMD      0xAA
#define UARTMARK_KEYBDATA   0xBB

void USART2_IRQHandler(void)                                            // USART2 is connected to ST IKBD port
{
    if((USART2->SR & USART_FLAG_RXNE) != 0) {                           // if something received
        BYTE val = USART2->DR;                                          // read received value

        if(hostIsUp) {                                                  // RPi is up - buffered send to RPi
            if(buff1.count > 0 || (USART1->SR & USART_FLAG_TXE) == 0) { // got data in buffer or usart1 can't TX right now?
                cicrularAdd(&buff1, UARTMARK_STCMD);                    // add to buffer - MARK
            } else {                                                    // if no data in buffer and usart2 can TX
                USART1->DR = UARTMARK_STCMD;                            // send to USART1 - MARK
            }

            cicrularAdd(&buff1, val);                                   // add to buffer - DATA
            USART1->CR1 |= USART_FLAG_TXE;                              // enable interrupt on USART TXE
        } else {                                                        // if RPi is not up, something received from ST?
            // nothing to do, data should automatically continue to ST keyboard
        }
    }

    if((USART2->SR & USART_FLAG_TXE) != 0) {                            // if can TX
        if(buff0.count > 0) {                                           // and there is something to TX
            BYTE val = cicrularGet(&buff0);
            USART2->DR = val;
        } else {                                                        // and there's nothing to TX
            USART2->CR1 &= ~USART_FLAG_TXE;                             // disable interrupt on USART2 TXE
        }
    }
}

void USART3_IRQHandler(void)                                            // USART3 is connected to original ST keyboard
{
    if((USART3->SR & USART_FLAG_RXNE) != 0) {                           // if something received
        BYTE val = USART3->DR;                                          // read received value

        if(hostIsUp) {                                                  // RPi is up - buffered send to RPi
            if(buff1.count > 0 || (USART1->SR & USART_FLAG_TXE) == 0) { // got data in buffer or usart1 can't TX right now?
                cicrularAdd(&buff1, UARTMARK_KEYBDATA);                 // add to buffer - MARK
            } else {                                                    // if no data in buffer and usart2 can TX
                USART1->DR = UARTMARK_KEYBDATA;                         // send to USART1 - MARK
            }

            cicrularAdd(&buff1, val);                                   // add to buffer - DATA
            USART1->CR1 |= USART_FLAG_TXE;                              // enable interrupt on USART TXE
        } else {                                                        // if RPi is not up - send it to IKBD (through buffer or immediatelly)
            if(buff0.count > 0 || (USART2->SR & USART_FLAG_TXE) == 0) { // got data in buffer or usart2 can't TX right now?
                cicrularAdd(&buff0, val);                               // add to buffer
                USART2->CR1 |= USART_FLAG_TXE;                          // enable interrupt on USART TXE
            } else {                                                    // if no data in buffer and usart2 can TX
                USART2->DR = val;                                       // send it to USART2
            }
        }
    }
}

void circularInit(volatile TCircBuffer *cb)
{
	BYTE i;

	// set vars to zero
	cb->addPos = 0;
	cb->getPos = 0;
	cb->count = 0;

	// fill data with zeros
	for(i=0; i<CIRCBUFFER_SIZE; i++) {
		cb->data[i] = 0;
	}
}

void cicrularAdd(volatile TCircBuffer *cb, BYTE val)
{
	// if buffer full, fail
	if(cb->count >= CIRCBUFFER_SIZE) {
        return;
	}
    cb->count++;

	// store data at the right position
	cb->data[ cb->addPos ] = val;

	// increment and fix the add position
	cb->addPos++;
	cb->addPos = cb->addPos & CIRCBUFFER_POSMASK;
}

BYTE cicrularGet(volatile TCircBuffer *cb)
{
	BYTE val;

	// if buffer empty, fail
	if(cb->count == 0) {
		return 0;
	}
    cb->count--;

	// buffer not empty, get data
	val = cb->data[ cb->getPos ];

	// increment and fix the get position
	cb->getPos++;
	cb->getPos = cb->getPos & CIRCBUFFER_POSMASK;

	// return value from buffer
	return val;
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

