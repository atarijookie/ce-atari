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

extern volatile BYTE spiDmaIsIdle;
extern volatile BYTE spiDmaTXidle, spiDmaRXidle;       // flags set when the SPI DMA TX or RX is idle

extern BYTE hostIsUp;                                  // used to just pass through IKBD until RPi is up
extern TOutputFlags outFlags;

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
