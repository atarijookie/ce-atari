#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xbra.h"
#include "acsi.h"
#include "translated.h"
#include "gemdos.h"
#include "main.h"

char *version = "2014-08-12";

/* 
 * CosmosEx GEMDOS driver by Jookie, 2013
 * GEMDOS hooks part (assembler and C) by MiKRO (Miro Kropacek), 2013
 */
 
/* ------------------------------------------------------------------ */
/* init and hooks part - MiKRO */
typedef void  (*TrapHandlerPointer)( void );

extern void gemdos_handler( void );
extern TrapHandlerPointer old_gemdos_handler;
int32_t (*gemdos_table[256])( void* sp ) = { 0 };
int16_t useOldGDHandler = 0;								/* 0: use new handlers, 1: use old handlers */

extern void bios_handler( void );
extern TrapHandlerPointer old_bios_handler;
int32_t (*bios_table[256])( void* sp ) = { 0 };
int16_t useOldBiosHandler = 0;								/* 0: use new handlers, 1: use old handlers */ 
/* ------------------------------------------------------------------ */
/* CosmosEx and Gemdos part - Jookie */
BYTE ce_findId(void);
BYTE ce_identify(void);
void ce_initialize(void);
void getConfig(void);
BYTE setDateTime(void);
void showDateTime(void);
void showInt(int value, int length);

void setBootDrive(void);

BYTE dmaBuffer[DMA_BUFFER_SIZE + 2];
BYTE *pDmaBuffer;

BYTE deviceID;

BYTE commandShort[CMD_LENGTH_SHORT]	= {			0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};
BYTE commandLong[CMD_LENGTH_LONG]	= {0x1f,	0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0};

BYTE *pDta;
BYTE tempDta[45];

WORD dtaCurrent, dtaTotal;
BYTE dtaBuffer[DTA_BUFFER_SIZE + 2];
BYTE *pDtaBuffer;
BYTE fsnextIsForUs, tryToGetMoreDTAs;

WORD ceDrives;
WORD ceMediach;
BYTE currentDrive;
WORD driveMap;
BYTE configDrive;

BYTE setDate;
int year, month, day, hours, minutes, seconds;

