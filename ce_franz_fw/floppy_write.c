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
#include "ikbd.h"
#include "global_vars.h"

void handleFloppyWrite(void);

void moveReadIndexToNextSector(void)
{
    inIndexGet = STREAM_START_OFFSET;           // set get index to stream start in case we fail to find next sector index

    if(streamed.sector >= 1 && streamed.sector < STREAM_TABLE_ITEMS) {          // if streamed sector seems to be valid (will fit in stream table)
        WORD nextSectorIndex = readTrackDataBfr[STREAM_TABLE_OFFSET + (streamed.sector + 1)];       // get index of next sector from stream table

        if(nextSectorIndex < (READTRACKDATA_SIZE_BYTES - 1)) {    // if next sector index seems to be valid, use it
            inIndexGet = nextSectorIndex;
        }
    }
}

// The SW_WRITE is defined in main.h, where it also enables/disabled HW capturing DMA
// The SW_WRITE can't handle IKBD UARTs and WRITE at the same time, but it's kept here for now.
// Use HW WRITE (that means comment out SW_WRITE in main.h)

#ifdef SW_WRITE
void handleFloppyWrite(void)
{
    WORD inputs = GPIOB->IDR;
    WORD times = 0, timesCount = 0;
    WORD wval;
    register WORD newTime, duration;
    WORD *pWriteNow, *pWriteEnd;

//#define SW_CAPTURE

#ifdef SW_CAPTURE
    // software write capturing
    WORD wData = inputs & WDATA;    // initialize wData and wDataPrev
    WORD wDataPrev = wData;

    #define PULSE_TOO_SHORT 22
    #define PULSE_4US       40
    #define PULSE_6US       56
    #define PULSE_8US       72
#else
    #define PULSE_TOO_SHORT 18
    #define PULSE_4US       36
    #define PULSE_6US       52
    #define PULSE_8US       72
#endif

    TIM3->CNT = 0;                  // initialize pulse width counter

    wrNow->readyToSend = FALSE;     // mark this buffer as not ready to be sent yet
    wrNow->count = 4;               // at the start we already have 4 WORDs in buffer - SYNC, ATN code, TX len, RX len

    pWriteNow = &wrNow->buffer[wrNow->count];           // where we want to store current written data (using pointer instead of index for speed reasons)
    pWriteEnd = &wrNow->buffer[WRITEBUFFER_SIZE - 4];   // end of write buffer - terminate write loop if we get here (minus some space at the end for additional data)

    wval = MAKEWORD(streamed.track, streamed.sector);   // next word (buffer[4]) is side, track, sector

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

#ifdef SW_CAPTURE
        // software WRITE capturing - might be prone to errors due to interrupts
        wDataPrev = wData;          // store previous state of WDATA
        wData = inputs & WDATA;     // get   current  state of WDATA

        if(wDataPrev != WDATA || wData != 0) {    // not falling edge or WDATA? try again (we want 1,0; anything else is ignored)
            continue;
        }

        duration = TIM3->CNT;       // get current pulse duration
#else
        // capturing using Timer Input Capture
        if(!(TIM3->SR & TIM_FLAG_CC1)) {    // CC1IF flag NOT set? wait some more
            continue;
        }

        duration =  TIM3->CCR1;     // get current pulse duration
#endif

        if(duration < PULSE_TOO_SHORT) {  // if this pulse is too short (less than 2.7 us long)
            continue;
        }

        TIM3->CNT = 0;              // reset timer back to zero
        newTime = 0;

        if(duration >= PULSE_8US) { // if the pulse is too long (longer than 9 us)
             continue;
        }

        if(duration < PULSE_4US) {         // 4 us? (interval 2.7 us - 5.0 us) (40 pulses of 0.125 us)
            newTime = MFM_4US;
        } else if(duration < PULSE_6US) {  // 6 us? (interval 5.0 us - 7.0 us) (56 pulses of 0.125 us)
            newTime = MFM_6US;
        } else if(duration < PULSE_8US) {  // 8 us? (interval 7.0 us - 9.0 us) (72 pulses of 0.125 us)
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
#else       // if not SW_WRITE (thus it's HW WRITE)

// HW floppy write capturing
// TIM3_CH1 captures the time when pulse happened, this value is transfered to cyclic buffer using DMA1_CH6.
// This loads off the MCU enough to manage writted data to be OK and to be able to transfer IKDB UARTs during the free time.

WORD *pWriteNow, *pWriteEnd;

__attribute__( ( always_inline ) ) void processMfmWriteBuffer(DWORD DMA1_ISR, BYTE flushRest)
{
    WORD i, wval;
    WORD *pMfmWriteStreamBuffer;
    static WORD start = 0;      // where we will start processing buffer. Also holds last position where we started to process (for flushRest)
    static WORD prevTime = 0;   // previous capture time for calculating duration
    static WORD times = 0, timesCount = 0;
    register WORD newTime, duration;

    if(flushRest) {     // flushRest processing - start position is the opposite of previous start position (if 0 then half, if half then 0)
        start = (start == 0) ? MFM_WRITE_STREAM_SIZE_HALF : 0;
    } else {            // normal processing - find out start position based on the supplied TC flags
        start = (DMA1_ISR & DMA1_IT_HT6) ? 0 : MFM_WRITE_STREAM_SIZE_HALF;   // HT is words 0..half, TC is words half..end
    }

    // using pointer to write buffer is faster than using index (twice):
    // duration + prevTime using index: 2.75 us, the same using pointer: 1.62 us, the same using pointer acces only once: 1.25 us
    // whole loop: 3.51 us on 4 us pulse without storing, 3.87 us on 8 us pulse without storing, aditional 0.62 us us for storing
    pMfmWriteStreamBuffer = &mfmWriteStreamBuffer[start];

    for(i=0; i<MFM_WRITE_STREAM_SIZE_HALF; i++) {   // go through the stored values (first half or second half), calculate duration
        wval     = *pMfmWriteStreamBuffer;
        duration = wval - prevTime;
        prevTime = wval;
        pMfmWriteStreamBuffer++;

        newTime = 0;                        // convert timer time to pulse duration enum

        if(duration < 8 || duration >= 72) {    // too short (< 1.0 us) or too long (8.75 us)? ignore
            continue;
        }

        if(duration < 39) {         // 4 us? (usualy 3.5 - 4.0 us -- 28-32 ticks)
            newTime = MFM_4US;
        } else if(duration < 57) {  // 6 us? (usualy 5.8 - 6.4 us -- 47-51 ticks)
            newTime = MFM_6US;
        } else if(duration < 72) {  // 8 us? (usualy 7.8 - 8.4 us -- 63-67 ticks)
            newTime = MFM_8US;
        }

        times = times << 2;
        times |= newTime;                   // append new time

        timesCount++;

        // the following storing takes 0.62 us (was 7.5 us using the wrBuffer_add() macro)
        if(timesCount >= 8) {               // if already got 8 times stored
            timesCount = 0;

            *pWriteNow = times;             // add to write buffer
            pWriteNow++;

            if(pWriteNow >= pWriteEnd) {    // no more space in write buffer, send it to host
                break;
            }
        }
    }

    if(flushRest) {             // if writing finished and should flush the rest
        if(timesCount > 0) {        // if there was something captured but not stored at the end
            *pWriteNow = times;     // add to write buffer
            pWriteNow++;
        }
    }
}

void handleFloppyWrite(void)
{
    WORD inputs = GPIOB->IDR;
    WORD wval;
    DWORD DMA1_ISR;

    DMA1->IFCR = DMA1_IT_HT6 | DMA1_IT_TC6; // clear HT & TC flags

    TIM3->CNT = 0;                                  // initialize pulse width capturer
    DMA1_Channel6->CNDTR = MFM_WRITE_STREAM_SIZE;   // set CNDTR back to original full value so it would start storing data at start of buffer
    DMA_Cmd(DMA1_Channel6, ENABLE);                 // enable DMA

    wrNow->readyToSend = FALSE;     // mark this buffer as not ready to be sent yet
    wrNow->count = 4;               // at the start we already have 4 WORDs in buffer - SYNC, ATN code, TX len, RX len

    pWriteNow = &wrNow->buffer[wrNow->count];           // where we want to store current written data (using pointer instead of index for speed reasons)
    pWriteEnd = &wrNow->buffer[WRITEBUFFER_SIZE - 4];   // end of write buffer - terminate write loop if we get here (minus some space at the end for additional data)

    wval = MAKEWORD(streamed.track, streamed.sector); // next word (buffer[4]) is side, track, sector

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

        if(pWriteNow >= pWriteEnd) {                    // no more space in write buffer, send it to host
            break;
        }

        // check for half transfer or transfer complete IF
        DMA1_ISR = DMA1->ISR & (DMA1_IT_HT6 | DMA1_IT_TC6);     // get only Half-Transfer and Transfer-Complete flags

        if(!DMA1_ISR) {                                 // no HT and TC flag set, try again later
            continue;
        }

        DMA1->IFCR = DMA1_IT_HT6 | DMA1_IT_TC6;         // clear HTIF6 flags

        processMfmWriteBuffer(DMA1_ISR, FALSE);         // process half buffer, don't flush rest
    }

    processMfmWriteBuffer(0, TRUE);                     // process remaining half buffer, flush rest

    DMA_Cmd(DMA1_Channel6, DISABLE);                    // disable DMA

    *pWriteNow = 0;             // last word: 0
    pWriteNow++;

    wrNow->count = (pWriteNow - wrNow->buffer);         // calculate how many words we got in write buffer (pointer subtraction gives the number of elements between the two pointers)
    wrNow->readyToSend = TRUE;                          // mark this buffer as ready to be sent

    moveReadIndexToNextSector();
}
#endif

