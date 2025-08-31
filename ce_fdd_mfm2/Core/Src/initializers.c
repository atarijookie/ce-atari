#include "main.h"
#include "defs.h"
#include "circularbuffer.h"
#include "initializers.h"
#include "dma_handlers.h"

extern DMA_HandleTypeDef hdma_tim3_up;

void setupSpiUsingCircularDma(void)
{
    CLEAR_BIT(SPI1->CR1, SPI_CR1_SPE);          // SPI disable
    CLEAR_BIT(DMA1_Channel2->CCR, DMA_CCR_EN);  // DMA disable channel 1
    CLEAR_BIT(DMA1_Channel3->CCR, DMA_CCR_EN);  // DMA disable channel 2

    SET_BIT(SPI1->CR2, SPI_RXFIFO_THRESHOLD);   // Set RX FIFO threshold according the reception data length: 8bit

    //----
    // DMA1 channel 2 - SPI RX
    DMA1->IFCR = DMA_FLAG_GI2;                  // Clear all flags

    DMA1_Channel2->CPAR = (uint32_t) &(SPI1->DR);   // peripheral address: SPI DR
    DMA1_Channel2->CMAR = (uint32_t) rxData;        // memory address: rxData
    DMA1_Channel2->CNDTR = BFR_SIZE;                // Configure DMA Channel data length
    SET_BIT(DMA1_Channel2->CCR, (DMA_CCR_PL_1 | DMA_CCR_PL_0));         // channel 1 priority - very high (3)
    SET_BIT(DMA1_Channel2->CCR, (DMA_IT_TC | DMA_IT_HT | DMA_IT_TE));   // enable interrupts for half-transfer and transfer complete
    SET_BIT(DMA1_Channel2->CCR, (DMA_CCR_MINC | DMA_CCR_CIRC));         // enable memory increment, circular mode
    CLEAR_BIT(DMA1_Channel2->CCR, (DMA_CCR_PINC | DMA_CCR_DIR));        // disable peripheral increment, direction: read from peripheral

    //----
    // DMA1 channel 3 - SPI TX
    DMA1->IFCR = DMA_FLAG_GI3;                  // Clear all flags

    DMA1_Channel3->CPAR = (uint32_t) &(SPI1->DR);   // peripheral address: SPI DR
    DMA1_Channel3->CMAR = (uint32_t) wrBuffer[0].data;  // memory address: tx buffer
    DMA1_Channel3->CNDTR = WRITEBUFFER_SIZE;        // Configure DMA Channel data length
    SET_BIT(DMA1_Channel3->CCR, DMA_CCR_PL_1); CLEAR_BIT(DMA1_Channel3->CCR, DMA_CCR_PL_0); // channel 2 priority - high (2)
    SET_BIT(DMA1_Channel3->CCR, (DMA_IT_TC | DMA_IT_HT | DMA_IT_TE));   // enable interrupts for half-transfer and transfer complete
    SET_BIT(DMA1_Channel3->CCR, (DMA_CCR_MINC | DMA_CCR_DIR));      // enable memory increment, direction: read from memory
    CLEAR_BIT(DMA1_Channel3->CCR, DMA_CCR_PINC | DMA_CCR_CIRC);     // disable peripheral increment, normal (linear) mode

    //----
    SET_BIT(DMA1_Channel2->CCR, DMA_CCR_EN);    // DMA enable channel 2
    SET_BIT(DMA1_Channel3->CCR, DMA_CCR_EN);    // DMA enable channel 3

    SET_BIT(SPI1->CR1, SPI_CR1_SPE);            // SPI enable
    SET_BIT(SPI1->CR2, SPI_CR2_RXDMAEN);        // enable RX DMA on SPI
    SET_BIT(SPI1->CR2, SPI_CR2_TXDMAEN);        // enable TX DMA on SPI
}

