#ifndef _GPIO_H_
#define _GPIO_H_

#include "datatypes.h"

#include <bcm2835.h>

// assuming we're having P1 connector in version 2 (V2)
// http://www.airspayce.com/mikem/bcm2835/group__constants.html
	
// pins for XILINX programming
#define PIN_TDO					RPI_V2_GPIO_P1_03
#define PIN_TDI					RPI_V2_GPIO_P1_05
#define PIN_TCK					RPI_V2_GPIO_P1_07
#define PIN_TMS					RPI_V2_GPIO_P1_11

// pins for both STM32 programming 
#define PIN_RESET_HANS			RPI_V2_GPIO_P1_13
#define PIN_RESET_FRANZ			RPI_V2_GPIO_P1_15
#define PIN_TXD					RPI_V2_GPIO_P1_8
#define PIN_RXD					RPI_V2_GPIO_P1_10
#define PIN_TX_SEL1N2			RPI_V2_GPIO_P1_12
#define PIN_BOOT0_FRANZ_HANS	RPI_V2_GPIO_P1_22

// pins for communication with Franz and Hans
#define PIN_ATN_HANS			RPI_V2_GPIO_P1_16
#define PIN_ATN_FRANZ			RPI_V2_GPIO_P1_18
#define PIN_CS_HANS				RPI_V2_GPIO_P1_24
#define PIN_CS_FRANZ			RPI_V2_GPIO_P1_26
#define PIN_MOSI				RPI_V2_GPIO_P1_19
#define PIN_MISO				RPI_V2_GPIO_P1_21
#define PIN_SCK					RPI_V2_GPIO_P1_23

//#ifndef bool 
//	#define bool	int
//	#define true	1
//	#define false	0	
//#endif

#define	SPI_CS_HANS		BCM2835_SPI_CS0
#define	SPI_CS_FRANZ	BCM2835_SPI_CS1

#define SPI_ATN_HANS	PIN_ATN_HANS
#define SPI_ATN_FRANZ	PIN_ATN_FRANZ

bool gpio_open(void);
void gpio_close(void);

void spi_tx_rx(int whichSpiCS, int count, BYTE *txBuf, BYTE *rxBuf);
bool spi_atn(int whichSpiAtn);

#endif
