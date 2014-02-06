#include "bridge.h"
#include "timers.h"

extern BYTE brStat;															// status from bridge

void timeoutStart(void)
{
	// init the timer 4, which will serve for timeout measuring 
  TIM_Cmd(TIM3, DISABLE);												// disable timer
	TIM3->CNT	= 0;																// set timer value to 0
	TIM3->SR	= 0xfffe;														// clear UIF flag
  TIM_Cmd(TIM3, ENABLE);												// enable timer
}	

__forceinline BYTE timeout(void)
{
	if((TIM3->SR & 0x0001) != 0) {		// overflow of TIM4 occured?
		TIM3->SR = 0xfffe;							// clear UIF flag
		return TRUE;
	}
	
	return FALSE;
}

BYTE PIO_gotFirstCmdByte(void)
{
	WORD exti = EXTI->PR;													// Pending register (EXTI_PR)
		
	if(exti & aCMD ) {
		return TRUE;
	}
	
	return FALSE;
}

// get 1st CMD byte from ST  -- without setting INT
BYTE PIO_writeFirst(void)
{
	BYTE val;
	
	timeoutStart();																// start the timeout timer
	ACSI_DATADIR_WRITE();													// data as inputs (write)
	
	// init vars, 
	brStat = E_OK;																// init bridge status to E_OK
	EXTI->PR = aCMD | aCS;												// clear int for 1st CMD byte (aCMD) and also int for CS, because also that one will be set

	val = GPIOB->IDR;															// read the data
	return val;
}

// get next CMD byte from ST -- with setting INT to LOW and waiting for CS 
BYTE PIO_write(void)
{
	BYTE val = 0;
	
	// create rising edge on aPIO
	GPIOA->BSRR	= aPIO;														// aPIO to HIGH
	GPIOA->BRR	= aPIO;														// aPIO to LOW

	while(1) {																		// wait for CS or timeout
		WORD exti = EXTI->PR;
		
		if(exti & aCS) {														// if CS arrived
			val = GPIOB->IDR;													// read the data
			break;
		}
		
		if(timeout()) {															// if timeout happened
			brStat = E_TimeOut;												// set the bridge status
			break;
		}
	}
	
	EXTI->PR = aCS;																// clear int for CS
	return val;
}

// send status byte to ST 
void PIO_read(BYTE val)
{
	ACSI_DATADIR_READ();													// data as outputs (read)

	GPIOB->ODR = val;															// write the data to output data register
	
	// create rising edge on aPIO
	GPIOA->BSRR	= aPIO;														// aPIO to HIGH
	GPIOA->BRR	= aPIO;														// aPIO to LOW

	while(1) {																		// wait for CS or timeout
		WORD exti = EXTI->PR;
		
		if(exti & aCS) {														// if CS arrived
			break;
		}
		
		if(timeout()) {															// if timeout happened
			brStat = E_TimeOut;												// set the bridge status
			break;
		}
	}
	
	EXTI->PR = aCS;																// clear int for CS
	ACSI_DATADIR_WRITE();													// data as inputs (write)
}

__forceinline void DMA_read(BYTE val)
{
	GPIOB->ODR = val;															// write the data to output data register
	
	// create rising edge on aDMA
	GPIOA->BSRR	= aDMA;														// aDMA to HIGH
	GPIOA->BRR	= aDMA;														// aDMA to LOW

	while(1) {																		// wait for ACK or timeout
		WORD exti = EXTI->PR;
		
		if(exti & aACK) {														// if ACK arrived
			break;
		}
		
		if(timeout()) {															// if timeout happened
			brStat = E_TimeOut;												// set the bridge status
			break;
		}
	}
	
	EXTI->PR = aACK;															// clear int for ACK
}

BYTE DMA_write(void)
{
	BYTE val = 0;
	
	// create rising edge on aDMA
	GPIOA->BSRR	= aDMA;														// aDMA to HIGH
	GPIOA->BRR	= aDMA;														// aDMA to LOW

	while(1) {																		// wait for ACK or timeout
		WORD exti = EXTI->PR;
		
		if(exti & aACK) {														// if ACK arrived
			val = GPIOB->IDR;													// read the data
			break;
		}
		
		if(timeout()) {															// if timeout happened
			brStat = E_TimeOut;												// set the bridge status
			break;
		}
	}
	
	EXTI->PR = aACK;																// clear int for ACK
	return val;
}
