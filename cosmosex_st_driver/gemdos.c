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
int16_t gemdos_on = 0;	/* 0: use new handlers, 1: use old handlers */

/* ------------------------------------------------------------------ */
/* CosmosEx and Gemdos part - Jookie */
BYTE ce_findId(void);
BYTE ce_identify(BYTE ACSI_id);
void ce_initialize(void);

BYTE dmaBuffer[514];
BYTE *pDmaBuffer;

BYTE deviceID;
BYTE command[6] = {0, 'C', 'E', HOSTMOD_TRANSLATED_DISK, 0, 0};

_DTA *pDta;
BYTE tempDta[45];

BYTE switchToSuper;

#define CLEAR_HOME      "\33E"
#define Clear_home()    Cconws(CLEAR_HOME)

#define CALL_NOT_HANDLED	0
#define CALL_HANDLED		1
/* ------------------------------------------------------------------ */
/* the custom GEMDOS handlers now follow */

static int32_t custom_dgetdrv( void *sp )
{
	BYTE res;

	return CALL_NOT_HANDLED;

	command[4] = GEMDOS_Dgetdrv;						/* store GEMDOS function number */
	command[5] = 0;										
	
	res = acsi_cmd(1, command, 6, pDmaBuffer, 1);		/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {			/* not handled or error? */
		return CALL_NOT_HANDLED;
	}

	return res;											/* return the result */
}

static int32_t custom_dsetdrv( void *sp )
{
	BYTE res;

	return CALL_NOT_HANDLED;

	/* get the drive # from stack */
	WORD *pDrive = (WORD *) sp;
	WORD drive = *pDrive;
	
	if(drive > 15) {									/* probably invalid param */
		return CALL_NOT_HANDLED;
	}

	command[4] = GEMDOS_Dsetdrv;						/* store GEMDOS function number */
	command[5] = (BYTE) drive;							/* store drive number */
	
	res = acsi_cmd(1, command, 6, pDmaBuffer, 1);		/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {			/* not handled or error? */
		return CALL_NOT_HANDLED;
	}
	
	// TODO: replace BIOS Drvmap too!
	
	WORD drivesMapOrig	= Drvmap();						/* BIOS call - get drives bitmap */
	WORD myDrivesMap	= (WORD) *pDmaBuffer;			/* read result, which is drives bitmap*/	
	
	return (drivesMapOrig | myDrivesMap);				/* result = original + my drives bitmap */
}

static int32_t custom_fsetdta( void *sp )
{
	pDta = (_DTA *) sp;									/* just store the new DTA pointer and don't do anything */

	// TODO: on application start set the pointer to the default position (somewhere before the app args)
	
	return CALL_NOT_HANDLED;
}

static int32_t custom_dfree( void *sp )
{
	BYTE *params = (BYTE *) sp;
	BYTE res;

	BYTE *pDiskInfo	= (BYTE *)	*((DWORD *) params);
	params += 4;
	WORD drive		= (WORD)	*((WORD *)  params);
	
	command[4] = GEMDOS_Dfree;							/* store GEMDOS function number */
	command[5] = drive;									

/*	
	char bfr[256];
	sprintf( bfr, "dfree: drive %c, ptr: %x\r\n",  drive + 'A', (DWORD) pDiskInfo);
	(void)Cconws(bfr);
*/

	res = acsi_cmd(1, command, 6, pDmaBuffer, 1);		/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {			/* not handled or error? */
		return CALL_NOT_HANDLED;
	}

	memcpy(pDiskInfo, pDmaBuffer, 16);					/* copy in the results */
	return res;
}

