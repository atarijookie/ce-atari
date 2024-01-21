#include "stm32f10x.h"                       // STM32F103 definitions
#include "stm32f10x_spi.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_usart.h"
#include "misc.h"

#include "defs.h"
#include "main.h"
#include "init.h"
#include "floppyhelpers.h"
#include "timers.h"
#include "global_vars.h"

SPI_InitTypeDef spiStruct;

void spi_fillInitStruct(void)
{
    SPI_StructInit(&spiStruct);
    spiStruct.SPI_DataSize = SPI_DataSize_16b;                                      // use 16b data size to lower the MCU load
}

void spi_init(void)     // init SPI -- you should call spi_fillInitStruct() once before this
{
    SPI_Cmd(SPI1, DISABLE);
    
    SPI_Init(SPI1, &spiStruct);
    SPI1->CR2 |= (1 << 7) | (1 << 6) | SPI_I2S_DMAReq_Tx | SPI_I2S_DMAReq_Rx;       // enable TXEIE, RXNEIE, TXDMAEN, RXDMAEN
    
    SPI_Cmd(SPI1, ENABLE);
}

void dma_mfmRead_init(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    
    // DMA1 channel5 configuration -- TIM1_UP (update)
    DMA_DeInit(DMA1_Channel5);
    
    DMA_InitStructure.DMA_MemoryBaseAddr      = (uint32_t) mfmReadStreamBuffer;     // from this buffer located in memory
    DMA_InitStructure.DMA_PeripheralBaseAddr  = (uint32_t) &(TIM1->DMAR);           // to this peripheral address
    DMA_InitStructure.DMA_DIR                 = DMA_DIR_PeripheralDST;              // dir: from mem to periph
    DMA_InitStructure.DMA_BufferSize          = 16;                                 // 16 datas to transfer
    DMA_InitStructure.DMA_PeripheralInc       = DMA_PeripheralInc_Disable;          // PINC = 0 -- don't icrement, always write to DMAR register
    DMA_InitStructure.DMA_MemoryInc           = DMA_MemoryInc_Enable;               // MINC = 1 -- increment in memory -- go though buffer
    DMA_InitStructure.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_HalfWord;    // each data item: 16 bits 
    DMA_InitStructure.DMA_MemoryDataSize      = DMA_MemoryDataSize_HalfWord;        // each data item: 16 bits 
    DMA_InitStructure.DMA_Mode                = DMA_Mode_Circular;                  // circular mode
    DMA_InitStructure.DMA_Priority            = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M                 = DMA_M2M_Disable;                    // M2M disabled, because we move to peripheral
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);

    // Enable DMA1 Channel5 Transfer Complete interrupt
    DMA_ITConfig(DMA1_Channel5, DMA_IT_HT, ENABLE);                                 // interrupt on Half Transfer (HT)
    DMA_ITConfig(DMA1_Channel5, DMA_IT_TC, ENABLE);                                 // interrupt on Transfer Complete (TC)

    // Enable DMA1 Channel5 transfer
    DMA_Cmd(DMA1_Channel5, ENABLE);
}

// init DMA capturing for HW WRITE
#ifndef SW_WRITE
void dma_mfmWrite_init(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    
    // DMA1 channel6 configuration -- TIM3_CH1 (capture)
    DMA_DeInit(DMA1_Channel6);
    
    DMA_InitStructure.DMA_PeripheralBaseAddr  = (uint32_t) &(TIM3->DMAR);           // from this peripheral address
    DMA_InitStructure.DMA_MemoryBaseAddr      = (uint32_t) mfmWriteStreamBuffer;    // to this buffer located in memory
    DMA_InitStructure.DMA_DIR                 = DMA_DIR_PeripheralSRC;              // dir: from periph to mem
    DMA_InitStructure.DMA_BufferSize          = MFM_WRITE_STREAM_SIZE;              // size of whole buffer
    DMA_InitStructure.DMA_PeripheralInc       = DMA_PeripheralInc_Disable;          // PINC = 0 -- don't icrement, always read from DMAR register
    DMA_InitStructure.DMA_MemoryInc           = DMA_MemoryInc_Enable;               // MINC = 1 -- increment in memory -- go though buffer
    DMA_InitStructure.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_HalfWord;    // each data item: 16 bits 
    DMA_InitStructure.DMA_MemoryDataSize      = DMA_MemoryDataSize_HalfWord;        // each data item: 16 bits 
    DMA_InitStructure.DMA_Mode                = DMA_Mode_Circular;                  // circular mode
    DMA_InitStructure.DMA_Priority            = DMA_Priority_Medium;
    DMA_InitStructure.DMA_M2M                 = DMA_M2M_Disable;                    // M2M disabled, because we move to peripheral
    DMA_Init(DMA1_Channel6, &DMA_InitStructure);

    // Enable DMA1 Channel6 Transfer Complete interrupt
    DMA_ITConfig(DMA1_Channel6, DMA_IT_HT, ENABLE);                                 // interrupt on Half Transfer (HT)
    DMA_ITConfig(DMA1_Channel6, DMA_IT_TC, ENABLE);                                 // interrupt on Transfer Complete (TC)

    // Enable DMA1 Channel6 transfer
    //DMA_Cmd(DMA1_Channel6, ENABLE);
}
#endif

