#include "bridge.h"

extern BYTE brStat;															// status from bridge

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
	
	brStat = E_OK;																// init bridge status to E_OK
	EXTI->PR = aCMD | aCS;												// clear int for 1st CMD byte (aCMD) and also int for CS, because also that one will be set
	
	ACSI_DATADIR_WRITE();													// data as inputs
	GPIOB->BRR = aRNW;														// RnW to LOW, that means WRITE (ST to device)

	val = GPIOB->IDR;															// read the data
	return val;
}

// get next CMD byte from ST -- with setting INT to LOW and waiting for CS 
BYTE PIO_write(void)
{
	BYTE val = 0;
	
	// create rising edge on aPIO
	GPIOB->BSRR	= aPIO;														// aPIO to HIGH
	GPIOB->BRR	= aPIO;														// aPIO to LOW

	while(1) {
		WORD exti = EXTI->PR;
		
		if(exti & aCS) {														// if CS arrived
			val = GPIOB->IDR;													// read the data
			break;
		}
		
		if(0) {																			// if timeout happened
			brStat = E_TimeOut;												// set the bridge status
			break;
		}
	}
	
	EXTI->PR = aCS;																// clear int for CS
	return val;
}

