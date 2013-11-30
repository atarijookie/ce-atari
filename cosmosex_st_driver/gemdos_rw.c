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
#include "gemdos_errno.h"
#include "bios.h"
#include "main.h"

/* 
 * CosmosEx GEMDOS driver by Jookie, 2013
 * GEMDOS hooks part (assembler and C) by MiKRO (Miro Kropacek), 2013
 */
 
/* ------------------------------------------------------------------ */
/* init and hooks part - MiKRO */
extern int16_t useOldGDHandler;											/* 0: use new handlers, 1: use old handlers */
extern int16_t useOldBiosHandler;										/* 0: use new handlers, 1: use old handlers */ 

extern int32_t (*gemdos_table[256])( void* sp );
extern int32_t (  *bios_table[256])( void* sp );

/* ------------------------------------------------------------------ */
/* CosmosEx and Gemdos part - Jookie */

extern BYTE dmaBuffer[DMA_BUFFER_SIZE + 2];
extern BYTE *pDmaBuffer;

extern BYTE deviceID;
extern BYTE commandShort[CMD_LENGTH_SHORT];
extern BYTE commandLong[CMD_LENGTH_LONG];

extern _DTA *pDta;
extern BYTE tempDta[45];

extern WORD dtaCurrent, dtaTotal;
extern BYTE dtaBuffer[DTA_BUFFER_SIZE + 2];
extern BYTE *pDtaBuffer;
extern BYTE fsnextIsForUs, tryToGetMoreDTAs;

BYTE getNextDTAsFromHost(void);
DWORD copyNextDtaToAtari(void);

extern WORD ceDrives;
extern BYTE currentDrive;


/* ------------------------------------------------------------------ */
int32_t custom_fread( void *sp )
{
	DWORD res = 0;
	BYTE *params = (BYTE *) sp;

	WORD atariHandle	= (WORD)	*((WORD *) params);
	params += 2;
	DWORD length		= (DWORD)	*((DWORD *) params);
	params += 4;
	BYTE *buffer		= (BYTE *)	*((DWORD *) params);

	/* check if this handle should belong to cosmosEx */
	if(!handleIsFromCE(atariHandle)) {									/* not called with handle belonging to CosmosEx? */
		CALL_OLD_GD(Fread, atariHandle, length, buffer);
	}
	
	WORD ceHandle = handleAtariToCE(atariHandle);						/* convert high atari handle to little CE handle */
	
	/* set the params to buffer */
	commandLong[5] = GEMDOS_Fread;										/* store GEMDOS function number */

	
	res = acsi_cmd(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, pDmaBuffer, 1);	/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		CALL_OLD_GD(Fread, atariHandle, length, buffer);
	}
	
	return res;
}

int32_t custom_fwrite( void *sp )
{
	DWORD res = 0;
	WORD handle = 0;

	// insert code for func param retrieval
	
	if(!handleIsFromCE(handle)) {										/* not called with handle belonging to CosmosEx? */
//		CALL_OLD_GD(Fwrite, );
		return res;
	}
	
	handle = handleAtariToCE(handle);									/* convert high atari handle to little CE handle */
	
	// insert the code for CosmosEx communication
	
	return res;
}

/* ------------------------------------------------------------------ */
