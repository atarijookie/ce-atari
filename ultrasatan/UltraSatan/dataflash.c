//-----------------------------------------------
#include <stdio.h>
#include <cdefBF531.h>

#include "serial.h"
#include "dataflash.h"
#include "spi.h"
#include "main.h"

#include "mydefines.h"
//---------------------------------------
BYTE Flash_ReadStatusByte(BYTE *value)
{
	BYTE a;

	//-----
	spiCSlow(DATAFLASH_ID);								// CS to L

	spiTransferByte(0xd7);								// send command
	a = spiTransferByte(0xff);						// read status byte

	spiCShigh(ALL_CS_PINS);								// all CS pins to H
	//-----

	if((a & 0x3c) != 0x1c)								// the status byte does not have the right format?
		return FALSE;
		
	if(value)
		*value = a;

	return TRUE;
}
//---------------------------------------
BYTE Flash_GetIsReady(void)
{
	BYTE res, status;
	
	res = Flash_ReadStatusByte(&status);
	
	if(!res)
		return 0;
		
	if(status & 0x80)
		return 1;
		
	return 0;
}
//---------------------------------------
BYTE Flash_GetIsProtected(void)
{
	BYTE res, status;
	
	res = Flash_ReadStatusByte(&status);
	
	if(!res)
		return 0;
		
	if(status & 0x02)
		return 1;
		
	return 0;
}
//---------------------------------------
BYTE Flash_ReadPage(DWORD page, BYTE *buffer)
{
	int i;
	DWORD retries;
	BYTE res;
	
	if(page > 2047)													// page number is out of range?
		return 0;			
	//---------------
	retries = 500;
	
	while(1)																// wait for flash to get ready
	{
		ShortDelay();													
	
		res = Flash_GetIsReady();							// get flash readyness
		
		if(res)																// if flash is ready, quit the loop
			break;
			
		retries--;														// decrement retries
		
		if(retries==0)												// time-out?
			return 0;														// return with error
	}
	//---------------
	page = page & 0x07FF;
	page = page << 8;
	
	spiCSlow(DATAFLASH_ID);									// CS to L
	
	spiTransferByte(0xd2);									// 'Main Memory Page Read' command
	spiTransferByte((page >> 16) & 0xff);		// now the address bits
	spiTransferByte((page >>  8) & 0xff);
	spiTransferByte((page      ) & 0xff);
	
	for(i=0; i<4; i++)											// the '32 DON'T CARE BITS'
		spiTransferByte(0xff);
	
	for(i=0; i<256; i++)										// read out the bytes
		buffer[i] = spiTransferByte(0xff);
	
	spiCShigh(ALL_CS_PINS);									// all CS pins to H
	
	return 1;																// return with success
}
//---------------------------------------
BYTE Flash_WritePage(DWORD page, BYTE *buffer)
{
	BYTE res;
	DWORD retries;

	if(page > 2047)													// page number is out of range?
		return 0;			
	//-----------------------	
	Flash_WriteBuffer(buffer);							// write data to internal buffer
	//-----------------------	
	// write the buffer 1 to the main memory of flash
	page = page & 0x07FF;
	page = page << 8;
	
	spiCSlow(DATAFLASH_ID);									// CS to L

	spiTransferByte(0x83);									// 'Buffer to Main Memory Page Program with Built-in Erase' command
	spiTransferByte((page >> 16) & 0xff);		// now the address bits
	spiTransferByte((page >>  8) & 0xff);
	spiTransferByte((page      ) & 0xff);

	spiCShigh(ALL_CS_PINS);									// all CS pins to H
	//------------------------
	// wait while it finishes writing...
	retries = 500;
	
	while(1)																// wait for the flash to get ready
	{
		ShortDelay();
	
		res = Flash_GetIsReady();							// get the flash readyness
		
		if(res)																// if ready, return with success
			return 1;
			
		retries--;														// decrement retries
		
		if(retries==0)												// if timed-out
			return 0;														// return with error
	}
}
//---------------------------------------
void Flash_WriteBuffer(BYTE *buffer)
{
	int i;
	
	// write data to the flash's internal buffer 1
	spiCSlow(DATAFLASH_ID);									// CS to L
	
	spiTransferByte(0x84);									// 'Buffer Write' command
	spiTransferByte(0x00);
	spiTransferByte(0x00);
	spiTransferByte(0x00);

	for(i=0; i<256; i++)										// write the bytes
		spiTransferByte(buffer[i]);
	
	spiCShigh(ALL_CS_PINS);									// all CS pins to H
}
//---------------------------------------
void ShortDelay(void)
{
	DWORD i;
	
	for(i=0; i<3000; i++)
		asm("nop;\nnop;\nnop;\n");
}
//---------------------------------------

