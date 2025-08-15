#include "main.h"
#include "circularbuffer.h"
#include "dma_handlers.h"

void updateWriteDataDirect(uint16_t capturedStamp);

uint8_t* pWrite;

#define MFM_READ_SIZE   8
#define MFM_WRITE_SIZE   8
uint16_t mfmReadStreamBuffer[MFM_READ_SIZE];
uint16_t mfmWriteStreamBuffer[MFM_WRITE_SIZE];

const uint16_t arrValues[4] = {7, 7, 11, 15};       // conversion table from mfm packed symbol to timer ARR value (for 0 us, 4 us, 6 us, 8 us)

void updateReadTimerDma(uint8_t lowerNotUpper)
{
    uint8_t streamByte = 0;

    if(rxCnt > 0) {             // got something in RX buffer?
        streamByte = RX_GET();
        UPDATE_PIN_RXE;         // after removing byte from RX buffer, update RXE flag
    } else {                    // RX buffer empty?
        streamByte = 0x55;
    }

    uint16_t* bfr = lowerNotUpper ? &mfmReadStreamBuffer[0] : &mfmReadStreamBuffer[MFM_READ_SIZE/2];

    bfr[0] = arrValues[ ((streamByte >> 6) & 3) ];
    bfr[1] = arrValues[ ((streamByte >> 4) & 3) ];
    bfr[2] = arrValues[ ((streamByte >> 2) & 3) ];
    bfr[3] = arrValues[ ((streamByte     ) & 3) ];
}

void processWriteTimerDma(uint8_t lowerNotUpper)
{
    uint16_t* bfr = lowerNotUpper ? &mfmWriteStreamBuffer[0] : &mfmWriteStreamBuffer[MFM_WRITE_SIZE/2];

    updateWriteDataDirect(bfr[0]);
    updateWriteDataDirect(bfr[1]);
    updateWriteDataDirect(bfr[2]);
    updateWriteDataDirect(bfr[3]);
}

uint8_t wrStreamByte = 0;
uint8_t wrBits = 0;

volatile uint16_t wrPrevCapturedStamp = 0;
extern volatile uint8_t writingNow;

/* Takes 2.61 us when also storing byte to circular buffer,
 * takes 1.81 us when just storing bits, not adding to circular buffer.
 * Can happen in 4 us intervals (min), but up to 6 us or 8 us also.
 *
 * If you use always_inline, you get a compiler warning, but in the Build Analyzer
 * you can see the difference:
 * - no inline - processWriteTimerDma  44
 *             - updateWriteDataDirect 184
 *             - total:   44 + 4*184 = 780
 * - inline    - processWriteTimerDma  664
 * This looks like 15% less of overall code executed, could help.
 */
__attribute__((always_inline)) void updateWriteDataDirect(uint16_t capturedStamp)
{
    uint16_t capturedDuration = capturedStamp - wrPrevCapturedStamp;  // calculate the change from previous captured value
    wrPrevCapturedStamp = capturedStamp;                      // store the current captured time

    uint8_t newTime = 0;

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
        if(txCnt < WRITEBUFFER_SIZE) {
            *pWrite = wrStreamByte;
            pWrite++;
            txCnt++;
        }

        // during the write we don't actually stream the data out, but
        // let's remove a RX byte at the speed the data is added to TX
        // to keep the SPI transfer going when needed
        if(rxCnt > 0) {             // got something in RX buffer?
            RX_DROP();
            UPDATE_PIN_RXE;         // after removing byte from RX buffer, update RXE flag
        }
    }
}

// can happen up to every 256 us, or longer
// Takes 1.1 us
void DMA1_Channel1_IRQHandler(void)
{
    uint32_t flag_it = DMA1->ISR;

    // DMA channel 1
    // Half Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_HT1) != 0U)
    {
       DMA1->IFCR = DMA_FLAG_HT1;   // clear flag
       rxCnt += BFR_SIZE_HALF;      // got half buffer of data now
       UPDATE_PIN_RXE;
    }

    // Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_TC1) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TC1;  // clear flag
        rxCnt += BFR_SIZE_HALF;     // got half buffer of data now
        UPDATE_PIN_RXE;
    }

    // Transfer Error Interrupt management
    if((flag_it & DMA_FLAG_TE1) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TE1;
    }
}

volatile uint8_t bfrStateLow, bfrStateHigh;

/*
 * Note: there's still a race-condition hazard, if:
 * - interrupt doesn't see the half of buffer ready to be sent, but it already contains tags
 * - main loop marks the buffer as waiting for send
 * - spi + dma will send this buffer, esp32 will receive it
 * - but upon half / full transfer complete this buffer is not marked as already sent
 * - this buffer gets sent again (so twice the same sector)
 */

// can happen up to every 1.3 ms for SPI, takes 0.5 us
// can happen up to every 16 us for TIM3, takes 3.2 us
void DMA1_Channel2_3_IRQHandler(void)
{
    uint32_t flag_it = DMA1->ISR;

    //-------------
    // DMA channel 2
    // Half Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_HT2) != 0U)
    {
       DMA1->IFCR = DMA_FLAG_HT2;   // clear flag

       // If the high part was waiting to be sent, now it's being sent.
       if(bfrStateHigh == STATE_WAIT_FOR_SEND) {
           bfrStateHigh = STATE_SENDING;
       }

       // If the low part was sending, now it's sent.
       // Mark lower part as sent/empty - first and last bytes are zeros now.
       if(bfrStateLow == STATE_SENDING) {
           bfrStateLow = STATE_EMPTY;
           txData[0] = 0;
           txData[WRITEBUFFER_SIZE - 1] = 0;
       }
    }

    // Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_TC2) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TC2;  // clear flag

        // If the low part was waiting to be sent, now it's being sent.
        if(bfrStateLow == STATE_WAIT_FOR_SEND) {
            bfrStateLow = STATE_SENDING;
        }

        // If the high part was sending, now it's sent.
        // Mark higher part as sent/empty - first and last bytes are zeros now.
        if(bfrStateHigh == STATE_SENDING) {
            bfrStateHigh = STATE_EMPTY;
            txData[WRITEBUFFER_SIZE] = 0;
            txData[TX_DATA_SIZE - 1] = 0;
        }
    }

    // Transfer Error Interrupt management
    if((flag_it & DMA_FLAG_TE2) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TE2;
    }

    //=======
    // DMA channel 3
    // Half Transfer Complete Interrupt management

    GPIOB->BSRR = (1 << 6);     // TODO: just for debug, remove later

    if((flag_it & DMA_FLAG_HT3) != 0U)
    {
       DMA1->IFCR = DMA_FLAG_HT3;   // clear flag

       if(writingNow)       // if we're WRITING - process write buffer
           processWriteTimerDma(1);
       else                // if we're READING - refill read buffer
           updateReadTimerDma(1);
    }

    // Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_TC3) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TC3;  // clear flag

        if(writingNow)      // if we're WRITING - process write buffer
            processWriteTimerDma(0);
        else                // if we're READING - refill read buffer
            updateReadTimerDma(0);
    }

    // Transfer Error Interrupt management
    if((flag_it & DMA_FLAG_TE3) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TE3;
    }

    GPIOB->BRR = (1 << 6);     // TODO: just for debug, remove later
}
