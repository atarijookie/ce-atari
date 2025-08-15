/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32c0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

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

#define STATE_EMPTY         0       // buffer currently not used and is empty
#define STATE_STORING       1       // write data is being stored here, but it's still incomplete, and doesn't have tags, so will be ignored by esp32
#define STATE_WAIT_FOR_SEND 2       // all data stored, start and stop tags present, but waiting for DMA to send it via SPI
#define STATE_SENDING       3       // DMA is currently sending this part of buffer

#define MFM_READ_SIZE   8
#define MFM_WRITE_SIZE   8

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