extern DWORD _runFromBootsector;			// flag meaning if we are running from TOS or bootsector
/* ------------------------------------------------------------------ */
int main( int argc, char* argv[] )
{
	BYTE found;
	int i;

	/* write some header out */
	(void) Clear_home();
	(void) Cconws("\33p[ CosmosEx disk driver  ]\r\n[ by Jookie 2013 & 2014 ]\r\n[        ver ");
    (void) Cconws(version);
    (void) Cconws(" ]\33q\r\n\r\n");

	BYTE kbshift = Kbshift(-1);
	
	if((kbshift & 0x0f) != 0 && kbshift != (K_CTRL | K_LSHIFT) ) {
		if((kbshift & K_ALT) != 0) {
			(void) Cconws("ALT ");
		}

		if((kbshift & (K_RSHIFT | K_LSHIFT)) != 0) {
			(void) Cconws("Shift ");
		}

		if((kbshift & K_CTRL) != 0) {
			(void) Cconws("CTRL ");
		}

		(void) Cconws("key pressed, skipping!\r\n" );
		sleep(2);
		return 0;
	}
	
	/* create buffer pointer to even address */
	pDmaBuffer = &dmaBuffer[2];
	pDmaBuffer = (BYTE *) (((DWORD) pDmaBuffer) & 0xfffffffe);		/* remove odd bit if the address was odd */

	/* initialize internal stuff for Fsfirst and Fsnext */
	dtaCurrent			= 0;
	dtaTotal			= 0;
	fsnextIsForUs		= 0;
	tryToGetMoreDTAs	= 0;
	pDtaBuffer		= &dtaBuffer[2];
	pDtaBuffer		= (BYTE *) (((DWORD) pDtaBuffer) & 0xfffffffe);		/* remove odd bit if the address was odd */

	// search for CosmosEx on ACSI bus
	found = ce_findId();

	if(!found) {								        // not found? quit
		sleep(3);
		return 0;
	}
    
	/* now set up the acsi command bytes so we don't have to deal with this one anymore */
	commandShort[0] = (deviceID << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	
	commandLong[0] = (deviceID << 5) | 0x1f;			/* cmd[0] = ACSI_id + ICD command marker (0x1f)	*/
	commandLong[1] = 0xA0;								/* cmd[1] = command length group (5 << 5) + TEST UNIT READY (0) */
	
	/* tell the device to initialize */
	Supexec(ce_initialize);
	
	/* now init our internal vars */
	pDta				= (BYTE *) &tempDta[0];				/* use this buffer as temporary one for DTA - just in case */

	currentDrive		= Dgetdrv();						/* get the current drive from system */
	
	driveMap	        = Drvmap();						    /* get the pre-installation drive map */

    Supexec(getConfig);                                     // get translated disk configuration, including date and time
    
    if(setDate) {                                           // now if we should set new date/time, then set it
        setDateTime();
        showDateTime();
    }
    
	Supexec(updateCeDrives);								/* update the ceDrives variable */
	
	initFunctionTable();

	for(i=0; i<MAX_FILES; i++) {
		initFileBuffer(i);									/* init the file buffers */
	}
	
	/* either remove the old one or do nothing, old memory isn't released */
	if( unhook_xbra( VEC_GEMDOS, 'CEDD' ) == 0L && unhook_xbra( VEC_BIOS, 'CEDD' ) == 0L ) {
		(void)Cconws( "\r\nDriver installed.\r\n" );
	} else {
		(void)Cconws( "\r\nDriver reinstalled, some memory was lost.\r\n" );
	}

	/* and now place the new gemdos handler */
	old_gemdos_handler	= Setexc( VEC_GEMDOS,	gemdos_handler );
	old_bios_handler	= Setexc( VEC_BIOS,		bios_handler ); 

    /* only force boot drive if not cancelled by LSHIFT+CTRL */
    if( kbshift != (K_CTRL | K_LSHIFT) ){
    	Supexec(setBootDrive);								/* set the boot drive */
    }

	/* wait for a while so the user could read the message and quit */
	sleep(2);

	if(_runFromBootsector == 0) {	// if the prg was not run from boot sector, terminate and stay resident (execution ends here)
		Ptermres( 0x100 + _base->p_tlen + _base->p_dlen + _base->p_blen, 0 );
	}

	// if the prg was run from bootsector, we will return and the asm code will do rts
	return 0;		
}

/* this function scans the ACSI bus for any active CosmosEx translated drive */
BYTE ce_findId(void)
{
	char bfr[2], res, i;

	bfr[1] = 0;
	deviceID = 0;
	
	(void) Cconws("Looking for CosmosEx: ");

	for(i=0; i<8; i++) {
		bfr[0] = i + '0';
		(void) Cconws(bfr);

		deviceID = i;									/* store the tested ACSI ID */
		res = Supexec(ce_identify);  					/* try to read the IDENTITY string */
		
		if(res == 1) {                           		/* if found the CosmosEx */
			(void) Cconws("\r\nCosmosEx found on ACSI ID: ");
			bfr[0] = i + '0';
			(void) Cconws(bfr);

			return 1;
		}
	}

	/* if not found */
    (void) Cconws("\r\nCosmosEx not found on ACSI bus, not installing driver.");
	return 0;
}

/* send an IDENTIFY command to specified ACSI ID and check if the result is as expected */
BYTE ce_identify(void)
{
	WORD res;
  
	commandShort[0] = (deviceID << 5); 											/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	commandShort[4] = TRAN_CMD_IDENTIFY;
  
	memset(pDmaBuffer, 0, 512);              									/* clear the buffer */

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);	/* issue the command and check the result */

	if(res != OK) {                        										/* if failed, return FALSE */
		return 0;
	}

	if(strncmp((char *) pDmaBuffer, "CosmosEx translated disk", 24) != 0) {		/* the identity string doesn't match? */
		return 0;
	}
	
	return 1;                             										/* success */
}

