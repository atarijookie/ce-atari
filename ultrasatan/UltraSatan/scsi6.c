// code for SCSI(6) commands support

#include <stdio.h>

#include "bridge.h"
#include "scsiDefs.h"
#include "scsi6.h"
#include "mmc.h"

#include "main.h"
#include "mydefines.h"

#include "serial.h"

extern TDevice device[MAX_DEVICES];
//----------------------------
extern BYTE brStat;						// byte from ST for device
extern BYTE cmd[20];					// received command bytes
extern BYTE len;							// length of received command 

extern BYTE InquiryName[11];	// name of device in INQUIRY
//----------------------------
#define WRITEOUT

BYTE shitHasHappened;

//----------------------------
void ProcSCSI6(BYTE devIndex)
{
	BYTE justCmd, lun;
	
	shitHasHappened = 0;
	
	justCmd	= cmd[0] & 0x1f;				// get only the command part of byte
	lun		= cmd[1] >> 5;					// get the LUN from the command.
	
	// The following commands support LUN in command, check if it's valid
	// Note: INQUIRY also supports LUNs, but it should report in a different way...
	if( justCmd == SCSI_C_READ6				|| justCmd == SCSI_C_FORMAT_UNIT || 
		justCmd == SCSI_C_TEST_UNIT_READY	|| justCmd == SCSI_C_REQUEST_SENSE ) {

		if(lun != 0) {					// LUN must be 0
		    Return_LUNnotSupported(devIndex);
			return;
		}
	}

	//----------------
	// now to solve the not initialized device
	if(device[devIndex].IsInit != TRUE)
	{
		// for the next 3 commands the device is not ready
		if((justCmd == SCSI_C_FORMAT_UNIT) || (justCmd == SCSI_C_READ6) || (justCmd == SCSI_C_WRITE6))
		{
			ReturnStatusAccordingToIsInit(devIndex);
			return;	
		}
	}
	//----------------
	// if media changed, and the command is not INQUIRY and REQUEST SENSE
	if(device[devIndex].MediaChanged == TRUE)
	{
		if((justCmd != SCSI_C_INQUIRY) && (justCmd != SCSI_C_REQUEST_SENSE))
		{
			ReturnUnitAttention(devIndex);
			return;	
		}
	}
	//----------------
	switch(justCmd)			
	{
	case SCSI_C_SEND_DIAGNOSTIC:
	case SCSI_C_RESERVE:
	case SCSI_C_RELEASE:
	case SCSI_C_TEST_UNIT_READY:    ReturnStatusAccordingToIsInit(devIndex);    return;

	case SCSI_C_MODE_SENSE6:        SCSI_ModeSense6(devIndex);                  return;
	
	case SCSI_C_REQUEST_SENSE:      SCSI_RequestSense(devIndex);                return;
	case SCSI_C_INQUIRY:            SCSI_Inquiry(devIndex);                     return;

	case SCSI_C_FORMAT_UNIT:        SCSI_FormatUnit(devIndex);                  return;
	case SCSI_C_READ6:              SCSI_ReadWrite6(devIndex, TRUE);            return;
	case SCSI_C_WRITE6:             SCSI_ReadWrite6(devIndex, FALSE);           return;
		//----------------------------------------------------
	default: 
		{
		device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
		device[devIndex].SCSI_SK	= SCSI_E_IllegalRequest;
		device[devIndex].SCSI_ASC	= SCSI_ASC_INVALID_COMMAND_OPERATION_CODE;
		device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

//		showCommand(0xf0, 6, device[devIndex].LastStatus);
		
		PIO_read(device[devIndex].LastStatus);   // send status byte
		break;
		}
	}
}
//----------------------------------------------
void Return_LUNnotSupported(BYTE devIndex)
{
    device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
	device[devIndex].SCSI_SK	= SCSI_E_IllegalRequest;
	device[devIndex].SCSI_ASC	= SCSI_ASC_LU_NOT_SUPPORTED;
	device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

	PIO_read(device[devIndex].LastStatus);   // send status byte
}
//----------------------------------------------
void ReturnUnitAttention(BYTE devIndex)
{
	device[devIndex].MediaChanged = FALSE;
	
	device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
	device[devIndex].SCSI_SK	= SCSI_E_UnitAttention;
	device[devIndex].SCSI_ASC	= SCSI_ASC_NOT_READY_TO_READY_TRANSITION;
	device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

#ifdef WRITEOUT
	if(shitHasHappened)
		showCommand(1, 6, device[devIndex].LastStatus);
		
	shitHasHappened = 0;
#endif

	PIO_read(device[devIndex].LastStatus);   // send status byte
}
//----------------------------------------------
void ReturnStatusAccordingToIsInit(BYTE devIndex)
{
	if(device[devIndex].IsInit == TRUE)
		SendOKstatus(devIndex);
	else
	{
		device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
		device[devIndex].SCSI_SK	= SCSI_E_NotReady;
		device[devIndex].SCSI_ASC	= SCSI_ASC_MEDIUM_NOT_PRESENT;
		device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

#ifdef WRITEOUT
	if(shitHasHappened)
		showCommand(2, 6, device[devIndex].LastStatus);
		
	shitHasHappened = 0;
#endif

		PIO_read(device[devIndex].LastStatus);   // send status byte
	}
}
//----------------------------------------------
void SendOKstatus(BYTE devIndex)
{
	device[devIndex].LastStatus	= SCSI_ST_OK;
	device[devIndex].SCSI_SK	= SCSI_E_NoSense;
	device[devIndex].SCSI_ASC	= SCSI_ASC_NO_ADDITIONAL_SENSE;
	device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

#ifdef WRITEOUT
	if(shitHasHappened)
		showCommand(3, 6, device[devIndex].LastStatus);
		
	shitHasHappened = 0;
#endif

	PIO_read(device[devIndex].LastStatus);   // send status byte, long time-out 
}
//----------------------------------------------
BYTE SCSI_Read6_SDMMC(BYTE devIndex, DWORD sector, WORD lenX)
{
	BYTE res = 0;

	#define READ_SINGLE 
	//--------------------
	#ifndef READ_SINGLE
	
	if(lenX==1)
	{
		res = mmcRead(devIndex, sector);
	}
	else
	{
		res = mmcReadMore(devIndex, sector, lenX);
	}
	#endif
	//--------------------
	#ifdef READ_SINGLE
	WORD i;

	for(i=0; i<lenX; i++)					// all needed sectors
 	{
 		if(sector >= device[devIndex].SCapacity)
 			return 1;

		res = mmcRead(devIndex,sector);		// write

		if(res!=0)							// if error, then break
			break;
		
		sector++;							// next sector
	}
	#endif
/*	
	if(sector < 0xff)
		wait_ms(10);						
*/	
	return res;
}
//----------------------------------------------
BYTE SCSI_Write6_SDMMC(BYTE devIndex, DWORD sector, WORD lenX)
{
	WORD i;
	BYTE res = 0;

//#define WRITE_SINGLE		// ON for experimental, was OFF in v 1.00
	
#ifdef WRITE_SINGLE	//-----------------------------
	
 	for(i=0; i<lenX; i++)					// all needed sectors
 		{
 		if(sector >= device[devIndex].SCapacity)
 			return 1;

		res = mmcWrite(devIndex, sector);	// write
	
		if(res!=0)							// if error, then break
			break;
		
		sector++;							// next sector
		}
#else			//----------------------------------------

	if(lenX == 1)									// just 1 sector?
	{
 		if(sector >= device[devIndex].SCapacity)
 			return 1;

		res = mmcWrite(devIndex, sector);
	}
	else
	{
		res = mmcWriteMore(devIndex, sector, lenX);
	}
		
#endif		//----------------------------------------
		
	return res;
}
//----------------------------------------------
void SCSI_ReadWrite6(BYTE devIndex, BYTE Read)
{
	DWORD sector, sectorEnd;
	BYTE res = 0;
	WORD lenX;
 
	sector  = (cmd[1] & 0x1f);
	sector  = sector << 8;
	sector |= cmd[2];
	sector  = sector << 8;
	sector |= cmd[3];
 
	lenX = cmd[4];	   	 	   	  // get the # of sectors to read
 
	if(lenX==0)
		lenX=256;
	
	sectorEnd = sector + ((DWORD)lenX) - 1;
	
	// if we're trying to address a sector beyond the last one - error!
	if( sector >= device[devIndex].SCapacity || sectorEnd >= device[devIndex].SCapacity) {   
		device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
		device[devIndex].SCSI_SK	= SCSI_E_IllegalRequest;
		device[devIndex].SCSI_ASC	= SCSI_ASC_LBA_OUT_OF_RANGE;
		device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

		PIO_read(device[devIndex].LastStatus);    // send status byte, long time-out
		return;
	}	
	//--------------------------------
	if(Read==TRUE)				// if read
		res = SCSI_Read6_SDMMC(devIndex, sector, lenX);
	else						// if write
		res = SCSI_Write6_SDMMC(devIndex, sector, lenX);
	//--------------------------------
	if(res==0)			   							// if everything was OK
	{
		SendOKstatus(devIndex);
	}
	else 							   			   // if error 
	{
		device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
		device[devIndex].SCSI_SK		= SCSI_E_MediumError;
		device[devIndex].SCSI_ASC		= SCSI_ASC_NO_ADDITIONAL_SENSE;
		device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

#ifdef WRITEOUT
	if(shitHasHappened)
		showCommand(4, 6, device[devIndex].LastStatus);
		
	shitHasHappened = 0;
#endif

		PIO_read(device[devIndex].LastStatus);    // send status byte, long time-out
	}
}
//----------------------------------------------
void SCSI_FormatUnit(BYTE devIndex)
{
BYTE res = 0;

	res = EraseCard(devIndex);
	//---------------
	if(res==0)			   							// if everything was OK
	{
		wait_ms(1000);

		SendOKstatus(devIndex);
	}
	else 							   			   // if error 
	{
		device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
		device[devIndex].SCSI_SK	= SCSI_E_MediumError;
		device[devIndex].SCSI_ASC	= SCSI_ASC_NO_ADDITIONAL_SENSE;
		device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

#ifdef WRITEOUT
	if(shitHasHappened)
		showCommand(5, 6, device[devIndex].LastStatus);
		
	shitHasHappened = 0;
#endif

		PIO_read(device[devIndex].LastStatus);    // send status byte, long time-out
	}
}
//----------------------------------------------
void ClearTheUnitAttention(BYTE devIndex)
{
	device[devIndex].LastStatus	= SCSI_ST_OK;
	device[devIndex].SCSI_SK		= SCSI_E_NoSense;
	device[devIndex].SCSI_ASC		= SCSI_ASC_NO_ADDITIONAL_SENSE;
	device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

	device[devIndex].MediaChanged = FALSE;		
}
//----------------------------------------------
void SCSI_Inquiry(BYTE devIndex)
{
	WORD i,xx;
	BYTE val, lun, firstByte;
	
	BYTE vendor[8] = {"JOOKIE  "};
	
	lun = cmd[1] >> 5;					// get the LUN from the command.
	
	if(lun == 0) {						// for LUN 0
		firstByte = 0;
	} else {							// for other LUNs
		firstByte = 0x7f;
	}
	
	if(device[devIndex].MediaChanged == TRUE)                       // this command clears the unit attention state
		ClearTheUnitAttention(devIndex);

	if(cmd[1] & 0x01)                                               // EVPD bit is set? Request for vital data?
	{                                                               // vital data not suported
		device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
		device[devIndex].SCSI_SK	= SCSI_E_IllegalRequest;
		device[devIndex].SCSI_ASC	= SCSO_ASC_INVALID_FIELD_IN_CDB;
		device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;

		PIO_read(device[devIndex].LastStatus);    // send status byte, long time-out
		
		return;	
	}

	//-----------	
	xx = cmd[4];		  									// how many bytes should be sent

	PreDMA_read();

	for(i=0; i<xx; i++)			  
	{
		// first init the val to zero or space
		if(i >= 8 && i<=43) {           // if the returned byte is somewhere from ASCII part of data, init on 'space' character
		    val = ' ';
		} else {                        // for other locations init on ZERO
    		val = 0;
		}

		// then for the appropriate position return the right value
		if(i == 0) {					// PERIPHERAL QUALIFIER + PERIPHERAL DEVICE TYPE
			val = firstByte;			// depending on LUN number
		}
		
		if(i==1) {                      // 1st byte
			val = 0x80;                 // removable (RMB) bit set
		}
			
		if(i==2 || i==3) {              // 2nd || 3rd byte
			val = 0x02;                 // SCSI level || response data format
		}			
				
		if(i==4) {
			val = 0x27;                 // 4th byte = Additional length
		}
				
		if(i>=8 && i<=15) {             // send vendor (JOOKIE)
		    val = vendor[i-8];
		}
			
		if(i>=16 && i<=25) {            // send device name (UltraSatan)
		    val = InquiryName[i-16];
		}
		
		if(i == 27) {                   // send slot # (1 or 2)
			val = '1' + devIndex;
		}

		if(i>=32 && i<=35) {            // version string
		    val = VERSION_STRING_SHORT[i-32];
		}

		if(i>=36 && i<=43) {            // date string
		    val = DATE_STRING[i-36];
		}
		
		DMA_read(val);	
	
		if(brStat != E_OK)  // if something isn't OK
		{
		#ifdef DEBUG_OUT
 			fputs(str_e);
		#endif

			PostDMA_read();
			return;		  								// quit this
		}
	}	

	PostDMA_read();

    SendOKstatus(devIndex);
}
//----------------------------------------------
void SCSI_ModeSense6(BYTE devIndex)
{
	WORD length, i, len;
	BYTE PageCode, val;
	//-----------------
	BYTE page_control[]	= {0x0a, 0x06, 0, 0, 0, 0, 0, 0};
	BYTE page_medium[]	= {0x0b, 0x06, 0, 0, 0, 0, 0, 0};
	//-----------------
	PageCode	= cmd[2] & 0x3f;	// get only page code
	length		= cmd[4];		  		// how many bytes should be sent

	//-----------------
	// page not supported?
	if(PageCode != 0x0a && PageCode != 0x0b && PageCode != 0x3f)	
	{
		device[devIndex].LastStatus	= SCSI_ST_CHECK_CONDITION;
		device[devIndex].SCSI_SK	= SCSI_E_IllegalRequest;
		device[devIndex].SCSI_ASC	= SCSO_ASC_INVALID_FIELD_IN_CDB;
		device[devIndex].SCSI_ASCQ	= SCSI_ASCQ_NO_ADDITIONAL_SENSE;
		
		PIO_read(device[devIndex].LastStatus);    // send status byte, long time-out
		
		return;	
	}
	//-----------------
	// send the page
	
	PreDMA_read();

	switch(PageCode)
	{
		case 0x0a: 
		case 0x0b:	len = 8;			break;
		case 0x3f:	len = 8 + 8;	break;
		default:		len = 0;			break;
	}
	
	for(i=0; i<length; i++)			  
	{
		val = 0;									// send 0 by default?
		
		if(i==0)									// Mode parameter header - Mode data length?
			val = 3 + len;
			
		if(PageCode == 0x0a)			// should send control page?
		{
			if(i>=4 && i<=11)
				val = page_control[i - 4];
		}

		if(PageCode == 0x0b)			// should send medium page?
		{
			if(i>=4 && i<=11)
				val = page_medium[i - 4];
		}
		
		if(PageCode == 0x3f)			// should send all pages?
		{
			if(i>=4 && i<=11)
				val = page_control[i - 4];
				
			if(i>=12 && i<=19)
				val = page_medium[i - 12];
		}
		
		DMA_read(val);
	
		if(brStat != E_OK)		  			   		 		// if something isn't OK
		{
		 #ifdef DEBUG_OUT
 		 	fputs(str_e);
		 #endif

			PostDMA_read();
		   return;		  								// quit this
		}
	}

	PostDMA_read();
	SendOKstatus(devIndex);
}
//----------------------------------------------
// return the last error that occured
void SCSI_RequestSense(BYTE devIndex)
{
char i,xx; //, res;
unsigned char val;

if(device[devIndex].MediaChanged == TRUE)	// this command clears the unit attention state
	ClearTheUnitAttention(devIndex);

xx = cmd[4];		  // how many bytes should be sent

PreDMA_read();

for(i=0; i<xx; i++)			  
	{
	switch(i)
		{
		case  0:	val = 0xf0;							break;		// error code 
		case  2:	val = device[devIndex].SCSI_SK;		break;		// sense key 
		case  7:	val = xx-7;							break;		// AS length
		case 12:	val = device[devIndex].SCSI_ASC;	break;		// additional sense code
		case 13:	val = device[devIndex].SCSI_ASCQ;	break;		// additional sense code qualifier

		default:	val = 0; 			   				break;
		}
		
	DMA_read(val);
	
	if(brStat != E_OK)		  			   		 		// if something isn't OK
		{
		 #ifdef DEBUG_OUT
 		 	fputs(str_e);
		 #endif

		PostDMA_read();
	    return;		  								// quit this
		}
	}

PostDMA_read();
SendOKstatus(devIndex);
}
//----------------------------------------------
void SendEmptySecotrs(WORD sectors)
{
	WORD i,j;
	BYTE r1;
	
	PreDMA_read();

	for(j=0; j<sectors; j++)
		{
		for(i=0; i<512; i++)
			{
			r1 = DMA_read(0);		

			if(brStat != E_OK)		  						// if something was wrong
				{
				uart_outhexD(i);
				uart_putchar('\n');

				PostDMA_read();
				return;
				}
			}
		}
	
	PostDMA_read();
}
//----------------------------------------------
void showCommand(WORD id, WORD length, WORD errCode)
{
	uart_putchar('\n');

	uart_outhexB(id);
	uart_putchar(32);
	uart_putchar('-');
	uart_putchar(32);
	
	WORD i;
			
	for(i=0; i<length; i++)
	{
		uart_outhexB(cmd[i]);
		uart_putchar(32);
	}

	
	uart_putchar('-');
	uart_putchar(32);
	uart_outhexB(errCode);
}
//----------------------------------------------
