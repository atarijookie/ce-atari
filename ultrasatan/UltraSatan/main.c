//-------------------------------------------------------------------
/*
	UltraSatan firmware by Jookie
	-----------------------------
	
Go to Project -> Project options and use these project options:
	
Project -> Target -> Type: Loader file

Load -> Boot mode   : SPI
        Boot format : binary
        Output width: 8 bits
*/
//-------------------------------------------------------------------
#include <stdio.h>
#include <cdefBF531.h>
#include <sys\exception.h>

#include "dataflash.h"
#include "serial.h"
#include "mmc.h"
#include "spi.h"
#include "bridge.h"
#include "scsi6.h"
#include "scsiDefs.h"
#include "scsi_icd.h"
#include "rtc.h"

#include "main.h"

#include "mydefines.h"
//-----------------------------------------------
TDevice device[MAX_DEVICES];

BYTE SectorBufer[2*512];

BYTE cmd[14];										// received command bytes
BYTE len;											// length of received command
BYTE isICD;											// a flag - is the received command in ICD format? 
BYTE brStat;										// status from bridge
volatile DWORD timeval;		

char *DeviceType[5] = {"nothing", "MMC", "SD", "SDHC"};

extern void TestDRQ(void);
void DisplayINT(void);

BYTE DMA_readC(BYTE byte);

BYTE InquiryName[11];

extern BYTE shitHasHappened;

// this log buffer contains 32 last commands and their return codes, each command and status is 16 bytes long
BYTE logBuffer[512];	// place to store the commands
WORD logIndex;			// index in the logBuffer (0 .. 31), 
BYTE logCount;			// counter of all logged commands (0 .. 63)

