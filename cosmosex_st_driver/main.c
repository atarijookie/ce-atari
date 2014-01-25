#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "xbra.h"
#include "acsi.h"
#include "translated.h"
#include "gemdos.h"
#include "main.h"

/* 
 * CosmosEx GEMDOS driver by Jookie, 2013
 * GEMDOS hooks part (assembler and C) by MiKRO (Miro Kropacek), 2013
 */
 
/* ------------------------------------------------------------------ */
/* init and hooks part - MiKRO */

#include "extern_vars.h"
#include "helpers.h"

int32_t (*gemdos_table[256])( void* sp ) = { 0 };
int16_t useOldGDHandler = 0;								/* 0: use new handlers, 1: use old handlers */

int32_t (*bios_table[256])( void* sp ) = { 0 };
int16_t useOldBiosHandler = 0;								/* 0: use new handlers, 1: use old handlers */ 
/* ------------------------------------------------------------------ */
/* CosmosEx and Gemdos part - Jookie */
BYTE ce_findId(void);
BYTE ce_identify(void);
void ce_initialize(void);

BYTE *getDmaBufferPointer(void);

BYTE commandShort[CMD_LENGTH_SHORT]	= {			0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};
BYTE commandLong[CMD_LENGTH_LONG]	= {0x1f,	0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0, 0, 0, 0, 0, 0, 0};

/* ------------------------------------------------------------------ */
int main( int argc, char* argv[] )
{
	BYTE found;
	int i;

	/* write some header out */
	(void) Clear_home();
	(void) Cconws("\33p[ CosmosEx disk driver ]\r\n[    by Jookie 2013    ]\33q\r\n\r\n");

	/* initialize internal stuff for Fsfirst and Fsnext */
	dtaCurrent			= 0;
	dtaTotal			= 0;
	fsnextIsForUs		= 0;
	tryToGetMoreDTAs	= 0;
	pDtaBuffer		= &dtaBuffer[2];
	pDtaBuffer		= (BYTE *) (((DWORD) pDtaBuffer) & 0xfffffffe);		/* remove odd bit if the address was odd */

	/* search for CosmosEx on ACSI bus */ 
	found = ce_findId();

	if(!found) {								/* not found? quit */
		sleep(3);
		return 0;
	}
	
	/* now set up the acsi command bytes so we don't have to deal with this one anymore */
	commandShort[0] = (getSetDeviceId(DEVICEID_GET) << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	
	commandLong[0] = (getSetDeviceId(DEVICEID_GET) << 5) | 0x1f;			/* cmd[0] = ACSI_id + ICD command marker (0x1f)	*/
	commandLong[1] = 0xA0;								/* cmd[1] = command length group (5 << 5) + TEST UNIT READY (0) */
	
	/* tell the device to initialize */
	Supexec(ce_initialize);
	
	/* now init our internal vars */
	getSetCurrentDrive(Dgetdrv());						/* get the current drive from system */
	
	Supexec(updateCeDrives);								/* update the ceDrives variable */
	
	initFunctionTable();

	for(i=0; i<MAX_FILES; i++) {
		initFileBuffer(i);									/* init the file buffers */
	}
	
	/* either remove the old one or do nothing, old memory isn't released */
	if( unhook_xbra( VEC_GEMDOS, XBRA_ID ) == 0L && unhook_xbra( VEC_BIOS, XBRA_ID ) == 0L ) {
		(void)Cconws( "\r\nDriver installed.\r\n" );
	} else {
		(void)Cconws( "\r\nDriver reinstalled, some memory was lost.\r\n" );
	}

	/* and now place the new gemdos handler */
	old_gemdos_handler	= Setexc( VEC_GEMDOS,	gemdos_handler );
	old_bios_handler	= Setexc( VEC_BIOS,		bios_handler ); 
	
	/* wait for a while so the user could read the message and quit */
	sleep(2);
	return 0;		/* make compiler happy, we wont return */
}

/* this function scans the ACSI bus for any active CosmosEx translated drive */
BYTE ce_findId(void)
{
	char bfr[2], res, i;

	bfr[1] = 0;
	getSetDeviceId(0);
	
	(void) Cconws("Looking for CosmosEx: ");

	for(i=0; i<8; i++) {
		bfr[0] = i + '0';
		(void) Cconws(bfr);

		getSetDeviceId(i);									/* store the tested ACSI ID */
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
  
	commandShort[0] = (getSetDeviceId(DEVICEID_GET) << 5); 											/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	commandShort[4] = TRAN_CMD_IDENTIFY;
  
	BYTE *pDmaBuff = getDmaBufferPointer();
	memset(pDmaBuff, 0, 512);              									/* clear the buffer */

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuff, 1);	/* issue the command and check the result */

	if(res != OK) {                        										/* if failed, return FALSE */
		return 0;
	}

	if(strncmp((char *) pDmaBuff, "CosmosEx translated disk", 24) != 0) {		/* the identity string doesn't match? */
		return 0;
	}
	
	return 1;                             										/* success */
}

/* send INITIALIZE command to the CosmosEx device telling it to do all the stuff it needs at start */
void ce_initialize(void)
{
	commandShort[0] = (getSetDeviceId(DEVICEID_GET) << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	commandShort[4] = GD_CUSTOM_initialize;
  
	BYTE *pDmaBuff = getDmaBufferPointer();
	acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuff, 1);			/* issue the command and check the result */
}

BYTE *getDmaBufferPointer(void)
{
	static BYTE dmaBuffer[DMA_BUFFER_SIZE + 2];				// buffer
	static BYTE *pDmaBuffer = 0;							// buffer pointer - first uninitialized

	if(pDmaBuffer == 0) {									// if buffer pointer not initialized, initialize it
		/* create buffer pointer to even address */
		pDmaBuffer = &dmaBuffer[2];
		pDmaBuffer = (BYTE *) (((DWORD) pDmaBuffer) & 0xfffffffe);		/* remove odd bit if the address was odd */
	}
	
	return pDmaBuffer;
}

BYTE getSetDeviceId(BYTE newId)
{
	static BYTE deviceID = 0;
	
	if(newId != DEVICEID_GET) {
		deviceID = newId;
	}
	
	return deviceID;
}

BYTE getSetCurrentDrive(BYTE newVal)
{
	static BYTE currentDrive = 0;
	
	if(newVal != GET_CURRENTDRIVE) {
		currentDrive = newVal;
	}

	return currentDrive;
}

BYTE *getSetPDta(BYTE *newVal) 
{
	static BYTE tempDta[45];
	static BYTE *pDta = 0;
	
	if(pDta == 0) {								// not initialized?
		pDta = (BYTE *) &tempDta[0];			// use this buffer as temporary one for DTA - just in case
	}
	
	if(newVal != PDTA_GET) {					// if not GET
		pDta = newVal;
	}
	
	return pDta;
}
