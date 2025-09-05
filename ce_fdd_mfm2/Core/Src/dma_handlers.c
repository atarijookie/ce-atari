#include "main.h"
#include "defs.h"
#include "circularbuffer.h"
#include "dma_handlers.h"
#include "initializers.h"


/*
 * There can he up to 8 us delay between setting a flag in the interrupt
 * and actually handling it in main.
 *
 * Read is reliable when refilled in interrupt, not reliable when filled in main.
 */

uint16_t mfmReadStreamBuffer[MFM_READ_SIZE];
uint16_t mfmWriteStreamBuffer[MFM_WRITE_SIZE];

volatile uint8_t circHandleWhat = CIRC_HANDLE_NOTHING;

const uint16_t arrValues[4] = {7, 7, 11, 15};       // conversion table from mfm packed symbol to timer ARR value (for 0 us, 4 us, 6 us, 8 us)

__attribute__((always_inline)) void fillFourReadTimes(uint16_t* bfr)
{
    uint8_t streamByte = 0;

    if(rxCnt > 0) {             // got something in RX buffer?
        streamByte = RX_GET();
        UPDATE_PIN_RXE;         // after removing byte from RX buffer, update RXE flag
    } else {                    // RX buffer empty?
        streamByte = 0x55;
    }

    bfr[0] = arrValues[ ((streamByte >> 6) & 3) ];
    bfr[1] = arrValues[ ((streamByte >> 4) & 3) ];
    bfr[2] = arrValues[ ((streamByte >> 2) & 3) ];
    bfr[3] = arrValues[ ((streamByte     ) & 3) ];
}

__attribute__((always_inline)) void updateReadTimerDma(uint8_t whichPart)
{
    uint16_t* bfr = (whichPart == CIRC_HANDLE_LOW) ? &mfmReadStreamBuffer[0] : &mfmReadStreamBuffer[MFM_READ_SIZE_HALF];

    for(int i=0; i<MFM_READ_SIZE_FILLS; i++) {
        fillFourReadTimes(bfr);
        bfr += 4;
    }
}

uint8_t wrStreamByte = 0;
uint8_t wrBits = 0;

volatile uint16_t wrPrevCapturedStamp = 0;

//uint16_t diffs[PULSE_8US];                      // TODO: remove

/*
 * If you use always_inline, you get a compiler warning, but the test shows the differences:
 * - 3.2 us when not inline
 * - 2.6 us when always_inline
 */
__attribute__((always_inline)) void updateWriteDataDirect(uint16_t capturedStamp)
{
    uint16_t capturedDuration = capturedStamp - wrPrevCapturedStamp;  // calculate the change from previous captured value
    wrPrevCapturedStamp = capturedStamp;                      // store the current captured time

    uint8_t newTime = 0;

//    if(capturedDuration < PULSE_8US) {          // TODO: remove
//        diffs[capturedDuration]++;
//    }

    if(capturedDuration < PULSE_TOO_SHORT) {    // if this pulse is too short (less than 2.7 us long)
        return;
    } else if(capturedDuration < PULSE_4US) {   // 4 us? (interval 2.7 us - 5.0 us) (40 pulses of 0.125 us)
        newTime = MFM_4US;
    } else if(capturedDuration < PULSE_6US) {   // 6 us? (interval 5.0 us - 7.0 us) (56 pulses of 0.125 us)
        newTime = MFM_6US;
    } else if(capturedDuration < PULSE_8US) {   // 8 us? (interval 7.0 us - 9.0 us) (72 pulses of 0.125 us)
        newTime = MFM_8US;
    } else {                            // pulse too long? (longer than 9 us)
        return;
    }

    wrStreamByte = (wrStreamByte << 2) | newTime;   // append new time to streamByte
    wrBits += 2;            // now got 2 more bits

    if(wrBits >= 8) {       // got 8 bits?
        wrBits = 0;           // don't have bits now

        // tx buffer not full? add streamByte to tx buffer
        if(wrStore->count < WRITEBUFFER_SIZE) {
            wrStore->data[wrStore->count] = wrStreamByte;
            wrStore->count++;
        }
    }
}

// With the check of registers in the interrupt, this takes:
// - 3.2 us when updateWriteDataDirect() is without inline, or...
// - 2.7 us when updateWriteDataDirect() is always_inline
__attribute__((always_inline)) void processWriteTimerDma(uint8_t whichPart)
{
    uint16_t* bfr = (whichPart == CIRC_HANDLE_LOW) ? &mfmWriteStreamBuffer[0] : &mfmWriteStreamBuffer[MFM_WRITE_SIZE_HALF];

    for(int i=0; i<MFM_WRITE_SIZE_HALF; i++) {
        updateWriteDataDirect(bfr[i]);
    }
}

/*
 * DMA channel1 - MFM READ timer16 refill
 */
void DMA1_Channel1_IRQHandler(void)
{
    uint32_t flag_it = DMA1->ISR;

    // Half Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_HT1) != 0)
    {
       DMA1->IFCR = DMA_FLAG_HT1;
       updateReadTimerDma(CIRC_HANDLE_LOW);
    }

    // Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_TC1) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TC1;
        updateReadTimerDma(CIRC_HANDLE_HIGH);
    }

    // Transfer Error Interrupt management
    if((flag_it & DMA_FLAG_TE1) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TE1;
    }
}

/*
 * DMA channel 2 - SPI RX
 * DMA channel 3 - SPI TX
 */
void DMA1_Channel2_3_IRQHandler(void)
{
    uint32_t flag_it = DMA1->ISR;

    // DMA channel 2 - SPI RX
    // Half Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_HT2) != 0U)
    {
       DMA1->IFCR = DMA_FLAG_HT2;   // clear flag
       rxCnt += BFR_SIZE_HALF;      // got half buffer of data now
       UPDATE_PIN_RXE;
    }

    // Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_TC2) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TC2;  // clear flag
        rxCnt += BFR_SIZE_HALF;     // got half buffer of data now
        UPDATE_PIN_RXE;
    }

    // Transfer Error Interrupt management
    if((flag_it & DMA_FLAG_TE2) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TE2;
    }

    //=======
    // DMA channel 3 - SPI TX
    // Half Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_HT3) != 0U)
    {
        DMA1->IFCR = DMA_FLAG_HT3;
    }

    // Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_TC3) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TC3;
        spiIsSending = 0;           // finished sending, so not sending anymore
    }

    // Transfer Error Interrupt management
    if((flag_it & DMA_FLAG_TE3) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TE3;
    }
}

