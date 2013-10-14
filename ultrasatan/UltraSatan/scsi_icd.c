#include <stdio.h>

#include "mydefines.h"
#include "bridge.h"
#include "scsiDefs.h"
#include "scsi_icd.h"
#include "scsi6.h"
#include "mmc.h"
#include "main.h"
#include "special.h"
#include "serial.h"

#include "mydefines.h"
//------------------------------------
extern TDevice device[MAX_DEVICES];

extern BYTE WrByte;				// byte from ST for device
extern BYTE cmd[14];				// received command bytes
extern BYTE len;					// length of received command 
//------------------------------------
extern BYTE shitHasHappened;
//------------------------------------

void ProcICD(BYTE devIndex)
{
	shitHasHappened = 0;
	
	#define WRITEOUT
	
	//----------------
	// 1st we need to process the UltraSatan's special commands
	// Their format is as follows:
	// cmd[0] - (ACSI ID << 5) | 0x1f   - command in ICD format 
	// cmd[1] - 0x20     - group code 1 (1 + 10 bytes long command) and command TEST UNIT READY
	// cmd[2..3]         - the 'US' string (US as UltraSatan)
	// cmd[4..7]         - special command code / string
	// cmd[8..10]        - 3 bytes of parameters
	
	// so the complete command could look like this:
	// 0x1f, 0x20, 'USRdFW', 0x01, 0x0010  (read sector 0x0010 of firmware 1)
	
	if(cmd[2]=='U' && cmd[3]=='S')									// some UltraSatan's specific commands?
	{
		if(!cmpn(&cmd[4], "RdFW", 4))									// firmware read?
		{
			Special_ReadFW();
			return;			
		}
		//---------
		if(!cmpn(&cmd[4], "WrFW", 4))									// firmware write?
		{
			Special_WriteFW();
			return;			
		}
		//---------
		if(!cmpn(&cmd[4], "RdSt", 4))									// read settings?
		{
			Special_ReadSettings();
			return;			
		}
		//---------
		if(!cmpn(&cmd[4], "WrSt", 4))									// write settings?
		{
			Special_WriteSettings();
			return;			
		}
		//---------
		if(!cmpn(&cmd[4], "RdCl", 4))									// read clock?
		{
			Special_ReadRTC();
			return;			
		}
		//---------
		if(!cmpn(&cmd[4], "WrCl", 4))									// write clock?
		{
			Special_WriteRTC();
			return;			
		}
		//---------
		if(!cmpn(&cmd[4], "CurntFW", 7))							// read the name of currently running FW?
		{
			Special_ReadCurrentFirmwareName();
			return;			
		}
		//---------
		if(!cmpn(&cmd[4], "RdINQRN", 7))									// read INQUIRY name?
		{
			Special_ReadInquiryName();
			return;			
		}
		//---------
		if(!cmpn(&cmd[4], "WrINQRN", 7))									// write INQUIRY name?
		{
			Special_WriteInquiryName();
			return;			
		}
		//---------
	}
	//----------------
 if((cmd[2] & 0xE0) != 0x00)   			  					// if device ID isn't ZERO
	{
	device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
	device[devIndex].SCSI_SK	= SCSI_E_IllegalRequest;		// other devices = error 
	device[devIndex].SCSI_ASC	= SCSI_ASC_LU_NOT_SUPPORTED;
	device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;
	
	PIO_read(device[devIndex].LastStatus);   // send status byte

	return;
	}
	//----------------
	// now for the not present media
	if(device[devIndex].IsInit != TRUE)
	{
		// for the next 3 commands the device is not ready
		if((cmd[1] == SCSI_C_READ10) || (cmd[1] == SCSI_C_WRITE10) || (cmd[1] == SCSI_C_READ_CAPACITY))
		{
			ReturnStatusAccordingToIsInit(devIndex);
			return;	
		}
	}
	//----------------
	// if media changed, and the command is not INQUIRY and REQUEST SENSE
	if(device[devIndex].MediaChanged == TRUE)
	{
		if(cmd[1] != SCSI_C_INQUIRY)
		{
			ReturnUnitAttention(devIndex);
			return;	
		}
	}
	//----------------
//	showCommand(0xe1, 12, 0);

	
	switch(cmd[1])
	{
	case SCSI_C_READ_CAPACITY: 		
									SCSI_ReadCapacity(devIndex); 
		 							break;

	case SCSI_C_INQUIRY:			
									ICD7_to_SCSI6();
		 							SCSI_Inquiry(devIndex);
		 							break;
  	//------------------------------
	case SCSI_C_READ10:				SCSI_ReadWrite10(devIndex, TRUE); break;
	case SCSI_C_WRITE10:			SCSI_ReadWrite10(devIndex, FALSE); break;
	//----------------------------------------------------
	case SCSI_C_VERIFY:				SCSI_Verify(devIndex); break;
	
	//----------------------------------------------------
	default: 
		device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
		device[devIndex].SCSI_SK		= SCSI_E_IllegalRequest;		// other devices = error 
		device[devIndex].SCSI_ASC		= SCSI_ASC_InvalidCommandOperationCode;
		device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

//		showCommand(0xf1, 12, device[devIndex].LastStatus);
		
		PIO_read(device[devIndex].LastStatus);   // send status byte
		break;
	}
} 
//---------------------------------------------
void SCSI_Verify(BYTE devIndex)
{
	DWORD sector;
	BYTE res=0;
	WORD lenX, i;
	BYTE foundError;
  
	sector  = cmd[3];			// get starting sector #
	sector  = sector << 8;
	sector |= cmd[4];
	sector  = sector << 8;
	sector |= cmd[5];
	sector  = sector << 8;
	sector |= cmd[6];
 
	lenX  = cmd[8];	  	   		// get the # of sectors to read
	lenX  = lenX << 8;
	lenX |= cmd[9];	
	
	foundError = 0;					// no error found yet

	if((cmd[2] & 0x02) == 0x02)		// BytChk == 1? : compare with data
	{
		for(i=0; i<lenX; i++)					// all needed sectors
 		{
 			if(sector >= device[devIndex].SCapacity)	// out of bounds?
 				break;									// stop right now

			res = mmcCompare(devIndex, sector);	// compare data

			if(res!=0)							// if error, then set flag
				foundError = 1;
		
			sector++;							// next sector
		}

		if(foundError)							// problem when comparing?
		{
			device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
			device[devIndex].SCSI_SK	= SCSI_E_Miscompare;
			device[devIndex].SCSI_ASC	= SCSI_ASC_VERIFY_MISCOMPARE;
			device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;
		
			PIO_read(device[devIndex].LastStatus);   // send status byte
		}
		else									// no problem?
		{
			SendOKstatus(devIndex);
		}
	}
	else							// BytChk == 0? : no data comparison
	{								// just send all OK
		SendOKstatus(devIndex);
	}
}
//---------------------------------------------
void SCSI_ReadCapacity(BYTE devIndex)
{	 // return disk capacity and sector size
DWORD cap;
BYTE hi,midlo, midhi, lo;

PreDMA_read();

cap = device[devIndex].SCapacity;
cap--;

hi		= (cap >> 24) & 0xff;
midhi	= (cap >> 16) & 0xff;
midlo	= (cap >>  8) & 0xff;
lo		=  cap        & 0xff;

if(device[devIndex].IsInit != TRUE)
{
	hi		= 0;
	midhi	= 0;
	midlo	= 0;
	lo		= 0;
}

//#define SHOW_CAPACITY

#ifdef SHOW_CAPACITY
	uart_putchar(32);
	uart_outhexB(hi);
	uart_putchar(32);
	uart_outhexB(midhi);
	uart_putchar(32);
	uart_outhexB(midlo);
	uart_putchar(32);
	uart_outhexB(lo);
	uart_putchar('\n');
#endif

DMA_read(hi);		 	// Hi
DMA_read(midhi);	// mid-Hi
DMA_read(midlo);	// mid-Lo
DMA_read(lo);		 	// Lo

// return sector size
DMA_read(0);				 // fixed to 512 B	  
DMA_read(0);				 
DMA_read(2);
DMA_read(0);

PostDMA_read();

SendOKstatus(devIndex);				
}
//---------------------------------------------
void ICD7_to_SCSI6(void)
{
cmd[0] = cmd[1];
cmd[1] = cmd[2];
cmd[2] = cmd[3];
cmd[3] = cmd[4];
cmd[4] = cmd[5];
cmd[5] = cmd[6];
}
//---------------------------------------------
void SCSI_ReadWrite10(BYTE devIndex, char Read)
{
	DWORD sector;
	BYTE res=0;
	WORD lenX;
  
	sector  = cmd[3];
	sector  = sector << 8;
	sector |= cmd[4];
	sector  = sector << 8;
	sector |= cmd[5];
	sector  = sector << 8;
	sector |= cmd[6];
 
	lenX  = cmd[8];	  	   		// get the # of sectors to read
	lenX  = lenX << 8;
	lenX |= cmd[9];	
	//--------------------------------
	if(Read==TRUE)				// if read
		res = SCSI_Read6_SDMMC(devIndex, sector, lenX);
	else									// if write
		res = SCSI_Write6_SDMMC(devIndex, sector, lenX);
	//--------------------------------
	if(res==0)							// if everything was OK
	{
		SendOKstatus(devIndex);
	}
	else									// if error 
	{
		device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
		device[devIndex].SCSI_SK		= SCSI_E_MediumError;
		device[devIndex].SCSI_ASC		= SCSI_ASC_NO_ADDITIONAL_SENSE;
		device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

#ifdef WRITEOUT
	if(shitHasHappened)
		showCommand(0xc1, 12, device[devIndex].LastStatus);
		
	shitHasHappened = 0;
#endif
		
		PIO_read(device[devIndex].LastStatus);    // send status byte, long time-out
	}
}
//----------------------------------------------