void dma_spi_init(void)
{
    DMA_InitTypeDef     DMA_InitStructure;
    NVIC_InitTypeDef    NVIC_InitStructure;
    
    // DMA1 channel3 configuration -- SPI1 TX
    DMA_DeInit(DMA1_Channel3);
    
    DMA_InitStructure.DMA_MemoryBaseAddr      = (uint32_t) 0;                       // from this buffer located in memory (now only fake)
    DMA_InitStructure.DMA_PeripheralBaseAddr  = (uint32_t) &(SPI1->DR);             // to this peripheral address
    DMA_InitStructure.DMA_DIR                 = DMA_DIR_PeripheralDST;              // dir: from mem to periph
    DMA_InitStructure.DMA_BufferSize          = 0;                                  // fake buffer size
    DMA_InitStructure.DMA_PeripheralInc       = DMA_PeripheralInc_Disable;          // PINC = 0 -- don't icrement, always write to SPI1->DR register
    DMA_InitStructure.DMA_MemoryInc           = DMA_MemoryInc_Enable;               // MINC = 1 -- increment in memory -- go though buffer
    DMA_InitStructure.DMA_PeripheralDataSize  = DMA_PeripheralDataSize_HalfWord;    // each data item: 16 bits 
    DMA_InitStructure.DMA_MemoryDataSize      = DMA_MemoryDataSize_HalfWord;        // each data item: 16 bits 
    DMA_InitStructure.DMA_Mode                = DMA_Mode_Normal;                    // normal mode
    DMA_InitStructure.DMA_Priority            = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M                 = DMA_M2M_Disable;                    // M2M disabled, because we move to peripheral
    DMA_Init(DMA1_Channel3, &DMA_InitStructure);

    // Enable interrupts
    DMA_ITConfig(DMA1_Channel3, DMA_IT_TC, ENABLE);                                 // interrupt on Transfer Complete (TC)

    //----------------
    // DMA1 channel2 configuration -- SPI1 RX
    DMA_DeInit(DMA1_Channel2);
    
    DMA_InitStructure.DMA_PeripheralBaseAddr    = (uint32_t) &(SPI1->DR);               // from this peripheral address
    DMA_InitStructure.DMA_MemoryBaseAddr        = (uint32_t) 0;                         // to this buffer located in memory (now only fake)
    DMA_InitStructure.DMA_DIR                   = DMA_DIR_PeripheralSRC;                // dir: from periph to mem
    DMA_InitStructure.DMA_BufferSize            = 0;                                    // fake buffer size
    DMA_InitStructure.DMA_PeripheralInc         = DMA_PeripheralInc_Disable;            // PINC = 0 -- don't icrement, always write to SPI1->DR register
    DMA_InitStructure.DMA_MemoryInc             = DMA_MemoryInc_Enable;                 // MINC = 1 -- increment in memory -- go though buffer
    DMA_InitStructure.DMA_PeripheralDataSize    = DMA_PeripheralDataSize_HalfWord;      // each data item: 16 bits 
    DMA_InitStructure.DMA_MemoryDataSize        = DMA_MemoryDataSize_HalfWord;          // each data item: 16 bits 
    DMA_InitStructure.DMA_Mode                  = DMA_Mode_Normal;                      // normal mode
    DMA_InitStructure.DMA_Priority              = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M                   = DMA_M2M_Disable;                      // M2M disabled, because we move from peripheral
    DMA_Init(DMA1_Channel2, &DMA_InitStructure);

    // Enable DMA1 Channel2 Transfer Complete interrupt
    DMA_ITConfig(DMA1_Channel2, DMA_IT_TC, ENABLE);                                     // interrupt on Transfer Complete (TC)

    //----------------
    // now enable interrupt on DMA1_Channel3
    NVIC_InitStructure.NVIC_IRQChannel                      = DMA1_Channel3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority    = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority           = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelCmd                   = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // and also interrupt on DMA1_Channel2
    NVIC_InitStructure.NVIC_IRQChannel                      = DMA1_Channel2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority    = 0x02;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority           = 0x02;
    NVIC_InitStructure.NVIC_IRQChannelCmd                   = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void setupAtnBuffers(void)
{
    WORD i;
    
    atnSendFwVersion[0] = ATN_SYNC_WORD;                    // starting mark
    atnSendFwVersion[1] = ATN_FW_VERSION;                   // attention code
    // WORDs 2 and 3 are reserved for TX LEN and RX LEN
    atnSendFwVersion[4] = version[0];
    atnSendFwVersion[5] = version[1];
    atnSendFwVersion[6] = 0;                                // not used, but in Hans it's used, so kept for length compatibility
    atnSendFwVersion[7] = 0;                                // terminating zero

    
    atnSendTrackRequest[0] = ATN_SYNC_WORD;                 // starting mark
    atnSendTrackRequest[1] = ATN_SEND_TRACK;                // attention code
    // WORDs 2 and 3 are reserved for TX LEN and RX LEN
    // WORD  4 contains side # and track #
    atnSendTrackRequest[5] = 0;                             // terminating zero
    
    
    // now init both writeBuffers
    for(i=0; i<WRITEBUFFER_SIZE; i++) {
        wrBuffer[0].buffer[i] = 0;
        wrBuffer[1].buffer[i] = 0;
    }
        
    wrBuffer[0].count       = 4;                            // at the start we already have 4 WORDs in buffer - SYNC, ATN code, TX len, RX len
    wrBuffer[0].readyToSend = FALSE;
    wrBuffer[0].next        = &wrBuffer[1];
    
    wrBuffer[0].buffer[0]   = ATN_SYNC_WORD;                // starting mark
    wrBuffer[0].buffer[1]   = ATN_SECTOR_WRITTEN;           // attention code
    // WORDs 2 and 3 are reserved for TX LEN and RX LEN
    // WORDs 4 and 5 contain side, track, sector #
    
    wrBuffer[1].count       = 4;                            // at the start we already have 4 WORDs in buffer - SYNC, ATN code, TX len, RX len
    wrBuffer[1].readyToSend = FALSE;
    wrBuffer[1].next        = &wrBuffer[0];

    wrBuffer[1].buffer[0]   = ATN_SYNC_WORD;                // starting mark
    wrBuffer[1].buffer[1]   = ATN_SECTOR_WRITTEN;           // attention code
    // WORDs 2 and 3 are reserved for TX LEN and RX LEN
    // WORDs 4 and 5 contain side, track, sector #

    wrNow = &wrBuffer[0];
}

void init_hw_sw(void)
{
    WORD i;
    NVIC_InitTypeDef NVIC_InitStructure;
    
    RCC->AHBENR     |= (1 <<  0);                                   // enable DMA1
    RCC->APB1ENR    |= (1 << 18) | (1 << 17) | (1 << 2) | (1 <<  1) | (1 <<  0);    // enable USART3, USART2, TIM4, TIM3, TIM2
    RCC->APB2ENR    |= (1 << 14) | (1 << 12) | (1 << 11) | (1 << 3) | (1 << 2);     // Enable USART1, SPI1, TIM1, GPIOA and GPIOB clock
    
    // set input / output for GPIOB0-7
    GPIOB->BRR = DEVICE_OFF_H;      // DEVICE_OFF_H pin must be LOW before we set it to output
    GPIOB->CRL = 0x84444833;        // 8 means input with pull-up, 4 means floating input, 3 means output push-pull
    GPIOB->BSRR = DIR | WGATE;      // set DIR, WGATE to 1 in ODR == pull up

    driveId         = 0;
    driveEnabled    = TRUE;
    
    isDiskChanged       = FALSE;
    isWriteProtected    = FALSE;
    
    FloppyOut_Disable();
    
    // SPI -- enable atlernate function for PA4, PA5, PA6, PA7, 
    // UART2 (to IKBD) -- alternate function for PA2, floating input for PA3
    GPIOA->CRL &= ~(0xffffff0f);                                    // remove bits from GPIOA
    GPIOA->CRL |=   0xbbbb4b04;

    // UART1 (to RPi) - alternate function for PA9, floating input for PA10
    GPIOA->CRH &= ~(0xf00f0ff0);                                    // remove bits from GPIOA
    GPIOA->CRH |=   0x300304b0;

    // ATTENTION -- set GPIOB_15 (ATTENTION) as --- CNF1:0 -- 00 (push-pull output), MODE1:0 -- 11 (output 50 Mhz)
    // UART3 (to original keyboard) -- floating input for PB11
    GPIOB->CRH &= ~(0xf000f000); 
    GPIOB->CRH |=  (0x30004000);

    RCC->APB2ENR |= (1 << 0);                                       // enable AFIO
    
    AFIO->EXTICR[0] = 0x1000;                                       // EXTI3 -- source input: GPIOB_3
    EXTI->IMR       = STEP;                                         // EXTI3 -- 1 means: Interrupt from line 3 not masked
    EXTI->EMR       = STEP;                                         // EXTI3 -- 1 means: Event     form line 3 not masked
    EXTI->FTSR      = STEP;                                         // Falling trigger selection register - STEP pulse
    
    /* Enable and set EXTI Line3 Interrupt to the lowest priority */
    
    NVIC_InitStructure.NVIC_IRQChannel                      = EXTI3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority    = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority           = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelCmd                   = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    //----------
    AFIO->MAPR |= 0x02000000;                                       // SWJ_CFG[2:0] (Bits 26:24) -- 010: JTAG-DP Disabled and SW-DP Enabled
    AFIO->MAPR |= 0x00000800;                                       // TIM3_REMAP -- Partial remap (CH1/PB4, CH2/PB5, CH3/PB0, CH4/PB1)
    //----------
    
    timerSetup_index();
    
    timerSetup_mfmWrite();

#ifndef SW_WRITE
    // init DMA for HW WRITE
    dma_mfmWrite_init();
#endif

    //--------------
    // DMA + SPI initialization
    for(i=0; i<READTRACKDATA_SIZE_WORDS; i++) {                           // fill the readTrackData with CMD_TRACK_STREAM_END_BYTE, which will tell that every byte is END OF STREAM (shouldn't read further)
        readTrackData[i] = CMD_TRACK_STREAM_END_BYTE;
    }
    
    spiDmaIsIdle = TRUE;
    spiDmaTXidle = TRUE;
    spiDmaRXidle = TRUE;
    
    spi_fillInitStruct();   // fill SPI init struct with wanted values
    spi_init();             // init SPI interface
    dma_spi_init();

    //--------------
    // configure MFM read stream by TIM1_CH1 and DMA in circular mode
    // WARNING!!! Never let mfmReadStreamBuffer[] contain a 0! With 0 the timer update never comes and streaming stops!
    for(i=0; i<16; i++) {
        mfmReadStreamBuffer[i] = 7;                                 // by default -- all pulses 4 us
    }
    
    timerSetup_mfmRead();                                           // init MFM READ stream timer
    dma_mfmRead_init();                                             // and init the DMA which will feed it from circular buffer
    //--------------
    
    timerSetup_stepLimiter();                                       // this 2 kHz timer should be used to limit step rate
//  timerSetup_cmdTimeout();                                        // timer used for handling of time-out
    
    // init circular buffer for data incomming via SPI
    inIndexGet              = 0;
    
    // init track and side vars for the floppy position
    now.side                = 0;
    now.track               = 0;
    
    streamed.side   = (BYTE) -1;
    streamed.track  = (BYTE) -1;
    streamed.sector = (BYTE) -1;
    
    prev.side       = 0;
    
    next.track      = 0;
    next.side       = 0;
    
    lastRequested.track = 0xff;
    lastRequested.side  = 0xff;
    
    lastRequestTime = 0;
    
    initUsarts();
}

void initUsarts(void)
{
    USART_InitTypeDef usartStruct;

    USART_Cmd(USART1, ENABLE);
    USART_Cmd(USART2, ENABLE);
    USART_Cmd(USART3, ENABLE);
    
    usartStruct.USART_BaudRate              = 19200;   
    usartStruct.USART_WordLength            = USART_WordLength_8b;  
    usartStruct.USART_StopBits              = USART_StopBits_1;   
    usartStruct.USART_Parity                = USART_Parity_No;
    usartStruct.USART_Mode                  = USART_Mode_Rx | USART_Mode_Tx;
    usartStruct.USART_HardwareFlowControl   = USART_HardwareFlowControl_None;
    
    USART_Init(USART1, &usartStruct);               // Configure USART1 - 19200 baud -- for communication with RPi

    usartStruct.USART_BaudRate              = 7812;   
    USART_Init(USART2, &usartStruct);               // Configure USART2 - 7812 baud  -- for communication with ST

    USART_Init(USART3, &usartStruct);               // Configure USART3 - 7812 baud  -- for communication with original keyboard
    
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);  // Enable RXNE interrupt
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);  // Enable RXNE interrupt
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);  // Enable RXNE interrupt

    NVIC_EnableIRQ(USART1_IRQn);                    // Enable USART1 global interrupt
    NVIC_EnableIRQ(USART2_IRQn);                    // Enable USART2 global interrupt
    NVIC_EnableIRQ(USART3_IRQn);                    // Enable USART3 global interrupt
}

void Exti3InterruptOn(BYTE onNotOff)
{
    if(onNotOff) {  // if ON
        NVIC->ICER[EXTI3_IRQn >> 0x05] = (uint32_t)0x01 << (EXTI3_IRQn & (uint8_t)0x1F);
    } else {                // if OFF
        NVIC->ISER[EXTI3_IRQn >> 0x05] = (uint32_t)0x01 << (EXTI3_IRQn & (uint8_t)0x1F);
    }
}
