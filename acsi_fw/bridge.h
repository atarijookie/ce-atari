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


// set GPIOB0-7 as --- CNF1:0 -- 01 (floating input), MODE1:0 -- 00 (input)
#define ACSI_DATADIR_WRITE()		{	GPIOB->CRL = 0x44444444; }
// set GPIOB0-7 as --- CNF1:0 -- 00 (push pull output), MODE1:0 -- 11 (output, max 50 MHz)
#define ACSI_DATADIR_READ()			{	GPIOB->CRL = 0x33333333; }


BYTE PIO_gotFirstCmdByte(void);					// check if we got the 1st command byte
BYTE PIO_writeFirst(void);							// get 1st CMD byte from ST  -- without setting INT
BYTE PIO_write(void);										// get next CMD byte from ST -- with setting INT to LOW and waiting for CS 
