#include <stdio.h>

#include "main.h"
#include "bridge.h"
#include "mydefines.h"

extern char Count;
extern unsigned char brStat;
extern char DevID;

extern TDevice device[MAX_DEVICES];

// ID of this device (TARGET ID in Profibuch terminology)
//extern unsigned char buffer[512];
//---------------------------------------
// send data from device to ST in PIO mode
// input  - byte   : the data for the ST
//        - timeout: time in 1/200 secs
// output - E_OK or E_TimeOut
BYTE PIO_read(BYTE byte)
{

	
return 0;
}
//---------------------------------------
// get data from ST for the device in PIO mode
// input  - timeout: time in 1/200 secs
//        - DoIRQ  : should the IRQ go LOW?
// output - WrByte : the byte written from ST to device
//        - E_OK or E_TimeOut
BYTE PIO_write(void)
{
	
return 0;								// get only the data
}
//---------------------------------------
void PreDMA_read(void)
{

}
//---------------------------------------
void PostDMA_read(void)
{

}
//---------------------------------------
// send data from device to ST in DMA mode
// input  - byte   : the data for the ST
//        - timeout: time in 1/200 secs
// output - E_OK or E_TimeOut
BYTE DMA_read(BYTE byte)
{
	
return 0;	 			   				// return result
}
//---------------------------------------
void PreDMA_write(void)
{

}
//---------------------------------------
void PostDMA_write(void)
{

}
//---------------------------------------
// get data from ST for the device in DMA mode
// input  - timeout: time in 1/200 secs
// output - WrByte : the byte written from ST to device
//        - E_OK or E_TimeOut
BYTE DMA_write(void)
{

	return 0;									// get only the data
}
//---------------------------------------
BYTE GetCmdByte(void)
{
	WORD wVal, wSignals, wData;
	
	wVal = *pFIO_DIR;										// read current PF directions
	wVal = wVal & (~aDATA_MASK);				// clear the bits where the ACSI data is
	*pFIO_DIR = wVal;										// ACSI data as input
	
	*pFIO_INEN = aDATA_MASK;						// enable ACSI DATA pins as inputs

	while(TRUE)
	{
		wSignals	= *pASYNCMEM;						// read ACSI controll signals (A1, CS, ...)
		wData			= *pFIO_FLAG_D;					// read ACSI data

		if((wSignals & (aRESET | aCS | aA1)) == aRESET)		// wait while RESET is H, aCS is L, aA1 is L
			break;
	}
				
	wData = wData >> 8;									// shift data to the lower nibble
	return (BYTE)wData;									// return the data
}
//---------------------------------------

