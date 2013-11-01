#include "defs.h"
#include "stm32f10x.h"                       // STM32F103 definitions
#include "stm32f10x_exti.h"
#include "misc.h"


// bridge functions return value
#define	E_TimeOut			0
#define	E_OK					1
#define	E_OK_A1				2
#define	E_CARDCHANGE	3	
#define	E_RESET				4	


// set GPIOB0-7 as floating input, then RnW to LOW
#define ACSI_DATADIR_WRITE()		{	GPIOB->CRL = 0x44444444; GPIOA->BRR = aRNW; }

// first RnW to HIGH, then set GPIOB0-7 as push pull output
#define ACSI_DATADIR_READ()			{	GPIOA->BSRR = aRNW; GPIOB->CRL = 0x33333333; }

void timeoutStart(void);								// starts the timeout timer
BYTE timeout(void);											// returns TRUE if timeout since writeFirst occured

BYTE PIO_gotFirstCmdByte(void);					// check if we got the 1st command byte
BYTE PIO_writeFirst(void);							// get 1st CMD byte from ST  -- without setting INT
BYTE PIO_write(void);										// get next CMD byte from ST -- with setting INT to LOW and waiting for CS 

void PIO_read(BYTE val);								// send status byte to ST 

BYTE DMA_write(void);										// get byte from ST using DMA
void DMA_read(BYTE val);								// send byte to ST using DMA

