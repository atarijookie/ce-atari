#include "main.h"
#include "circularbuffer.h"
#include "initializers.h"
#include "dma_handlers.h"

extern DMA_HandleTypeDef hdma_tim3_up;

void setupSpiUsingCircularDma(void)
{
    CLEAR_BIT(SPI1->CR1, SPI_CR1_SPE);          // SPI disable
    CLEAR_BIT(DMA1_Channel1->CCR, DMA_CCR_EN);  // DMA disable channel 1
    CLEAR_BIT(DMA1_Channel2->CCR, DMA_CCR_EN);  // DMA disable channel 2

    SET_BIT(SPI1->CR2, SPI_RXFIFO_THRESHOLD);   // Set RX FIFO threshold according the reception data length: 8bit

    DMAMUX1_ChannelStatus->CFR = 0x1f;          // Clear the DMAMUX synchro overrun flag
    DMAMUX1_RequestGenStatus->RGCFR = 0x0f;     // Clear the DMAMUX request generator overrun flag

    //----
    // DMA1 channel 1 - SPI RX
    DMA1->IFCR = DMA_FLAG_GI1;                  // Clear all flags

    DMA1_Channel1->CPAR = (uint32_t) &(SPI1->DR);   // peripheral address: SPI DR
    DMA1_Channel1->CMAR = (uint32_t) rxData;        // memory address: rxData
    DMA1_Channel1->CNDTR = BFR_SIZE;                // Configure DMA Channel data length
    SET_BIT(DMA1_Channel1->CCR, (DMA_CCR_PL_1 | DMA_CCR_PL_0));         // channel 1 priority - very high (3)
    SET_BIT(DMA1_Channel1->CCR, (DMA_IT_TC | DMA_IT_HT | DMA_IT_TE));   // enable interrupts for half-transfer and transfer complete
    SET_BIT(DMA1_Channel1->CCR, (DMA_CCR_MINC | DMA_CCR_CIRC));         // enable memory increment, circular mode
    CLEAR_BIT(DMA1_Channel1->CCR, (DMA_CCR_PINC | DMA_CCR_DIR));        // disable peripheral increment, direction: read from peripheral

    //----
    // DMA1 channel 2 - SPI TX
    DMA1->IFCR = DMA_FLAG_GI2;                  // Clear all flags

    DMA1_Channel2->CPAR = (uint32_t) &(SPI1->DR);   // peripheral address: SPI DR
    DMA1_Channel2->CMAR = (uint32_t) txData;        // memory address: tx buffer
    DMA1_Channel2->CNDTR = TX_DATA_SIZE;            // Configure DMA Channel data length
    SET_BIT(DMA1_Channel2->CCR, DMA_CCR_PL_1); CLEAR_BIT(DMA1_Channel2->CCR, DMA_CCR_PL_0); // channel 2 priority - high (2)
    SET_BIT(DMA1_Channel2->CCR, (DMA_IT_TC | DMA_IT_HT | DMA_IT_TE));   // enable interrupts for half-transfer and transfer complete
    SET_BIT(DMA1_Channel2->CCR, (DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_DIR));   // enable memory increment, circular mode, direction: read from memory
    CLEAR_BIT(DMA1_Channel2->CCR, DMA_CCR_PINC);        // disable peripheral increment

    //----
    SET_BIT(DMA1_Channel1->CCR, DMA_CCR_EN);    // DMA enable channel 1
    SET_BIT(DMA1_Channel2->CCR, DMA_CCR_EN);    // DMA enable channel 2

    SET_BIT(SPI1->CR1, SPI_CR1_SPE);            // SPI enable
    SET_BIT(SPI1->CR2, SPI_CR2_RXDMAEN);        // enable RX DMA on SPI
    SET_BIT(SPI1->CR2, SPI_CR2_TXDMAEN);        // enable TX DMA on SPI
}

