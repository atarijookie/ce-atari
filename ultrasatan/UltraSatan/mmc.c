#include "main.h"

#include "mmc.h"
#include "spi.h"
#include "mydefines.h"

#include "serial.h"
#include "bridge.h"

extern TDevice device[MAX_DEVICES];

extern BYTE SPIstatus;
extern unsigned char brStat;

extern volatile DWORD SPIbytesToRead;

volatile BYTE *bfrIn;
volatile WORD GotBytes;

//------------------------------------
extern BYTE shitHasHappened;
//------------------------------------

//#define USE_C_FUNCTIONS		

//#define WITHOUT_ST
// Functions
//-----------------------------------------------
void mmcInit(void)
{
	int i;
	
	// initialize SPI interface
	spiInit();
	spiSetSPCKfreq(SPI_FREQ_SLOW);
}
//-----------------------------------------------
BYTE mmcReset(BYTE spiID)
{
	BYTE r1;
	WORD retry;
	BYTE isSD, buff[5], isSDHC;
	
	TimeOut_Start_ms(1000);
	
	spiSetSPCKfreq(SPI_FREQ_SLOW);
	spiCShigh(0xff);
	
	// send dummy bytes with CS high before accessing
	for(r1=0; r1<10; r1++)
	{
		spiTransferByte(0xFF);
		
		if(SPIstatus != TRUE)
			return DEVICETYPE_NOTHING;
	}

	//---------------
	// now send the card to IDLE state	
	r1 = mmcSendCommand(spiID, MMC_GO_IDLE_STATE, 0);
	
	if(r1 != 1)
		return DEVICETYPE_NOTHING;
	//---------------
	isSDHC	= FALSE;
	isSD		= FALSE;
	
	r1 = mmcSendCommand5B(spiID, SDHC_SEND_IF_COND, 0x1AA, buff);		  // init SDHC card
	
	if(r1 == 0x01)				// if got the right response -> it's an SD or SDHC card
	{
		if(buff[3] == 0x01 && buff[4] == 0xaa)	// if the rest of R7 is OK (OK echo and voltage)
		{
			//--------------
			retry = 0xffff;
	
			while(retry)
			{
				retry--;
				
				if(TimeOut_DidHappen()) {
					retry = 0;
					break;
				}
				
				r1 = mmcSendCommand(spiID, 55, 0);		  // ACMD41 = CMD55 + CMD41

				if(r1 != 1)						  // if invalid reply to CMD55, then it's MMC card
				{
					retry = 0;						// fuck, it's MMC card
					break;			
				}
		
				r1 = mmcSendCommand(spiID, 41, 0x40000000);	// ACMD41 = CMD55 + CMD41 ---  HCS (High Capacity Support) bit set

				if(r1 == 0)						  // if everything is OK, then it's SD card
					break;
					
				if(TimeOut_DidHappen()) {
					retry = 0;
					break;
				}
			}
			//--------------
			if(retry)									// if not timed out
			{
				r1 = mmcSendCommand5B(spiID, MMC_READ_OCR, 0, buff);		  

				if(r1 == 0)							// if command succeeded
				{
					if(buff[1] & 0x40)		// if CCS bit is set -> SDHC card, else SD card
					{
						isSDHC	= TRUE;
					 	return DEVICETYPE_SDHC;
					}
					else
						isSD		= TRUE;
				}
			}			
		}		
	}
	//---------------
	// if we came here, then it's initialized SD card or not initialized MMC card
	if(isSD == FALSE)				// it's not an initialized SD card, try to init
		retry = 0xffff;
	else
	{
		retry = 0;						// if it's initialized SD card, skip init
		r1 = 0;
	}
	//---------------
	while(retry)
	{
		r1 = mmcSendCommand(spiID, 55, 0);		  // ACMD41 = CMD55 + CMD41
		
		if(r1 != 1)						  // if invalid reply to CMD55, then it's MMC card
			break;			
		
		r1 = mmcSendCommand(spiID, 41, 0);		  // ACMD41 = CMD55 + CMD41
		
		if(r1 == 0)						  // if everything is OK, then it's SD card
		{
		 	isSD = TRUE;
			break;
		}
		
		if(TimeOut_DidHappen()) {
			retry = 0;
			break;
		}
		
		retry--;
	}
	
	if(isSD && r1!=0)	   	   			 // if it's SD but failed to initialize
		return DEVICETYPE_NOTHING;
		
	//-------------------------------
	if(isSD==FALSE)
	{
	 	// try to initialize the MMC card
		r1 = mmcCmdLow(spiID, MMC_SEND_OP_COND, 0, 0);
	
		if(r1 != 0)
		  return DEVICETYPE_NOTHING;
	}
	
	// set block length to 512 bytes
	r1 = mmcSendCommand(spiID, MMC_SET_BLOCKLEN, 512);
		
	if(isSD==TRUE)
	 	return DEVICETYPE_SD;
	else
		return DEVICETYPE_MMC;
}
//-----------------------------------------------
BYTE mmcCmd(BYTE cs, BYTE cmd, DWORD arg, BYTE retry, BYTE val)
{
 	BYTE r1;

	do {
		r1 = mmcSendCommand(cs, cmd, arg);
			  
		if(retry==0 || TimeOut_DidHappen())
			return 0xff;

		// do retry counter
		retry--;
		
	} while(r1 != val);
	
	return r1;
}
//-----------------------------------------------
BYTE mmcCmdLow(BYTE cs, BYTE cmd, DWORD arg, BYTE val)
{
 	BYTE r1;
	WORD retry = 0xffff;
	 
	spiCSlow(cs);		   // CS to L
	
	do
	{
	  	// issue the command
		r1 = mmcSendCommand(cs, cmd, arg);

	  	spiTransferByte(0xff);

		if(retry==0 || TimeOut_DidHappen()) {
		
			for(retry=0; retry<10; retry++)
				spiTransferByte(0xff);

			spiCShigh(cs);		   // CS to H
			return 0xff;
		}

		// do retry counter
		retry--;
			
	} while(r1 != val);

	for(retry=0; retry<10; retry++)
		spiTransferByte(0xff);
	
	spiCShigh(cs);		   // CS to H
	
	return r1;
}
//-----------------------------------------------
BYTE mmcSendCommand(BYTE cs, BYTE cmd, DWORD arg)
{
	BYTE r1;

	// assert chip select
	spiCSlow(cs);		   // CS to L

	spiTransferByte(0xFF);

	// issue the command
	r1 = mmcCommand(cmd, arg);

	spiTransferByte(0xFF);

	// release chip select
	spiCShigh(cs);		   // CS to H

	return r1;
}
//-----------------------------------------------
BYTE mmcSendCommand5B(BYTE cs, BYTE cmd, DWORD arg, BYTE *buff)
{
	BYTE r1, i;

	// assert chip select
	spiCSlow(cs);		   // CS to L

	spiTransferByte(0xFF);

	// issue the command
	r1 = mmcCommand(cmd, arg);
	buff[0] = r1;
	
	// receive the rest of R7 register	
	for(i=1; i<5; i++)										
		buff[i] = spiTransferByte(0xFF);
		
	spiTransferByte(0xFF);

	// release chip select
	spiCShigh(cs);		   // CS to H

	return r1;
}
//-----------------------------------------------
BYTE mmcCommand(BYTE cmd, DWORD arg)
{
	BYTE r1;
	WORD retry=0xffff;				// v. 1.00
//	DWORD retry=0x001fffff;			// experimental

	spiTransferByte(0xFF);

	// send command
	spiTransferByte(cmd | 0x40);
	spiTransferByte(arg>>24);
	spiTransferByte(arg>>16);
	spiTransferByte(arg>>8);
	spiTransferByte(arg);
	
	if(cmd == SDHC_SEND_IF_COND)
		spiTransferByte(0x86);	 			// crc valid only for SDHC_SEND_IF_COND
	else
		spiTransferByte(0x95);	 			// crc valid only for MMC_GO_IDLE_STATE
	
	// end command
	// wait for response
	// if more than 8 retries, card has timed-out
	// return the received 0xFF
	while(1)
	{
	 	r1 = spiTransferByte(0xFF);

		if(SPIstatus != TRUE)
			break;
		
		if(r1 != 0xff)
			break;
		
		if(retry == 0 || TimeOut_DidHappen())
			break;
			
		retry--;
	}
	
	// return response
	return r1;
}
//**********************************************************************
BYTE mmcRead(BYTE cs, DWORD sector)
{
	BYTE r1;
	DWORD i;
	BYTE byte;
	DWORD addr, addr2, deltaAddr;

	TimeOut_Start_ms(1000);
	
	// assert chip select
	spiCSlow(cs);		   // CS to L

	if(device[cs].Type != DEVICETYPE_SDHC)		// for non SDHC cards change sector into address
		sector = sector<<9;

	// issue command
	r1 = mmcCommand(MMC_READ_SINGLE_BLOCK, sector);

	// check for valid response
	if(r1 != 0x00)
	{
		shitHasHappened = 1;
		uart_putc('k');

		spiTransferByte(0xFF);
		spiCShigh(cs);		   // CS to H

		return r1;
	}

	//------------------------
	// wait for block start
	i = 0x000fffff;				// v. 1.00
	
	while(i)
	{
#ifdef USE_C_FUNCTIONS
		r1 = spiTransferByte(0xFF);					// get byte
#else
		r1 = spiTransferByteAsm(0xff);
#endif

		if(r1 == MMC_STARTBLOCK_READ)				  // if it's the wanted byte
			break;
		
		if(TimeOut_DidHappen()) {
			i = 0;
			break;
		}
		
		i--;	  									  // decrement
	}
	//------------------------
	if(i == 0)				  						  // timeout?
	{
		shitHasHappened = 1;
		uart_putc('l');						// <<< TIMEOUT HAPPENED!

		spiTransferByte(0xFF);
		spiCShigh(cs);		   // CS to H

		return 0xff;
	}
	//------------------------
	// read in data

	PreDMA_read();

//**********************************************************************
//#define ASM_PIO

#ifdef ASM_PIO
	MMC_ReadAsm();
#endif
//**********************************************************************
#define SPI_ONREAD_ASM

#ifdef SPI_ONREAD_ASM
	WORD stat;

	CLEAR(*pSPI_CTL, (1<<14));										// disable SPI
	*pSPI_CTL = 0x1c04;														// Start transfer with read of SPI_RDBR
	SET(*pSPI_CTL, (1<<14));											// enable SPI

	*pSPI_TDBR = (WORD) 0xff;
  byte = (BYTE) (*pSPI_RDBR);										// do one dummy read to init the transfer
	
	i = SPIreadSectorAsm();
	//---------------------------------
	CLEAR(*pSPI_CTL, (1<<14));										// disable SPI
	*pSPI_CTL = 0x1c05;														// Start transfer with write of SPI_TDBR
	SET(*pSPI_CTL, (1<<14));											// enable SPI
	//---------------------------------
	if(i != 0)
	{
		for(; i>0; i--)							// finish the sector
			spiTransferByte(0xFF);
			
		shitHasHappened = 1;
		uart_putc('x');
	}
	//---------------------------------
	
#endif
//**********************************************************************
//#define SLOW_PIO

#ifdef SLOW_PIO
	for(i=0; i<0x200; i++)
	{
		
#ifdef USE_C_FUNCTIONS
		byte = spiTransferByte(0xFF);					// get byte
#else
		byte = spiTransferByteAsm(0xff);
#endif

#ifndef WITHOUT_ST
	r1 = DMA_read(byte);							// send it to ST
#else
	SectorBufer[i] = byte;
#endif

		if(brStat != E_OK)		  						// if something was wrong
		{
			shitHasHappened = 1;
			uart_putc('m');

		 	for(; i<0x200; i++)							// finish the sector
		  		spiTransferByte(0xFF);
		  
			break;										// quit
		}
	}
#endif 
//**********************************************************************
	PostDMA_read();
	
	// read 16-bit CRC
	#ifdef USE_C_FUNCTIONS
		spiTransferByte(0xFF);					// get byte
		spiTransferByte(0xFF);					// get byte

		spiTransferByte(0xFF);
	#else
		spiTransferByteAsm(0xff);
		spiTransferByteAsm(0xff);

		spiTransferByteAsm(0xff);
	#endif

	spiCShigh(cs);		   // CS to H
///////////////////////////////////
// !!! THIS HELPS TO SYNCHRONIZE THE THING !!!
	for(i=0; i<3; i++)					
		spiTransferByte(0xFF);
///////////////////////////////////

	if(brStat != E_OK)
	{
		shitHasHappened = 1;
		uart_putc('n');

		return 0xff;
	}
		
	//-----------------
	// return success
	return 0;	
}
//-----------------------------------------------
BYTE mmcReadMore(BYTE cs, DWORD sector, WORD count)
{
	BYTE r1, quit;
	WORD i,j;
	BYTE byte;

	TimeOut_Start_ms(1000);
	
	// assert chip select
	spiCSlow(cs);		   // CS to L

	if(device[cs].Type != DEVICETYPE_SDHC)		// for non SDHC cards change sector into address
		sector = sector<<9;

	// issue command
	r1 = mmcCommand(MMC_READ_MULTIPLE_BLOCKS, sector);

	// check for valid response
	if(r1 != 0x00)
	{
		shitHasHappened = 1;
		
 		spiTransferByte(0xFF);
		spiCShigh(cs);		   // CS to H
		return r1;
	}

	// read in data
	PreDMA_read();
	
	quit = 0;
	
	for(j=0; j<count; j++)		   				// read this many sectors
	{
		// wait for block start
		while(spiTransferByte(0xFF) != MMC_STARTBLOCK_READ);

		for(i=0; i<0x200; i++)	   				// read this many bytes
		{
			byte = spiTransferByte(0xFF);		// get byte

			DMA_read(byte);									// send it to ST
		
#ifdef DEVNULL
SectorBufer[i] = byte;
#endif
			
			if(brStat != E_OK)		  							// if something was wrong
			{
				for(; i<0x200; i++)							// finish the sector
		  		spiTransferByte(0xFF);

		  		shitHasHappened = 1;
		  		
				quit = 1;
				break;										// quit
			}
		}			
		//---------------
		if((count - j) == 1)	// if we've read the last sector
			break;
		
		if(quit)   	 			// if error happened
			break;		
			
		// if we need to read more, then just read 16-bit CRC
		spiTransferByte(0xFF);
		spiTransferByte(0xFF);
	}
	//-------------------------------
	PostDMA_read();
	
	if(quit)	  				// if error happened
	{
		shitHasHappened = 1;
		
		mmcCommand(MMC_STOP_TRANSMISSION, 0);			// send command instead of CRC

 		spiTransferByte(0xFF);
		
		// release chip select
		spiCShigh(cs);		   // CS to H

		return 0xff;
	}	
	//----------------------
	// stop the transmition of next sector
	mmcCommand(MMC_STOP_TRANSMISSION, 0);			// send command instead of CRC

	spiTransferByte(0xFF);
	
	// release chip select
	spiCShigh(cs);		   // CS to H
	
#ifdef DEVNULL
//DumpBuffer();
#endif
	
	return 0;	
}
//-----------------------------------------------
BYTE mmcWriteMore(BYTE cs, DWORD sector, WORD count)
{
	BYTE r1, quit;
	WORD i,j;
	BYTE byte;
	DWORD thisSector;

	TimeOut_Start_ms(1000);
	
	// assert chip select
	spiCSlow(cs);		   // CS to L

	thisSector = sector;
	
	if(device[cs].Type != DEVICETYPE_SDHC)		// for non SDHC cards change sector into address
		sector = sector<<9;

	//--------------------------
/*	
	// for SD and SDHC cards issue an SET_WR_BLK_ERASE_COUNT command
	if(device[cs].Type == DEVICETYPE_SDHC || device[cs].Type == DEVICETYPE_SD)		
	{
		mmcCommand(55, 0);		  				// ACMD23 = CMD55 + CMD23
		mmcCommand(23, (DWORD) count);	
	}		
*/
	//--------------------------
	// issue command
	r1 = mmcCommand(MMC_WRITE_MULTIPLE_BLOCKS, sector);

	// check for valid response
	if(r1 != 0x00)
	{
		shitHasHappened = 1;
		uart_putc('o');

 		spiTransferByte(0xFF);
		spiCShigh(cs);		   // CS to H
		return r1;
	}
	
	//--------------
	// read in data
	PreDMA_write();
	
	quit = 0;
	//--------------
	for(j=0; j<count; j++)		   								// read this many sectors
	{
		if(thisSector >= device[cs].SCapacity)		// sector out of range?
		{
			shitHasHappened = 1;
			uart_putc('u');
			
			quit = 1;
			break;																	// quit
		}
		//-------------- 			
		while(spiTransferByte(0xFF) != 0xff);			// while busy
		
		spiTransferByte(MMC_STARTBLOCK_MWRITE);		// 0xfc as start write multiple blocks
		//-------------- 			
		
		for(i=0; i<0x200; i++)	   								// read this many bytes
		{
			r1 = DMA_write();			   	   						// get it from ST

			if(brStat != E_OK)		  								// if something was wrong
			{
				shitHasHappened = 1;
				uart_putc('g');

				for(; i<0x200; i++)										// finish the sector
		  		spiTransferByte(0xFF);

				quit = 1;
				break;																// quit
			}
			
			spiTransferByte(r1);											// send it to card
		}			
		
		// send more: 16-bit CRC
		spiTransferByte(0xFF);
		spiTransferByte(0xFF);
		
		thisSector++;															// increment real sector #
		//---------------
		if(quit)   	 			// if error happened
			break;		
	}
	//-------------------------------
	PostDMA_write();
	//-------------------------------
	while(spiTransferByte(0xFF) != 0xff);		// while busy

	spiTransferByte(MMC_STOPTRAN_WRITE);		// 0xfd to stop write multiple blocks

	while(spiTransferByte(0xFF) != 0xff);		// while busy
	//-------------------------------
	// for the MMC cards send the STOP TRANSMISSION also
	if(device[cs].Type == DEVICETYPE_MMC)		
	{
		mmcCommand(MMC_STOP_TRANSMISSION, 0);

		while(spiTransferByte(0xFF) != 0xff);		// while busy
	}
	//-------------------------------
	spiTransferByte(0xFF);
	spiCShigh(cs);		   										// CS to H
	
	if(quit)																// if failed, return error
	{
		shitHasHappened = 1;
		uart_putc('q');
		return 0xff;
	}
		
	return 0;																// success
}
//-----------------------------------------------
BYTE mmcReadJustForTest(BYTE cs, DWORD sector)
{
	BYTE r1;
	DWORD i;
	BYTE byte;
	DWORD addr, addr2, deltaAddr;

	TimeOut_Start_ms(1000);
	
	// assert chip select
	spiCSlow(cs);		   // CS to L

	if(device[cs].Type != DEVICETYPE_SDHC)		// for non SDHC cards change sector into address
		sector = sector<<9;

	// issue command
	r1 = mmcCommand(MMC_READ_SINGLE_BLOCK, sector);

	// check for valid response
	if(r1 != 0x00)
	{
		shitHasHappened = 1;
		uart_putc('k');

		spiTransferByte(0xFF);
		spiCShigh(cs);		   // CS to H

		return r1;
	}

	//------------------------
	// wait for block start
	i = 0x000fffff;
	
	while(i != 0)
	{
		r1 = spiTransferByte(0xFF);					// get byte

		if(r1 == MMC_STARTBLOCK_READ)				  // if it's the wanted byte
			break;
		
		if(TimeOut_DidHappen()) {
			i = 0;
			break;
		}
		
		i--;	  									  // decrement
	}
	//------------------------
	if(i == 0)				  						  // timeout?
	{
		shitHasHappened = 1;
		uart_putc('l');

		spiTransferByte(0xFF);
		spiCShigh(cs);		   // CS to H

		return 0xff;
	}
	//------------------------
	// read in data
	for(i=0; i<0x200; i++)
		byte = spiTransferByte(0xFF);					// get byte

	// read 16-bit CRC
	spiTransferByte(0xFF);					// get byte
	spiTransferByte(0xFF);					// get byte

	spiTransferByte(0xFF);

	spiCShigh(cs);		   // CS to H

	// return success
	return 0;	
}
//-----------------------------------------------
BYTE mmcWrite(BYTE cs, DWORD sector)
{
	BYTE r1;
	DWORD i;
	
	TimeOut_Start_ms(1000);
	
	//-----------------
	// assert chip select
	spiCSlow(cs);		   // CS to L
	
	if(device[cs].Type != DEVICETYPE_SDHC)		// for non SDHC cards change sector into address
		sector = sector<<9;
		
	// issue command
	r1 = mmcCommand(MMC_WRITE_BLOCK, sector);

	// check for valid response
	if(r1 != 0x00)
	{
		shitHasHappened = 1;
		uart_putc('f');
		
 		spiTransferByte(0xFF);
		spiCShigh(cs);		   // CS to H

		return r1;
	}
	
	// send dummy
	spiTransferByte(0xFF);
	
	// send data start token
	spiTransferByte(MMC_STARTBLOCK_WRITE);
	// write data

	PreDMA_write();
//==================================================
#define WRITE_SLOW
	
#ifdef WRITE_SLOW

	for(i=0; i<0x200; i++)
	{
#ifndef WITHOUT_ST
		r1 = DMA_write();			   	   				// get it from ST
#else
		r1 = SectorBufer[i];
#endif

		if(brStat != E_OK)	  							// if something was wrong
		{
			shitHasHappened = 1;
			uart_putc('g');
		
		 	for(; i<0x200; i++)							// finish the sector
		  		spiTransferByte(0);
	
			break;										// quit
		}  

#ifdef USE_C_FUNCTIONS
		spiTransferByte(r1);					// send byte
#else
		spiTransferByteAsm(r1);
#endif
	}
	
#endif	
//==================================================
		
	PostDMA_write();
	
	// write 16-bit CRC (dummy values)
	spiTransferByte(0xFF);
	spiTransferByte(0xFF);
	
	// read data response token
	r1 = spiTransferByte(0xFF);
	
	if( (r1&MMC_DR_MASK) != MMC_DR_ACCEPT)
	{
		shitHasHappened = 1;
		uart_putc('h');
		
 		spiTransferByte(0xFF);
		spiCShigh(cs);		   // CS to H
		return r1;
	}

	// wait until card not busy
	//------------------------
	// wait for block start
	i = 0x000fffff;
	
	while(i != 0)
	{
	 	r1 = spiTransferByte(0xFF);		   			  // receive byte
		
		if(r1 != 0)									   // if it's the wanted byte
			break;
		
		if(TimeOut_DidHappen()) {
			i = 0;
			break;
		}
			
		i--;	  									  // decrement
	}
	//------------------------
	if(i == 0)				  						  // timeout?
	{
		shitHasHappened = 1;
		uart_putc('i');

		spiTransferByte(0xFF);
		spiCShigh(cs);		   // CS to H
		
		return 0xff;
	}
	//------------------------	
	spiTransferByte(0xFF);
	spiCShigh(cs);		   	// CS to H
	
//	DumpBuffer();
		
  return 0;							// return success
}
//--------------------------------------------------------
/** 
*   Retrieves the CSD Register from the mmc 
* 
*   @return      Status response from cmd 
**/ 
BYTE MMC_CardType(BYTE cs, unsigned char *buff) 
{ 
 BYTE byte, i; 
 
 // assert chip select
	spiCSlow(cs);		   // CS to L

 // issue the command
 byte = mmcCommand(MMC_SEND_CSD, 0);

 if (byte!=0)					 // if error 
 {
	spiCShigh(cs);		   // CS to H
 	spiTransferByte(0xFF);			// Clear SPI 
    
	return byte;
 } 

 // wait for block start
 for(i=0; i<16; i++)
 	{
	byte = spiTransferByte(0xFF);
	 
	if(byte == MMC_STARTBLOCK_READ)
		break;
	}

 if (byte!=MMC_STARTBLOCK_READ)		 // if error 
 {
	spiCShigh(cs);		   // CS to H
 	spiTransferByte(0xFF);			// Clear SPI 
    
	return byte;
 } 

 // read the data
 for (i=0; i<16; i++)
 	 buff[i] = spiTransferByte(0xFF); 

 spiCShigh(cs);		   // CS to H
 spiTransferByte(0xFF);			// Clear SPI 
    
 return 0; 
} 
//--------------------------------------------------------
DWORD SDHC_Capacity(BYTE cs) 
{ 
 BYTE byte,data, multi, blk_len; 
 DWORD c_size; 
 DWORD sectors; 
 BYTE buff[16];
 
	TimeOut_Start_ms(1000);
 
	byte = MMC_CardType(cs, buff); 

	if(byte!=0)								// if failed to get card type
		return 0xffffffff;

	blk_len = 0x0F & buff[5]; // this should ALWAYS BE equal 9 on SDHC

	if(blk_len != 9)
		return 0xffffffff;

	c_size = ((DWORD)(buff[7] & 0x3F) << 16) | ((DWORD)buff[8] << 8) | ((DWORD)(buff[9]));
	sectors = (c_size+1) << 10;              // get MaxSectors -> mcap=(csize+1)*512k -> msec=mcap/BytsPerSec(fix)

	return sectors; 
}
//--------------------------------------------------------
/** 
*   Calculates the capacity of the MMC in blocks 
* 
*   @return   uint32 capacity of MMC in blocks or -1 in error; 
**/ 
DWORD MMC_Capacity(BYTE cs) 
{ 
 BYTE byte,data, multi, blk_len; 
 DWORD c_size; 
 DWORD sectors; 
 BYTE buff[16];
 
 TimeOut_Start_ms(1000);
 
 byte = MMC_CardType(cs, buff); 

 if (byte!=0)
 	return 0xffffffff;
    
 // got info okay 
 blk_len = 0x0F & buff[5]; // this should equal 9 -> 512 bytes for cards <= 1 GB
 
 /*   ; get size into reg 
      ;     6            7         8 
      ; xxxx xxxx    xxxx xxxx    xxxx xxxx 
      ;        ^^    ^^^^ ^^^^    ^^ 
 */ 
 
 data    = (buff[6] & 0x03) << 6; 
 data   |= (buff[7] >> 2); 
 c_size  = data << 4; 
 data    = (buff[7] << 2) | ((buff[8] & 0xC0)>>6); 
 c_size |= data; 
       
      /*   ; get multiplier 
         ;   9         10 
         ; xxxx xxxx    xxxx xxxx 
         ;        ^^    ^ 
      */ 
	  
 multi    = ((buff[9] & 0x03 ) << 1); 
 multi   |= ((buff[10] & 0x80) >> 7); 
 sectors  = (c_size + 1) << (multi + 2); 
 
 if(blk_len != 9)											// sector size > 512B?
 	sectors = sectors << (blk_len - 9);	// then capacity of card is bigger
 
 return sectors; 
}
//--------------------------------------------------------
BYTE EraseCard(BYTE deviceIndex)
{
BYTE res;
TDevice *dev;

 TimeOut_Start_ms(5000);
 
 dev = &device[deviceIndex];

 if(dev->Type != DEVICETYPE_MMC && dev->Type != DEVICETYPE_SD 
 			&& dev->Type != DEVICETYPE_SDHC)	// not a SD/MMC card?
 {
	return 1;
 }

 if(dev->Type == DEVICETYPE_MMC)	 				// MMC
 	res = mmcSendCommand(deviceIndex, MMC_TAG_ERASE_GROUP_START, 0);	// start
 else	  		 															// SD or SDHC
 	res = mmcSendCommand(deviceIndex, MMC_TAG_SECTOR_START, 0);				// start

 if(res!=0)
 {
	spiTransferByte(0xFF);
	spiCShigh(deviceIndex);		   // CS to H

	return res;
 }

 if(dev->Type == DEVICETYPE_MMC)		// MMC
 	res = mmcSendCommand(deviceIndex, MMC_TAG_ERARE_GROUP_END, dev->BCapacity - 512);	// end

 if(dev->Type == DEVICETYPE_SD)			// SD
 	res = mmcSendCommand(deviceIndex, MMC_TAG_SECTOR_END, dev->BCapacity - 512);			// end

 if(dev->Type == DEVICETYPE_SDHC)		// SDHC
 	res = mmcSendCommand(deviceIndex, MMC_TAG_SECTOR_END, dev->SCapacity - 1);				// end

 if(res!=0)
 {
	spiTransferByte(0xFF);
	spiCShigh(deviceIndex);		   // CS to H

	return res;
 }
 	//-----------------------------
	// assert chip select
	spiCSlow(deviceIndex);		   // CS to L

	res = mmcCommand(MMC_ERASE, 0);			   // issue the 'erase' command

	if(res!=0)					   			   // if failed
	{
 		spiTransferByte(0xFF);
		spiCShigh(deviceIndex);		   // CS to H
		return res;
	}

	while(spiTransferByte(0xFF) == 0);	 	   // wait while the card is busy
	
	spiCShigh(deviceIndex);		   // CS to H

	spiTransferByte(0xFF);
	//-----------------------------
	return 0;
}
//--------------------------------------------------------
//////////////////////////////////////////////////////////////////
BYTE mmcCompare(BYTE cs, DWORD sector)
{
	BYTE r1;
	DWORD i;
	BYTE byteSD, byteST;

	TimeOut_Start_ms(1000);
	
	// assert chip select
	spiCSlow(cs);		   // CS to L

	if(device[cs].Type != DEVICETYPE_SDHC)		// for non SDHC cards change sector into address
		sector = sector<<9;

	// issue command
	r1 = mmcCommand(MMC_READ_SINGLE_BLOCK, sector);

	// check for valid response
	if(r1 != 0x00)
	{
		shitHasHappened = 1;
		uart_putc('k');

		spiTransferByte(0xFF);
		spiCShigh(cs);		   // CS to H

		return r1;
	}

	//------------------------
	// wait for block start
	i = 0x000fffff;				
	
	while(i)
	{
#ifdef USE_C_FUNCTIONS
		r1 = spiTransferByte(0xFF);					// get byte
#else
		r1 = spiTransferByteAsm(0xff);
#endif

		if(r1 == MMC_STARTBLOCK_READ)				  // if it's the wanted byte
			break;
			
		if(TimeOut_DidHappen()) {
			i = 0;
			break;
		}		
			
		i--;	  									  // decrement
	}
	//------------------------
	if(i == 0)				  						  // timeout?
	{
		shitHasHappened = 1;
		uart_putc('l');						// <<< TIMEOUT HAPPENED!

		spiTransferByte(0xFF);
		spiCShigh(cs);		   // CS to H

		return 0xff;
	}
	//------------------------
	// read in data

	PreDMA_write();

//**********************************************************************
//#define SLOW_PIO
//#ifdef SLOW_PIO

	for(i=0; i<0x200; i++)
	{
		
#ifdef USE_C_FUNCTIONS
		byteSD = spiTransferByte(0xFF);					// get byte from card
#else
		byteSD = spiTransferByteAsm(0xff);
#endif

		byteST = DMA_write();			   	   			// get byte from Atari

		if((brStat != E_OK) || (byteST != byteSD))		// if something was wrong
		{
			shitHasHappened = 1;
		
			if(brStat != E_OK)
				uart_putc('%');
			else
				uart_putc('$');

		 	for(; i<0x200; i++)							// finish the sector
		  		spiTransferByte(0xFF);
		  
			break;										// quit
		}
	}
//#endif 
//**********************************************************************
	PostDMA_write();
	
	// read 16-bit CRC
	#ifdef USE_C_FUNCTIONS
		spiTransferByte(0xFF);					// get byte
		spiTransferByte(0xFF);					// get byte

		spiTransferByte(0xFF);
	#else
		spiTransferByteAsm(0xff);
		spiTransferByteAsm(0xff);

		spiTransferByteAsm(0xff);
	#endif

	spiCShigh(cs);		   // CS to H
///////////////////////////////////
// !!! THIS HELPS TO SYNCHRONIZE THE THING !!!
	for(i=0; i<3; i++)					
		spiTransferByte(0xFF);
///////////////////////////////////

	if(brStat != E_OK)
	{
		shitHasHappened = 1;
		uart_putc('n');

		return 0xff;
	}
		
	//-----------------
	// return success
	return 0;	
}
//-----------------------------------------------

