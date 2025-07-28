/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "circularbuffer.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define MFM_4US         1
#define MFM_6US         2
#define MFM_8US         3

#define PULSE_TOO_SHORT 18
#define PULSE_4US       36
#define PULSE_6US       52
#define PULSE_8US       72

#define PIN_WGATE       (1 << 3)        // GPIOA 3, write is happening when WGATE is L
#define PIN_RXE         (1 << 5)        // GPIOA 5, SPI can get more data if this is H

#define TAG_WRITE_START 0x80    // start of sector data
#define TAG_WRITE_END   0xc0    // end of sector data

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi1_tx;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim16;
DMA_HandleTypeDef hdma_tim3_up;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM3_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM16_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

uint8_t* pWrite;

#define MFM_READ_SIZE   8
uint16_t mfmReadStreamBuffer[MFM_READ_SIZE];

void updateReadTimerDma(uint8_t lowerNotUpper)
{
    const uint16_t arrValues[4] = {7, 7, 11, 15};       // conversion table from mfm packed symbol to timer ARR value (for 0 us, 4 us, 6 us, 8 us)
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

void updateWriteData(uint32_t captured)
{
    static uint8_t streamByte = 0;
    static uint8_t bits = 0;
    static uint32_t prevCaptured = 0;

    uint32_t duration = captured - prevCaptured;    // calculate the change from previous captured value
    prevCaptured = captured;                        // store the current captured time

    uint8_t newTime = 0;

    if(duration < PULSE_TOO_SHORT) {    // if this pulse is too short (less than 2.7 us long)
        return;
    } else if(duration < PULSE_4US) {   // 4 us? (interval 2.7 us - 5.0 us) (40 pulses of 0.125 us)
        newTime = MFM_4US;
    } else if(duration < PULSE_6US) {   // 6 us? (interval 5.0 us - 7.0 us) (56 pulses of 0.125 us)
        newTime = MFM_6US;
    } else if(duration < PULSE_8US) {   // 8 us? (interval 7.0 us - 9.0 us) (72 pulses of 0.125 us)
        newTime = MFM_8US;
    } else {                            // pulse too long? (longer than 9 us)
        return;
    }

    streamByte = (streamByte << 2) | newTime;   // append new time to streamByte
    bits += 2;              // now got 2 more bits

    if(bits >= 8) {         // got 8 bits?
        // tx buffer not full? add streamByte to tx buffer
        if(txCnt < WRITEBUFFER_SIZE) {
            *pWrite = streamByte;
            pWrite++;
            txCnt++;
        }

        bits = 0;           // don't have bits now
    }
}

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

    DMA1_Channel3->CPAR = (uint32_t) &(TIM3->DMAR);         // peripheral address: SPI DR
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

#define STATE_EMPTY         0       // buffer currently not used and is empty
#define STATE_STORING       1       // write data is being stored here, but it's still incomplete, and doesn't have tags, so will be ignored by esp32
#define STATE_WAIT_FOR_SEND 2       // all data stored, start and stop tags present, but waiting for DMA to send it via SPI
#define STATE_SENDING       3       // DMA is currently sending this part of buffer

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

    //-------------
    // DMA channel 3
    // Half Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_HT3) != 0U)
    {
       DMA1->IFCR = DMA_FLAG_HT3;   // clear flag
       updateReadTimerDma(1);
    }

    // Transfer Complete Interrupt management
    if((flag_it & DMA_FLAG_TC3) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TC3;  // clear flag
        updateReadTimerDma(0);
    }

    // Transfer Error Interrupt management
    if((flag_it & DMA_FLAG_TE3) != 0)
    {
        DMA1->IFCR = DMA_FLAG_TE3;
    }
}

// on write START - see which part of buffer is empty - lower part or upper part?
// Initialize pointer and count to the free empty part of tx buffer.
void onWriteStart(void)
{
    if(bfrStateLow == STATE_EMPTY) {              // lower part empty?
        bfrStateLow = STATE_STORING;
        pWrite = &txData[1];
        txCnt = 0;
    } else if(bfrStateHigh == STATE_EMPTY) {      // upper part empty?
        bfrStateHigh = STATE_STORING;
        pWrite = &txData[WRITEBUFFER_SIZE + 1];
        txCnt = 0;
    }
}

// on write END - mark start and end of this buffer with known tags, this
// marks the buffer used and esp will know that the valid data is between these tags.
void onWriteEnd(void)
{
    // TODO: disable int?

    if(txCnt < WRITEBUFFER_SIZE) {
        *pWrite = TAG_WRITE_END;    // store END tag after the last valid data byte
        pWrite++;
        txCnt++;
    }

    // write was storing data to lower part of buffer?
    if(bfrStateLow == STATE_STORING) {
        bfrStateLow = STATE_WAIT_FOR_SEND;
        txData[0] = TAG_WRITE_START;
        txData[WRITEBUFFER_SIZE - 1] = TAG_WRITE_END;
    }

    // write was storing data to upper part of buffer?
    if(bfrStateHigh == STATE_STORING) {
        bfrStateHigh = STATE_WAIT_FOR_SEND;
        txData[WRITEBUFFER_SIZE] = TAG_WRITE_START;
        txData[TX_DATA_SIZE - 1] = TAG_WRITE_END;
    }

    // TODO: enable int?
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM3_Init();
  MX_SPI1_Init();
  MX_TIM16_Init();
  /* USER CODE BEGIN 2 */

  bfrStateLow = STATE_EMPTY;
  bfrStateHigh = STATE_EMPTY;

  setupSpiUsingCircularDma();
  setupTMI3circularDma();

  RX_CLEAR();

  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim16, TIM_CHANNEL_1);

  pWrite = &txData[1];
  txCnt = 0;

  UPDATE_PIN_RXE;       // set RXE pin because we're empty

  uint8_t writingPrev = (GPIOA->IDR & PIN_WGATE) == 0;      // track WGATE changes with this var, init to current WGATE state

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    uint8_t writingNow = (GPIOA->IDR & PIN_WGATE) == 0;     // true if now writing data (0.3 us)

    // when the WGATE signal changes, we need to
    // - on START of write - get pointer to empty buffer
    // - on END of write - just mark the buffer as ready to be sent and valid
    if(writingPrev != writingNow)   // when WGATE signal changed
    {
        writingPrev = writingNow;

        if(writingNow) {
            onWriteStart();
        } else {
            onWriteEnd();
        }
    }

    if(writingNow)  // when writing to floppy
    {
        // CC1IF bit set? input capture happened
        /* Takes 2.61 us when also storing byte to circular buffer,
         * takes 1.81 us when just storing bits, not adding to circular buffer.
         * Can happen in 4 us intervals (min), but up to 6 us or 8 us also.
         */
        if (TIM16->SR & TIM_SR_CC1IF) {
            TIM16->SR = ~TIM_SR_CC1IF;            // Clear the flag
            uint32_t captured = TIM16->CCR1;
            updateWriteData(captured);            // send the input WDATA to buffer
        }
    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_SLAVE;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_HARD_INPUT;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 23;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 8;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_TIM_DISABLE_OCxPRELOAD(&htim3, TIM_CHANNEL_1);
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 5;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 65535;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_FALLING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim16, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */

  /* USER CODE END TIM16_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