void setupTMI3circularDma(void)
{
    for(int i=0; i<MFM_READ_SIZE; i++) {
        mfmReadStreamBuffer[i] = 7;     // by default -- all pulses 4 us
    }

    CLEAR_BIT(DMA1_Channel3->CCR, DMA_CCR_EN);  // DMA disable channel

    //----

    DMA1_Channel3->CPAR = (uint32_t) &(TIM3->DMAR);         // peripheral address
    DMA1_Channel3->CMAR = (uint32_t) mfmReadStreamBuffer;   // memory address: mfmReadStreamBuffer
    DMA1_Channel3->CNDTR = MFM_READ_SIZE;                   // Configure DMA Channel data length
    CLEAR_BIT(DMA1_Channel3->CCR, DMA_CCR_PL_1); SET_BIT(DMA1_Channel2->CCR, DMA_CCR_PL_0); // channel 3 priority - low (1)
    SET_BIT(DMA1_Channel3->CCR, (DMA_IT_TC | DMA_IT_HT | DMA_IT_TE));   // enable interrupts for half-transfer and transfer complete
    SET_BIT(DMA1_Channel3->CCR, (DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_DIR));         // enable memory increment, circular mode, direction: read from memory
    CLEAR_BIT(DMA1_Channel3->CCR, (DMA_CCR_PINC));        // disable peripheral increment
    CLEAR_BIT(DMA1_Channel3->CCR, DMA_CCR_PSIZE_1); SET_BIT(DMA1_Channel2->CCR, DMA_CCR_PSIZE_0); // peripheral transfer size: 16 bits (1)
    CLEAR_BIT(DMA1_Channel3->CCR, DMA_CCR_MSIZE_1); SET_BIT(DMA1_Channel2->CCR, DMA_CCR_MSIZE_0); // memory transfer size: 16 bits (1)

    //----
    SET_BIT(DMA1_Channel3->CCR, DMA_CCR_EN);    // DMA enable channel

    // set TIM3 DMA control register (TIMx_DCR) as: DBL (<<8) =0 (1 transfer), DBA (<<0) =11 (0x2C TIMx_ARR)
    TIM3->DCR = 11;
    SET_BIT(TIM3->DIER, TIM_DIER_UDE);

    // set TIM16 DMA control register (TIMx_DCR) as: DBL (<<8) =0 (1 transfer), DBA (<<0) =6 (0x18 TIMx_CCMR1)
    TIM16->DCR = 6;
}

void dmaReconfigForRead(void)
{
    CLEAR_BIT(TIM16->DIER, TIM_DIER_CC1DE);                 // TIM16 DMA request disable
    CLEAR_BIT(DMA1_Channel3->CCR, DMA_CCR_EN);              // DMA disable channel

    for(int i=0; i<MFM_READ_SIZE; i++) {
        mfmReadStreamBuffer[i] = 7;                         // by default -- all pulses 4 us
    }

    DMA1_Channel3->CPAR = (uint32_t) &(TIM3->DMAR);         // peripheral address
    DMA1_Channel3->CMAR = (uint32_t) mfmReadStreamBuffer;   // memory address: mfmReadStreamBuffer
    DMA1_Channel3->CNDTR = MFM_READ_SIZE;                   // Configure DMA Channel data length
    SET_BIT(DMA1_Channel3->CCR, DMA_CCR_DIR);               // direction: read from memory

    uint32_t ccr = hdma_tim3_up.DMAmuxChannel->CCR & (~0x7f);  // get original value, remove request part
    hdma_tim3_up.DMAmuxChannel->CCR = (ccr | 37);          // DMA request source: tim3_up_dma

    SET_BIT(DMA1_Channel3->CCR, DMA_CCR_EN);                // DMA enable channel
    SET_BIT(TIM3->DIER, TIM_DIER_UDE);                      // enable timer update DMA
}

void dmaReconfigForWrite(void)
{
    CLEAR_BIT(TIM3->DIER, TIM_DIER_UDE);                    // TIM3 DMA request disable
    CLEAR_BIT(DMA1_Channel3->CCR, DMA_CCR_EN);              // DMA disable channel

    for(int i=0; i<MFM_READ_SIZE; i++) {
        mfmReadStreamBuffer[i] = 7;                         // by default -- all pulses 4 us
    }

    DMA1_Channel3->CPAR = (uint32_t) &(TIM16->DMAR);        // peripheral address
    DMA1_Channel3->CMAR = (uint32_t) mfmWriteStreamBuffer;  // memory address: mfmReadStreamBuffer
    DMA1_Channel3->CNDTR = MFM_WRITE_SIZE;                  // Configure DMA Channel data length
    CLEAR_BIT(DMA1_Channel3->CCR, DMA_CCR_DIR);             // direction: read from peripheral

    uint32_t ccr = hdma_tim3_up.DMAmuxChannel->CCR & (~0x7f);  // get original value, remove request part
    hdma_tim3_up.DMAmuxChannel->CCR = (ccr | 44);          // DMA request source: tim16_ch1_dma

    SET_BIT(DMA1_Channel3->CCR, DMA_CCR_EN);                // DMA enable channel
    SET_BIT(TIM16->DIER, TIM_DIER_CC1DE);                   // TIM16 DMA request enable
}