static int32_t custom_dcreate( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_ddelete( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_dsetpath( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fcreate( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fopen( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fclose( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fread( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fwrite( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fdelete( void *sp )
{
	static char buff[256];
	/* pretend the params are array of strings */
	char** args = (char**)sp;
	
	gemdos_on = 1;
	Fdelete( args[0] );
	gemdos_on = 0;

#ifndef SLIM
	/* be nice on supervisor stack */
	sprintf( buff, "arg: %s\n", args[0] );
	(void)Cconws(buff);
#endif

	/* not handled */
	return 0;
}

static int32_t custom_fseek( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fattrib( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_dgetpath( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fsfirst( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fsnext( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_frename( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fdatime( void *sp )
{
	/* not handled */
	return 0;
}
/* ------------------------------------------------------------------ */
int main( int argc, char* argv[] )
{
	BYTE found;

	/* write some header out */
	(void) Clear_home();
	(void) Cconws("CosmosEx disk driver, by Jookie 2013\r\n");

	/* create buffer pointer to even address */
	pDmaBuffer = &dmaBuffer[2];
	pDmaBuffer = (BYTE *) (((DWORD) pDmaBuffer) & 0xfffffffe);		/* remove odd bit if the address was odd */

	switchToSuper = TRUE;
	
	/* search for CosmosEx on ACSI bus */ 
	found = ce_findId();

	if(!found) {								/* not found? quit */
		sleep(1);
//		return 0;
	}
	
	/* tell the device to initialize */
	ce_initialize();							
	
	pDta = (_DTA *) &tempDta[0];				/* use this buffer as temporary one for DTA - just in case */
	
	/* ----------------------------------------- */
	/* fill the table with pointers to functions */
	gemdos_table[0x0e] = custom_dsetdrv;
	gemdos_table[0x19] = custom_dgetdrv;
	gemdos_table[0x1a] = custom_fsetdta;
	gemdos_table[0x36] = custom_dfree;
	gemdos_table[0x39] = custom_dcreate;
	gemdos_table[0x3a] = custom_ddelete;
	gemdos_table[0x3b] = custom_dsetpath;
	gemdos_table[0x3c] = custom_fcreate;
	gemdos_table[0x3d] = custom_fopen;
	gemdos_table[0x3e] = custom_fclose;
	gemdos_table[0x3f] = custom_fread;
	gemdos_table[0x40] = custom_fwrite;
	gemdos_table[0x41] = custom_fdelete;
	gemdos_table[0x42] = custom_fseek;
	gemdos_table[0x43] = custom_fattrib;
	gemdos_table[0x47] = custom_dgetpath;
	gemdos_table[0x4e] = custom_fsfirst;
	gemdos_table[0x4f] = custom_fsnext;
	gemdos_table[0x56] = custom_frename;
	gemdos_table[0x57] = custom_fdatime;

	/* either remove the old one or do nothing, old memory isn't released */
	if( unhook_xbra( VEC_GEMDOS, 'CEDD' ) == 0L ) {
		(void)Cconws( "\r\nDriver installed.\r\n" );
	} else {
		(void)Cconws( "\r\nDriver reinstalled, some memory was lost.\r\n" );
	}

	/* and now place the new gemdos handler */
	old_gemdos_handler = Setexc( VEC_GEMDOS, gemdos_handler );
		
	switchToSuper = FALSE;
		
	/* wait for a while so the user could read the message and quit */
	sleep(1);
	
	/* now terminate and stay resident */
	Ptermres( 0x100 + _base->p_tlen + _base->p_dlen + _base->p_blen, 0 );
	
	return 0;		/* make compiler happy, we wont return */
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

		res = ce_identify(i);      					/* try to read the IDENTITY string */
		
		if(res == 1) {                           	/* if found the CosmosEx */
			deviceID = i;                     		/* store the ACSI ID of device */

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
BYTE ce_identify(BYTE ACSI_id)
{
	WORD res;
  
	command[0] = (ACSI_id << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	command[4] = TRAN_CMD_IDENTIFY;
  
	memset(pDmaBuffer, 0, 512);              		/* clear the buffer */

	res = acsi_cmd(1, command, 6, pDmaBuffer, 1);	/* issue the command and check the result */

	if(res != OK) {                        			/* if failed, return FALSE */
		return 0;
	}

	if(strncmp((char *) pDmaBuffer, "CosmosEx translated disk", 24) != 0) {		/* the identity string doesn't match? */
		return 0;
	}
	
	return 1;                             			/* success */
}

/* send INITIALIZE command to the CosmosEx device telling it to do all the stuff it needs at start */
void ce_initialize(void)
{
	command[0] = (deviceID << 5); 					/* cmd[0] = ACSI_id + TEST UNIT READY (0)	*/
	command[4] = GD_CUSTOM_initialize;
  
	acsi_cmd(1, command, 6, pDmaBuffer, 1);			/* issue the command and check the result */
}