void addToLogBuffer(BYTE devIndex);
//-----------------------------------------------
void main(void)
{
	BYTE res;
	BYTE id;
	BYTE AllAreInit;
	DWORD dRes, i, j;
	WORD wSignals, wPrev, wVal;
	BYTE bVal, pbVal = 0;
	WORD stat;
	//------------
	*pFIO_DIR = (SPI_CS_PINS | aIRQ | aDRQ | RTC_SCL);		// set these PF as outputs
	*pFIO_FLAG_S = aDRQ | aIRQ;
	
	*pFIO_INEN = CARD_CHANGE;
	
	spiCShigh(0xff);																			// all CS high
	
	//------------
	SetupClocks();		// this is done in init block
	uart_init();
	//------------
	timeval = 0;
	
	uart_putchar('\n');
	uart_prints(VERSION_STRING);
	uart_putchar('\n');
	
	//-----------------------------------------------
	// clear log buffer
	for(i=0; i<512; i++) {
		logBuffer[i] = 0;
	}
	
	logIndex = 0;
	logCount = 0;
	//-----------------------------------------------
	// initialize the Async Mem register - where some Read-Only pins are located
	// to read the pins, read from pASYNCMEM
	*pEBIU_AMGCTL		= 0x00fe;					// CLKOUT disable, all banks (0,1,2,3) enabled 

	*pEBIU_AMBCTL0	= 0x11141114;
	*pEBIU_AMBCTL1	= 0x11141114;
	//-----------------------------------------------
	
	rtc_GetClock(NULL);
	//----------------
	// now enable all the things needed for card change detection
	*pFIO_EDGE		= CARD_CHANGE;				// edge sensitivity of PF
	*pFIO_BOTH		= CARD_CHANGE;				// do interrupt on both edges
	//----------------
	// BOTH card LEDs ON for 0.5 s
	spiCSlow(0);
	spiCSlow(1);
	
	wait_ms(500);						
	
	spiCShigh(0xff);
	wait_ms(200);						
	//----------------
	mmcInit();
	//----------------------------
	// initialize the device structures
	uart_prints("\nReading config: ");
	
	res = Config_Read(0);
	
	if(!res)
	{
		uart_prints("failed. Using default config.");
		Config_SetDefault();
	}
	else
		uart_prints("OK");
	//------------
	// copy in the default INQUIRY name
	copyn(InquiryName, "UltraSatan", 10);									
	InquiryName[10] = 0;

	ReadInquiryName();			// try to read the Inquiry name
	//----------------------------
	ClearAllStorage();

	InitStorage();
	ListStorage();
	//----------------------------
	// after the start of UltraSatan no device is in media change state
	for(i=0; i<MAX_DEVICES; i++)
		device[i].MediaChanged = FALSE;
	//---------------------------------------
	uart_prints("\nMainLoop:\n");
	//---------------
while(1)
{
  len   = 6;
	isICD = FALSE;
	
	cmd[0] = GetCmdByte();							// try to get byte from ST (waiting for the 1st byte)

	//---------------------
	if(brStat == E_RESET)								// if the reset is LOW (ST is in reset)
	{																		// clear all MediaChanged flags
		for(i=0; i<MAX_DEVICES; i++)			
			device[i].MediaChanged = FALSE;
	}	
	//---------------------
	// now check for media change and if occured, then initialize...
	if(brStat == E_CARDCHANGE)					// if CARD CHANGE occured
	{
		*pFIO_FLAG_C = CARD_CHANGE;				// clear interrupt flag
		
		InitStorage();										// init and list storage
		ListStorage();
		
		continue;													// try to get command byte
	}	
	//---------------------	
	if( brStat != E_OK_A1)							// not good 1st CMD byte?
	{
//		uart_prints("e1\n");
		continue;
	}
	//---------------------
	id = (cmd[0] >> 5) & 0x07;				// get only device ID
	res = 0xff;
		
	for(i=0; i<MAX_DEVICES; i++)			// check if we got that device
		if(id == device[i].ACSI_ID)		
		{
			res = i;											// got it, store it's the field index
			break;
		}
	
	//---------------------
	if(res == 0xff)										// we don't have that device
	{
//		uart_prints("e2\n");
		continue;
	}		
	//---------------------
	// This code is a workaround for the 'No drives shown in TOS when no card inserted'
	// The UltraSatan will not respond to READ command when card not initialized, but
	// will respond to other commands - this means that the TOS will not 'hang' and the
	// HDDRIVER will see the other empty card slot when card not inserted.
	if(device[res].IsInit != TRUE)		// the device is not init? 
	{
		BYTE justCmd;
		
		justCmd = cmd[0] & 0x1f;				// get only the command part of byte
		
		if(justCmd == SCSI_C_READ6)			// if the command is a READ command, don't answer
			continue;
	}
	//---------------------
	for(i=1; i<len; i++)							// receive the next command bytes
	{
		cmd[i] = PIO_write();						// drop down IRQ, get byte

		if(brStat!=E_OK)								// if something was wrong
		{
//			uart_prints("e3");
			
			showCommand(0xe3, i, i);
			uart_prints("\n");
			
			break;						  
		}

		if(i==1)												// if we got also the 2nd byte
			SetUpCmd();										// we set up the length of command, etc.
	}   	  	  
		
	if(brStat!=E_OK)					  			// if something was wrong
	{
//		uart_prints("e4\n");
		continue;						  					// try again
	}
	//---------------------
	shitHasHappened = 0;
		
	if(isICD==TRUE)						  			// if it's a ICD command
		ProcICD(res);						  
	else								  						// if it's a normal command
		ProcSCSI6(res);
		
	addToLogBuffer(res);							// store this cmd in the buffer
		
	if(shitHasHappened)
	{
		uart_prints("Unhandled shit!\n");
		shitHasHappened = 0;
	}
 }
}
//-----------------------------------------------
void addToLogBuffer(BYTE devIndex)
{
	BYTE special = 0, i;
	
	// first prepare the 0th byte, which is special:
	// bit 7: 0 - it's a command, 1 - it's other log info
	// bit 6: devIndex 0 or 1
	// bits 5..0: logCount - a counter of how many times we started to log from index 0
	if(devIndex != 0) {
		special |= (1 << 6);			// bit 6: devIndex 
	}
	
	special |= logCount;				// store logCount
	logBuffer[logIndex++] = special;	// pos 0: special
	//-------
	for(i=0; i<11; i++) {				// pos 1 - 11: cmd
		logBuffer[logIndex++] = cmd[i];
	}
	
	logBuffer[logIndex++] = device[devIndex].LastStatus;		// pos 12: status
	logBuffer[logIndex++] = device[devIndex].SCSI_SK;			// pos 13: SK
	logBuffer[logIndex++] =	device[devIndex].SCSI_ASC;			// pos 14: ASC
	logBuffer[logIndex++] = device[devIndex].SCSI_ASCQ;			// pos 15: ASCQ

	if(logIndex >= 512) {				// end of log buffer?
		logIndex = 0;					// reset buffer index
		
		logCount++;						// increment overflow / overlap counter
		logCount = logCount & 0x3f;		// keep only 6 lowest bits
	}
}
//-----------------------------------------------
void ClearAllStorage(void)
{
	BYTE i;
	
	for(i=0; i<MAX_DEVICES; i++)
	{
		device[i].IsInit			= FALSE;
		device[i].Type				= DEVICETYPE_NOTHING;
		device[i].InitRetries	= 5;
		
		device[i].MediaChanged = FALSE;
	}
}
//-----------------------------------------------
void InitStorage(void)
{
	WORD i;
	BYTE res, AllAreInit, stat;
	DWORD dRes;
	
	*pFIO_FLAG_C = CARD_CHANGE;		// clear interrupt flag

	uart_prints("\nInit: ");
	
	spiSetSPCKfreq(SPI_FREQ_SLOW);												// low SPI frequency
	//--------------------------
	// remove all not present devices
	for(i=0; i<MAX_DEVICES; i++)
	{
		if(IsCardInserted(i) == FALSE)											// card not present, then remove
		{
			device[i].IsInit				= FALSE;											
			device[i].Type					= DEVICETYPE_NOTHING;
			device[i].MediaChanged	= FALSE;
		}

		device[i].InitRetries	= 5;
	}
	//--------------------------
	// and now try to init all that is not initialized
	while(1)
 	{
		//---------------
		// check if all present and not failed to initialize devices are initialized
		AllAreInit = TRUE;
		
		for(i=0; i<MAX_DEVICES; i++)
		{
			if(device[i].IsInit == TRUE)			// initialized?
				continue;												// skip it!

			if(device[i].InitRetries == 0)		// if initialization retries reached the end
				continue;
			
			res = IsCardInserted(i);					// is it inserted?
				
			if(res == FALSE)									// no inserted - skip it
					continue;

			AllAreInit = FALSE;								// at least one is not initialized
			break;									
		}		
		//---------------
		// everything initialized?
		if(AllAreInit == TRUE)							
			break;
		//---------------
		// initialize all the devices!
		for(i=0; i<MAX_DEVICES; i++)
		{
			if(device[i].IsInit == TRUE)			// initialized?
				continue;												// skip it!
		
			if(device[i].InitRetries == 0)		// if initialization retries reached the end
				continue;
				
			res = IsCardInserted(i);					// is it inserted?
				
			if(res == FALSE)									// if not inserted
			{
				device[i].InitRetries--;
				continue;
			}
					
			res = mmcReset(i);			  				// init the MMC card
				
			if(res == DEVICETYPE_NOTHING)			// if failed to init
			{
				device[i].InitRetries--;
				continue;
			}
					
			//-------------
			// now to test if we can read from the card
			stat = mmcReadJustForTest(i, 0);		

			if(stat)
			{
				uart_prints2(i, "read failed\n");
				device[i].InitRetries--;
				
				continue;
			}						
			//-------------
			// get card capacity in blocks 
			if(res == DEVICETYPE_SDHC)
				dRes = SDHC_Capacity(i);
			else
				dRes = MMC_Capacity(i);				
				
			if(dRes == 0xffffffff)				// failed?
			{
				uart_prints2(i, "failed\n");
				device[i].InitRetries--;
			}
			else
			{
				device[i].Type			= res;				// store device type
				device[i].SCapacity = dRes;				// store capacity in sectors
				device[i].BCapacity = dRes << 9;	// store capacity in bytes
				device[i].IsInit 		= TRUE;				// set the flag
				
				device[i].MediaChanged = TRUE;
				
				uart_outDec1(i);
			}
 		}

		uart_prints(".");
		wait_ms(50);
	}
	
	*pFIO_FLAG_C = CARD_CHANGE;		// clear interrupt flag
	spiSetSPCKfreq(SPI_FREQ_22MHZ);	
}
//----------------------------	
void ListStorage(void)
{
	BYTE res, i;
	
	// count the devices
	uart_prints("\n\nDevices: \n");

	res = 0;
	
	for(i=0; i<MAX_DEVICES; i++)
	{
		if(device[i].IsInit != TRUE)			// not initialized?
			continue;												// skip it!

		uart_prints("[");
		uart_outDec1(i);
		uart_prints("] ID=");
		uart_outDec1(device[i].ACSI_ID);
		uart_prints(", TYPE=");
		uart_prints(DeviceType[device[i].Type]);
		uart_prints(", Cap.=");
		fputD(device[i].SCapacity >> 11, NULL);
		uart_prints("MB (");
		uart_outhexD(device[i].SCapacity);
		uart_prints(" s)\n");					

		res++;
	}		
	
	if(res == 0)
		uart_prints("(nothing)\n");
}
//----------------------------	
void copyn(BYTE *dest, BYTE *src, WORD ncount)
{
WORD i;

for(i=0; i<ncount; i++)
	dest[i] = src[i];
}
//--------------------------------------------
void DumpBuffer(void)
{
	WORD i, j;
	BYTE a;

	uart_prints("\nDump:");
	
	for(i=0; i<512; i += 24)
	{
		uart_putchar('\n');
		
		for(j=0; j<24; j++)
		{
			if(i + j > 511)
			{
				uart_putchar(32);
				uart_putchar(32);
			}
			else
				uart_outhexB(SectorBufer[i + j]);
				
			uart_putchar(32);
		}

		uart_prints(" | ");

		for(j=0; j<24; j++)
		{

			a = SectorBufer[i + j];
	
			if(i + j > 511)
				continue;
			
			if(a<33 || a>127)
				uart_putchar('.');
			else
				uart_putc(SectorBufer[i + j]);
		}
	}

	uart_putchar('\n');
}
//--------------------------------------------
BYTE IsCardInserted(BYTE which)
{
	WORD a = 0;
	
	a = *pASYNCMEM;
	
	if(which == 0)
		a = a & CARD_INS0;
	else
		a = a & CARD_INS1;
		
	if(a==0)
		return TRUE;
	else
		return FALSE;
}
//--------------------------------------------
void SetUpCmd(void)
{	
	// now it's time to set up the receiver buffer and length
	if((cmd[0] & 0x1f)==0x1f)			  // if the command is '0x1f'
	{
		isICD = TRUE;					  				// then it's a ICD command
					
		switch((cmd[1] & 0xe0)>>5)	 		// get the length of the command
		{
			case  0: len =  7; break;
			case  1: len = 11; break;
			case  2: len = 11; break;
			case  5: len = 13; break;
			default: len =  7; break;
		}
	}
	else 			 	 	   		 						// if it isn't a ICD command
	{
		isICD = FALSE;
		len   = 6;	  									// then length is 6 bytes 
	}
}
//---------------------------------
void wait_ms(DWORD ms)
{
	TimeOut_Start_ms(ms);
	
	while(!TimeOut_DidHappen());	
}
//--------------------------------------------
void wait_us(DWORD us)
{
	TimeOut_Start_us(us);
	
	while(!TimeOut_DidHappen());	
}
//--------------------------------------------
BYTE TimeOut_DidHappen(void)
{
	BYTE did;
	
	did = (*pTIMER_STATUS) & 0x0001;		// get TIMER0 IRQ flag
	
	if(did)															// if TIMER0 IRQ flag is set, disable timer
		*pTIMER_DISABLE	= 0x0001;					// disable TIMER0
		
	return did;
}
//--------------------------------------------
void Timer0_start(void)
{
 	*pTIMER_DISABLE		= 0x0007;					// disable all TIMERs
 	
	*pTIMER0_CONFIG		= 0x0059;					
	*pTIMER0_PERIOD		= 0x7fffffff;			
  *pTIMER_STATUS		= 0x7077;					//  W1C to all interrupt STATUS flags
  
	*pTIMER_ENABLE		= 0x0001;					//  enable TIMER0
}
//--------------------------------------------
void TimeOut_Stop(void)
{
	*pTIMER_DISABLE		= 0x0001;					// disable TIMER0
}
//--------------------------------------------
void TimeOut_Start_ms(DWORD ms)
{
	DWORD period;
	
	period = ms * 133333;								// calculate the period of timer
	
 	*pTIMER_DISABLE		= 0x0007;					// disable all TIMERs
 	
	*pTIMER0_CONFIG		= 0x0059;					
	*pTIMER0_PERIOD		= period;			
  *pTIMER_STATUS		= 0x7077;					//  W1C to all interrupt STATUS flags
  
	*pTIMER_ENABLE		= 0x0001;					//  enable TIMER0
}
//--------------------------------------------
void TimeOut_Start_us(DWORD us)
{
	DWORD period;
	
	period = us * 133;									// calculate the period of timer
	
 	*pTIMER_DISABLE		= 0x0007;					// disable all TIMERs
 	
	*pTIMER0_CONFIG		= 0x0059;					
	*pTIMER0_PERIOD		= period;			
  *pTIMER_STATUS		= 0x7077;					//  W1C to all interrupt STATUS flags
  
	*pTIMER_ENABLE		= 0x0001;					//  enable TIMER0
}
//--------------------------------------------
BYTE cmpn(BYTE *one, BYTE *two, WORD len)
{
	WORD i;
	
	for(i=0; i<len; i++)						// compare two fields
		if(one[i] != two[i])
			return 1;										// difference found
	
	return 0;												// fields match
}
//--------------------------------------------
void DisplayINT(void)
{
	WORD stat;
	
	stat = *pFIO_FLAG_D;
	stat = stat & aIRQ;
	
	if(stat == aIRQ)
		uart_putc('I');
	else
		uart_putc('i');
}
//--------------------------------------------
BYTE HexToByte(BYTE a, BYTE b)
{
	BYTE result;
	
	//--------------------
	if(a>='0' && a<='9')				// if it's a number
		a = a - '0';

	if(a>='A' && a<='F')				// if it's a capital character
		a = a - 'A' + 10;
		
	if(a>='a' && a<='f')				// if it's a small character
		a = a - 'a' + 10;
		
	if(a>15)										// if not converted yet, it's a shit
		a = 0;
	//--------------------
	if(b>='0' && b<='9')				// if it's a number
		b = b - '0';

	if(b>='A' && b<='F')				// if it's a capital character
		b = b - 'A' + 10;
		
	if(b>='a' && b<='f')				// if it's a small character
		b = b - 'a' + 10;
		
	if(b>15)										// if not converted yet, it's a shit
		b = 0;
	//--------------------
	result = (a<<4) | b;				// construct the byte

	return result;
}
//--------------------------------------------
BYTE Config_Read(BYTE DontInit)
{
	BYTE fw, res;
	WORD i;
	BYTE *buffer;
	
	buffer = &SectorBufer[0];
	//-----------	
	res = Flash_GetIsReady();														// check if dataflash is ready
	
	if(!res)
		return 0;
	//-----------	
	res = Flash_ReadPage(2020, buffer);									// read the page with settings

	if(!res)																						// if failed to read flash page
		return 0;
	//-----------	
  if(buffer[0]>4)                       							// if firmware to boot is bad?
		return 0;
	//-----------	
	// check if settings are OK
  for(i=0; i<2; i++)
  {
    if(buffer[2 + i] > 7)														// bad ACSI id?
			return 0;
  }
	//-----------	
  // some ACSI IDs are the same?
  if(buffer[2] == buffer[3])	
  	return 0;
	//-----------	
	// read the settings into structures
  for(i=0; i<2; i++)
  {
    device[i].ACSI_ID			= buffer[2 + i];
    
    if(DontInit)					// if shouldn't init, skip init
    	continue;
    	
		device[i].InitRetries	= 5;
		device[i].IsInit			= FALSE;
  }
	//-----------	
  
	return 1;
}
//--------------------------------------------
void Config_SetDefault(void)
{
	BYTE fw, res;
	WORD i;
	BYTE *buffer;
	
	buffer = &SectorBufer[0];
	//-----------	
	res = Flash_GetIsReady();														// check if dataflash is ready
	
	if(!res)
		return;
	//-----------	
	// set default config to structure
  for(i=0; i<2; i++)
  {
		device[i].InitRetries	= 5;
    device[i].ACSI_ID			= i;
    device[i].IsInit			= FALSE;
  }

	//-----------	
	// write default config to flash
	buffer[0] = 0;													
	buffer[1] = 0;
	
  for(i=0; i<2; i++)
  {
	  buffer[2 + i] = device[i].ACSI_ID;
  }

	Flash_WritePage(2020, buffer);
}
//--------------------------------------------
BYTE Config_GetBootBase(void)
{
	WORD val;
	
	val = *pASYNCMEM;
	
	if(val & 0x0080)
		return 0;
	
	return 1;	
}
//--------------------------------------------
void memset(BYTE *what, BYTE value, WORD count)
{
	WORD i;
	
	for(i=0; i<count; i++)
	{
		*what = value;
		what ++;
	}
}
//--------------------------------------------
BYTE DMA_readC(BYTE byte)
{
	WORD PFs;
	WORD ctrls;
	DWORD timeout = 0x17D78400;
	
PFs = *pFIO_FLAG_D;
PFs = PFs & (0x00fc | aIRQ);			// remove-data-and-drq mask (leave the IRQ)
PFs = PFs | (((WORD)byte)	<< 8);	// shift data up
	
*pFIO_FLAG_D = PFs;

	//----------
while(timeout)
{
	timeout--;

	ctrls = *pASYNCMEM;			// read ACSI control signals

	if((ctrls & (aRESET | aACK)) == aRESET)	// does the result match aRESET H and rest L?
		break;
}

if(!timeout)
	return 0;

*pFIO_FLAG_S = aDRQ;	// DRQ to H 
	//----------
while(timeout)
{
	timeout--;

	ctrls = *pASYNCMEM;			// read ACSI control signals

	if((ctrls & (aRESET | aACK)) == (aRESET | aACK))
		break;
}

if(!timeout)
	return 1;

return 2;
}
//--------------------------------------------
void ReadInquiryName(void)
{
	BYTE res;
	WORD i;
	BYTE *buffer;
	
	buffer = &SectorBufer[0];
	//-----------	
	res = Flash_GetIsReady();														// check if dataflash is ready
	
	if(!res)
		return;
	//-----------	
	res = Flash_ReadPage(2019, buffer);									// read the page with settings

	if(!res)																						// if failed to read flash page
		return;
	//-----------	
	if(buffer[0] == 0x00 || buffer[0] == 0xff)					// no name present?
	{
		copyn(InquiryName, "UltraSatan", 10);							// copy in the default INQUIRY name
	}
	else
	{
		copyn(InquiryName, buffer, 10);										// copy in the INQUIRY name
	}

	InquiryName[10] = 0;																// terminate the string
	
	uart_prints("\nINQUIRY name: ");
	uart_prints((char *) InquiryName);
	uart_prints("\n");
}
//--------------------------------------------