/* send INITIALIZE command to the CosmosEx device telling it to do all the stuff it needs at start */
void ce_initialize(void)
{
	commandShort[0] = (deviceID << 5); 					                        /* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	commandShort[4] = GD_CUSTOM_initialize;
  
	acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);			/* issue the command and check the result */
}

void getConfig(void)
{
    WORD res;
    
	commandShort[0] = (deviceID << 5); 					                        // cmd[0] = ACSI_id + TEST UNIT READY (0)
	commandShort[4] = GD_CUSTOM_getConfig;
  
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);	// issue the command and check the result
    
    if(res != OK) {                                                             // failed to get config?
        return;
    }
    
    configDrive = pDmaBuffer[4];                                                // store config drive letter for later use
    
    //------
    // get the date/time
    
    setDate = pDmaBuffer[5];
    
    year    = getWord(pDmaBuffer + 7);
    month   = pDmaBuffer[9];
    day     = pDmaBuffer[10];
    
    hours   = pDmaBuffer[11];
    minutes = pDmaBuffer[12];
    seconds = pDmaBuffer[13];
}

void setBootDrive(void)
{
    // are there any other drives installed besides A+B? don't change boot drive if other drives are present
    if( (driveMap & 0xfffc)==0 ) {              // no other drives detected
        
        // do we have this configDrive?
        if( isOurDrive(configDrive, 0) ){
		    (void)Cconws( "Setting boot drive to " );
            (void)Cconout(configDrive + 'A');
		    (void)Cconws( ".\r\n" );
            
            CALL_OLD_GD_VOIDRET(Dsetdrv, configDrive); 
        }
    }
}

BYTE setDateTime(void)
{
    WORD newDate, newTime;  
    WORD newYear, newMonth;
    WORD newHour, newMinute, newSecond;
    BYTE res;
    
    //------------------
    // set new date
    newYear = year - 1980;
    newYear = newYear << 9;
  
    newMonth = month;
    newMonth = newMonth << 5;
  
    newDate = newYear | newMonth | (day & 0x1f);
  
    res = Tsetdate(newDate);
  
    if(res)                   /* if some error, then failed */
        return 0;         
    
    //------------------
    // set new date
    newSecond   = ((seconds/2) & 0x1f);
    newMinute   = minutes & 0x3f;
    newMinute   = newMinute << 5;
    newHour     = hours & 0x1f;
    newHour     = newHour << 11;
  
    newTime = newHour | newMinute | newSecond;
  
    res = Tsettime(newTime);
  
    if(res)                   /* if some error, then failed */
        return 0;         
    //------------------
    
    return 1;
}

void showDateTime(void)
{
    (void) Cconws("Date: ");
    showInt(year, 4);
    (void) Cconout('-');
    showInt(month, 2);
    (void) Cconout('-');
    showInt(day, 2);
    (void) Cconws(" (YYYY-MM-DD)\n\r");

    (void) Cconws("Time:   ");
    showInt(hours, 2);
    (void) Cconout(':');
    showInt(minutes, 2);
    (void) Cconout(':');
    showInt(seconds, 2);
    (void) Cconws(" (HH:MM:SS)\n\r");
}

void showInt(int value, int length)
{
    char tmp[10];
    memset(tmp, 0, 10);
    
    int i;
    for(i=0; i<length; i++) {               // go through the int lenght and get the digits
        int val, mod;
        
        val = value / 10;
        mod = value % 10;
    
        tmp[length - 1 - i] = val + 48;     // store the current digit
        
        value = mod;
    }
    
    (void) Cconws(tmp);                     // write it out
}

