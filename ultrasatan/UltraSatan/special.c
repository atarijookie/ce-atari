//-------------------------------------------------------------------
#include <stdio.h>
#include <cdefBF531.h>

#include "dataflash.h"
#include "serial.h"
#include "bridge.h"
#include "main.h"
#include "special.h"

#include "scsiDefs.h"
#include "scsi_icd.h"
#include "scsi6.h"

#include "rtc.h"

#include "mydefines.h"
//-----------------------------------------------
// Format of UltraSatan's special commands is as follows:
// cmd[0] - (ACSI ID << 5) | 0x1f   - command in ICD format 
// cmd[1] - 0x20     - group code 1 (1 + 10 bytes long command) and command TEST UNIT READY
// cmd[2..3]         - the 'US' string (US as UltraSatan)
// cmd[4..7]         - special command code / string
// cmd[8..10]        - 3 bytes of parameters
	
// so the complete command could look like this:
// 0x1f, 0x20, 'USRdFW', 0x01, 0x0010  (ACSI device 0: read sector 0x0010 of firmware 1)
//-----------------------------------------------
extern BYTE SectorBufer[2*512];
extern BYTE brStat;										// status from bridge
extern BYTE cmd[14];									// received command bytes
extern TDevice device[MAX_DEVICES];

extern BYTE logBuffer[512];								// command and status log
//-----------------------------------------------
void Special_ReadFW(void)
{
	BYTE fw, res1, res2;
	WORD sector, i;
	
	fw			= cmd[8];
	sector	= (((WORD)cmd[9])<<8) | ((WORD)cmd[10]);
	
	if(fw<1 || fw>4 || sector>199)							// number of FW or sector number out of range?
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	res1 = Flash_GetIsReady();									// check if dataflash is ready
	
	if(!res1)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	// one HDD sector is 512 B, but one flash page is only 256 B, so we will read 2 pages
	sector = (fw * 400) + (sector * 2);					// convert sector # and FW # to page #
	
	res1 = Flash_ReadPage(sector,			&SectorBufer[0]);
	res2 = Flash_ReadPage(sector + 1,	&SectorBufer[256]);

	if(!res1 || !res2)													// if failed to read 1st or 2nd flash page
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}

	//-----------	
	// send sector to ST
	PreDMA_read();															// prepare bridge

	for(i=0; i<512; i++)
	{
		DMA_read(SectorBufer[i]);		

		if(brStat != E_OK)		  									// if something was wrong
			break;																	// quit
	}
	
	PostDMA_read();
	
	if(brStat != E_OK)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	PIO_read(SCSI_ST_OK);												// everything went OK
}
//-----------------------------------------------
void Special_WriteFW(void)
{
	BYTE fw, res1, res2;
	WORD sector, i;
	
	fw			= cmd[8];
	sector	= (((WORD)cmd[9])<<8) | ((WORD)cmd[10]);
	
	if(fw<1 || fw>4 || sector>199)							// number of FW or sector number out of range?
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	res1 = Flash_GetIsReady();									// check if dataflash is ready
	
	if(!res1)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	// get the sector from ST
	PreDMA_write();
	
	for(i=0; i<512; i++)
	{
		SectorBufer[i] = DMA_write();			   	   	// get it from ST

		if(brStat != E_OK)	  										// if something was wrong
			break;																	// quit
	}
	
	PostDMA_write();
	
	if(brStat != E_OK)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	// one HDD sector is 512 B, but one flash page is only 256 B, so we will read 2 pages
	sector = (fw * 400) + (sector * 2);					// convert sector # and FW # to page #
	
	res1 = Flash_WritePage(sector,			&SectorBufer[0]);
	res2 = Flash_WritePage(sector + 1,	&SectorBufer[256]);

	if(!res1 || !res2)													// if failed to read 1st or 2nd flash page
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	PIO_read(SCSI_ST_OK);												// everything went OK
}
//-----------------------------------------------
void Special_ReadSettings(void)
{
	BYTE fw, res;
	WORD sector, i;
	
	//-----------	
	res = Flash_GetIsReady();									// check if dataflash is ready
	
	if(!res)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	res = Flash_ReadPage(2020, &SectorBufer[0]);	// read the page with settings

	if(!res)																		// if failed to read flash page
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	
	SectorBufer[1] = Config_GetBootBase();			// store here if the base FW should be booted
	
	for(i=256; i<512; i++)											// clear the 2nd half of the buffer
		SectorBufer[i] = 0;
	//-----------	
	// send sector to ST
	PreDMA_read();															// prepare bridge

	for(i=0; i<512; i++)
	{
		DMA_read(SectorBufer[i]);		

		if(brStat != E_OK)		  									// if something was wrong
			break;																	// quit
	}
	
	PostDMA_read();
	
	if(brStat != E_OK)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	PIO_read(SCSI_ST_OK);												// everything went OK
}
//-----------------------------------------------
void Special_WriteSettings(void)
{
	BYTE fw, res;
	WORD sector, i;
	
	if(cmd[8]!=0x83 || cmd[9]!=0x03 || cmd[10]!=0x17)		// not a magical number?
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	res = Flash_GetIsReady();									// check if dataflash is ready
	
	if(!res)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	// get the sector from ST
	PreDMA_write();
	
	for(i=0; i<512; i++)
	{
		SectorBufer[i] = DMA_write();			   	   	// get it from ST

		if(brStat != E_OK)	  										// if something was wrong
			break;																	// quit
	}
	
	PostDMA_write();
	
	if(brStat != E_OK)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	res = Flash_WritePage(2020, &SectorBufer[0]);

	if(!res)																		// if failed to write the flash page
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	PIO_read(SCSI_ST_OK);												// everything went OK
	//-----------	
	uart_prints("\nGot new config!\n");
	//-----------	
	// now read back the config
	uart_prints("\nReading config: ");

	res = Config_Read(1);
	
	if(!res)
	{
		uart_prints("failed. Using default config.");
		Config_SetDefault();
	}
	else
		uart_prints("OK");
	
}
//-----------------------------------------------
void Special_ReadCurrentFirmwareName(void)
{
	BYTE fw, res;
	WORD sector, i;
	BYTE *ver = (BYTE *) VERSION_STRING;
	//-----------	
	for(i=0; i<512; i++)												// clear the buffer
		SectorBufer[i] = 0;
		
	for(i=0; i<512; i++)												// fill the buffer with the version string										
	{
		if(ver[i]==0)															// end of string?
			break;
		
		SectorBufer[i] = ver[i];									// copy in the string
	}
	//-----------	
	// send sector to ST
	PreDMA_read();															// prepare bridge

	for(i=0; i<512; i++)
	{
		DMA_read(SectorBufer[i]);		

		if(brStat != E_OK)		  									// if something was wrong
			break;																	// quit
	}
	
	PostDMA_read();
	
	if(brStat != E_OK)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	PIO_read(SCSI_ST_OK);												// everything went OK
}
//-----------------------------------------------
void Special_ReadRTC(void)
{
	BYTE fw, res;
	WORD sector, i;
	
	//-----------	
	for(i=0; i<512; i++)												// clear the buffer
		SectorBufer[i] = 0;
		
	SectorBufer[0] = 'R';												// do some kind of signature
	SectorBufer[1] = 'T';
	SectorBufer[2] = 'C';
	
	rtc_GetClock(&SectorBufer[3]);							// fill sector with clock data
	//-----------	
	// send sector to ST
	PreDMA_read();															// prepare bridge

	for(i=0; i<512; i++)
	{
		DMA_read(SectorBufer[i]);		

		if(brStat != E_OK)		  									// if something was wrong
			break;																	// quit
	}
	
	PostDMA_read();
	
	if(brStat != E_OK)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	PIO_read(SCSI_ST_OK);												// everything went OK
}
//-----------------------------------------------
void Special_WriteRTC(void)
{
	BYTE fw, res;
	WORD sector, i;
	
	if(cmd[8]!='R' || cmd[9]!='T' || cmd[10]!='C')		// not a magical number?
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	// get the sector from ST
	PreDMA_write();
	
	for(i=0; i<512; i++)
	{
		SectorBufer[i] = DMA_write();			   	   	// get it from ST

		if(brStat != E_OK)	  										// if something was wrong
			break;																	// quit
	}
	
	PostDMA_write();
	
	if(brStat != E_OK)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	if(SectorBufer[0]!='R' || SectorBufer[1]!='T' || SectorBufer[2]!='C')
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	
	rtc_SetClock(SectorBufer[3], SectorBufer[4], SectorBufer[5], SectorBufer[6], SectorBufer[7], SectorBufer[8]);
	//-----------	
	PIO_read(SCSI_ST_OK);												// everything went OK
}
//-----------------------------------------------
void Special_ReadInquiryName(void)
{
	BYTE fw, res;
	WORD sector, i;
	
	//-----------	
	res = Flash_GetIsReady();									// check if dataflash is ready
	
	if(!res)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	res = Flash_ReadPage(2019, &SectorBufer[0]);	// read the page with settings

	if(!res)																		// if failed to read flash page
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	
	for(i=10; i<512; i++)												// clear the most of that sector
		SectorBufer[i] = 0;
		
	if(SectorBufer[0] == 0x00 || SectorBufer[0] == 0xff)	// if no name present
		copyn(&SectorBufer[0], "UltraSatan", 10);						// copy in the default INQUIRY name
	//-----------	
	// send sector to ST
	PreDMA_read();															// prepare bridge

	for(i=0; i<512; i++)
	{
		DMA_read(SectorBufer[i]);		

		if(brStat != E_OK)		  									// if something was wrong
			break;																	// quit
	}
	
	PostDMA_read();
	
	if(brStat != E_OK)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	PIO_read(SCSI_ST_OK);												// everything went OK
}
//-----------------------------------------------
void Special_WriteInquiryName(void)
{
	BYTE fw, res;
	WORD sector, i;
	
	//-----------	
	res = Flash_GetIsReady();									// check if dataflash is ready
	
	if(!res)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	// get the sector from ST
	PreDMA_write();
	
	for(i=0; i<512; i++)
	{
		SectorBufer[i] = DMA_write();			   	   	// get it from ST

		if(brStat != E_OK)	  										// if something was wrong
			break;																	// quit
	}
	
	PostDMA_write();
	
	if(brStat != E_OK)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	res = Flash_WritePage(2019, &SectorBufer[0]);

	if(!res)																		// if failed to write the flash page
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}
	//-----------	
	PIO_read(SCSI_ST_OK);												// everything went OK
	//-----------	
	ReadInquiryName();
}
//-----------------------------------------------
void Special_ReadLog(void)
{
	WORD i;
	
	//-----------	
	// send sector to ST
	PreDMA_read();															// prepare bridge

	for(i=0; i<512; i++)
	{
		DMA_read(logBuffer[i]);		

		if(brStat != E_OK)		  									// if something was wrong
			break;																	// quit
	}
	
	PostDMA_read();
	//-----------	
	
	// send status
	if(brStat != E_OK)
	{
		PIO_read(SCSI_ST_CHECK_CONDITION);				// shit happened
		return;
	}

	PIO_read(SCSI_ST_OK);												// everything went OK
}
//-----------------------------------------------