void setupTMIcircularDma(DMA_Channel_TypeDef* dmaChannel, TIM_TypeDef* timer, uint16_t* pBfr, int bfrSize, int isOutput)
{
    for(int i=0; i<bfrSize; i++) {
        pBfr[i] = 7;     // by default -- all pulses 4 us
    }

    CLEAR_BIT(dmaChannel->CCR, DMA_CCR_EN);  // DMA disable channel

    //----
    dmaChannel->CPAR = (uint32_t) &(timer->DMAR);       // peripheral address
    dmaChannel->CMAR = (uint32_t) pBfr;                 // memory address
    dmaChannel->CNDTR = bfrSize;                        // Configure DMA Channel data length

    CLEAR_BIT(dmaChannel->CCR, DMA_CCR_PL_1);

    if(isOutput) {      // output - channel priority - 00 - low
        CLEAR_BIT(dmaChannel->CCR, DMA_CCR_PL_0);
    } else {            // input  - channel priority - 01 - high
        SET_BIT(dmaChannel->CCR, DMA_CCR_PL_0);     // channel priority
    }

    SET_BIT(dmaChannel->CCR, (DMA_IT_TC | DMA_IT_HT | DMA_IT_TE));   // enable interrupts for half-transfer and transfer complete
    SET_BIT(dmaChannel->CCR, (DMA_CCR_MINC | DMA_CCR_CIRC));         // enable memory increment, circular mode

    if(isOutput) {
        SET_BIT(dmaChannel->CCR, DMA_CCR_DIR);      // direction: read from memory
    } else {
        CLEAR_BIT(dmaChannel->CCR, DMA_CCR_DIR);    // direction: read from peripheral
    }

    CLEAR_BIT(dmaChannel->CCR, (DMA_CCR_PINC));        // disable peripheral increment
    CLEAR_BIT(dmaChannel->CCR, DMA_CCR_PSIZE_1); SET_BIT(dmaChannel->CCR, DMA_CCR_PSIZE_0); // peripheral transfer size: 16 bits (1)
    CLEAR_BIT(dmaChannel->CCR, DMA_CCR_MSIZE_1); SET_BIT(dmaChannel->CCR, DMA_CCR_MSIZE_0); // memory transfer size: 16 bits (1)

    if(isOutput) {
        // set TIM DMA control register (TIMx_DCR) as: DBL (<<8) =0 (1 transfer), DBA (<<0) =11 (0x2C TIMx_ARR)
        timer->DCR = 11;
        SET_BIT(timer->DIER, TIM_DIER_UDE);
    } else {
        // set TIM DMA control register (TIMx_DCR) as: DBL (<<8) =0 (1 transfer), DBA (<<0) =13 (0x34 TIMx_CCR1)
        timer->DCR = 13;
        SET_BIT(timer->DIER, TIM_DIER_CC1DE);
    }

    SET_BIT(dmaChannel->CCR, DMA_CCR_EN);    // DMA enable channel
}

// Reconfigure DMA channel, so that it reads from memory into SPI TX - for sending buffer out
void spiDmaTxBuffer(uint32_t pData, uint32_t count)
{
    CLEAR_BIT(SPI1->CR2, SPI_CR2_TXDMAEN);                  // disable TX DMA on SPI
    CLEAR_BIT(DMA1_Channel3->CCR, DMA_CCR_EN);              // DMA disable channel
    DMA1->IFCR = DMA_FLAG_GI2;                              // DMA channel 2 - clear interrupt flags

    CLEAR_BIT(DMA1_Channel3->CCR, DMA_CCR_CIRC);            // linear mode (disable circular mode)
    DMA1_Channel3->CMAR = pData;                            // memory address: the supplied buffer
    DMA1_Channel3->CNDTR = count;                           // length: the size of data in buffer

    SET_BIT(DMA1_Channel3->CCR, DMA_IT_TC);                 // enable interrupts for transfer complete
    CLEAR_BIT(DMA1_Channel3->CCR, DMA_IT_HT);               // disable interrupts for half-transfer

    SET_BIT(DMA1_Channel3->CCR, DMA_CCR_EN);                // DMA enable channel
    SET_BIT(SPI1->CR2, SPI_CR2_TXDMAEN);                    // enable TX DMA on SPI
}
