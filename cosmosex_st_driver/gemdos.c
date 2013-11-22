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
#include "main.h"

/* 
 * CosmosEx GEMDOS driver by Jookie, 2013
 * GEMDOS hooks part (assembler and C) by MiKRO (Miro Kropacek), 2013
 */
 
/* ------------------------------------------------------------------ */
/* init and hooks part - MiKRO */
extern int16_t useOldHandler;											/* 0: use new handlers, 1: use old handlers */
extern int32_t (*gemdos_table[256])( void* sp );

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

// TODO: init ceDrives and currentDrive
// TODO: vymysliet ako CE oznami ze mu prisiel / odisiel drive 

extern WORD ceDrives;
extern BYTE currentDrive;
extern DWORD lastCeDrivesUpdate;

extern BYTE switchToSuper;

// The following macros are used to convert atari handle numbers which are WORDs
// to CosmosEx ex handle numbers, which are only BYTEs; and back.
// To mark the difference between normal Atari handle and handle which came 
// from CosmosEx I've added some offset to CosmosEx handles.

// CosmosEx file handle:              6 ...  46
// Atari handle for regular files:    0 ...  90
// Atari handle for CosmosEx files: 150 ... 200
#define handleIsFromCE(X)		(X >= 150 && X <= 200)
#define handleAtariToCE(X)		(X  - 150)
#define handleCEtoAtari(X)		(X  + 150)

// TODO: cosmosEx file handles don't need to start from 6, they will be translated anyway

/* ------------------------------------------------------------------ */
/* the custom GEMDOS handlers now follow */

int32_t custom_dgetdrv( void *sp )
{
	DWORD res;

	if(!isOurDrive(currentDrive, 0)) {									/* if the current drive is not our drive */
		useOldHandler = 1;												/* call the old handler */
		res = Dgetdrv();
		useOldHandler = 0;
	
		currentDrive = res;												/* store the current drive */
		return res;
	}
	
	commandShort[4] = GEMDOS_Dgetdrv;										/* store GEMDOS function number */
	commandShort[5] = 0;										
	
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Dgetdrv();
		useOldHandler = 0;

		currentDrive = res;												/* store the current drive */
		return res;														/* return the value returned from old handler */
	}

	currentDrive = res;													/* store the current drive */
	return res;															/* return the result */
}

int32_t custom_dsetdrv( void *sp )
{
	DWORD res;

	/* get the drive # from stack */
	WORD drive = (WORD) *((WORD *) sp);
	currentDrive = drive;												/* store the drive - GEMDOS seems to let you set even invalid drive */
	
	if(!isOurDrive(drive, 0)) {											/* if the drive is not our drive */
		useOldHandler = 1;												/* call the old handler */
		res = Dsetdrv(drive);
		useOldHandler = 0;
	
		res = res | ceDrives;											/* mounted drives = original drives + ceDrives */	
		return res;
	}

	commandShort[4] = GEMDOS_Dsetdrv;										/* store GEMDOS function number */
	commandShort[5] = (BYTE) drive;											/* store drive number */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Dsetdrv(drive);
		useOldHandler = 0;

		res = res | ceDrives;											/* mounted drives = original drives + ceDrives */	
		return res;														/* return the value returned from old handler */
	}
	
	// TODO: replace BIOS Drvmap too!
	
	WORD drivesMapOrig	= Drvmap();										/* BIOS call - get drives bitmap */
	WORD myDrivesMap	= (WORD) *pDmaBuffer;							/* read result, which is drives bitmap*/	
	
	return (drivesMapOrig | myDrivesMap);								/* result = original + my drives bitmap */
}

int32_t custom_dfree( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	BYTE *pDiskInfo	= (BYTE *)	*((DWORD *) params);
	params += 4;
	WORD drive		= (WORD)	*((WORD *)  params);
	
	if(!isOurDrive(drive, 1)) {											/* not our drive? */
		useOldHandler = 1;												/* call the old handler */
		res = Dfree(pDiskInfo, drive);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}
	
	commandShort[4] = GEMDOS_Dfree;											/* store GEMDOS function number */
	commandShort[5] = drive;									

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Dfree(pDiskInfo, drive);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	memcpy(pDiskInfo, pDmaBuffer, 16);									/* copy in the results */
	return res;
}

int32_t custom_dcreate( void *sp )
{
	DWORD res;
	BYTE *pPath	= (BYTE *) *((DWORD *) sp);

	WORD drive = getDriveFromPath((char *) pPath);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		useOldHandler = 1;												/* call the old handler */
		res = Dcreate(pPath);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}
	
	commandShort[4] = GEMDOS_Dcreate;										/* store GEMDOS function number */
	commandShort[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Dcreate(pPath);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;
}

int32_t custom_ddelete( void *sp )
{
	DWORD res;
	BYTE *pPath	= (BYTE *) *((DWORD *) sp);
	
	WORD drive = getDriveFromPath((char *) pPath);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		useOldHandler = 1;												/* call the old handler */
		res = Ddelete(pPath);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}
	
	commandShort[4] = GEMDOS_Ddelete;										/* store GEMDOS function number */
	commandShort[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Ddelete(pPath);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;
}

int32_t custom_fdelete( void *sp )
{
	DWORD res;
	BYTE *pPath	= (BYTE *) *((DWORD *) sp);
	
	WORD drive = getDriveFromPath((char *) pPath);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		useOldHandler = 1;												/* call the old handler */
		res = Fdelete(pPath);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}
	
	commandShort[4] = GEMDOS_Fdelete;										/* store GEMDOS function number */
	commandShort[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Fdelete(pPath);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;
}

int32_t custom_dsetpath( void *sp )
{
	DWORD res;
	BYTE *pPath	= (BYTE *) *((DWORD *) sp);
	
	WORD drive = getDriveFromPath((char *) pPath);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		useOldHandler = 1;												/* call the old handler */
		res = Dsetpath(pPath);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}
	
	commandShort[4] = GEMDOS_Dsetpath;										/* store GEMDOS function number */
	commandShort[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Dsetpath(pPath);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;
}

int32_t custom_dgetpath( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	BYTE *buffer	= (BYTE *)	*((DWORD *) params);
	params += 4;
	WORD drive		= (WORD)	*((WORD *)  params);
	
	if(!isOurDrive(drive, 1)) {											/* not our drive? */
		useOldHandler = 1;												/* call the old handler */
		res = Dgetpath(buffer, drive);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}
	
	commandShort[4] = GEMDOS_Dgetpath;										/* store GEMDOS function number */
	commandShort[5] = drive;									

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Dgetpath(buffer, drive);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	strncpy((char *)buffer, (char *)pDmaBuffer, DMA_BUFFER_SIZE);		/* copy in the results */
	return res;
}

int32_t custom_frename( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	params += 2;														/* skip reserved WORD */
	char *oldName	= (char *)	*((DWORD *) params);
	params += 4;
	char *newName	= (char *)	*((DWORD *) params);
	
	WORD drive = getDriveFromPath((char *) oldName);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		useOldHandler = 1;												/* call the old handler */
		res = Frename(0, oldName, newName);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}
	
	commandShort[4] = GEMDOS_Frename;										/* store GEMDOS function number */
	commandShort[5] = 0;									

	memset(pDmaBuffer, 0, DMA_BUFFER_SIZE);
	strncpy((char *) pDmaBuffer, oldName, (DMA_BUFFER_SIZE / 2) - 2);	/* copy in the old name	*/
	
	int oldLen = strlen((char *) pDmaBuffer);							/* get the length of old name */
	
	char *pDmaNewName = ((char *) pDmaBuffer) + oldLen + 1;
	strncpy(pDmaNewName, newName, (DMA_BUFFER_SIZE / 2) - 2);			/* copy in the new name	*/

	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Frename(0, oldName, newName);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;
}

int32_t custom_fattrib( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	char *fileName	= (char *)	*((DWORD *) params);
	params += 4;
	WORD flag		= (WORD)	*((WORD *)  params);
	params += 2;
	WORD attr		= (WORD)	*((WORD *)  params);
	
	WORD drive = getDriveFromPath((char *) fileName);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		useOldHandler = 1;												/* call the old handler */
		res = Fattrib(fileName, flag, attr);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}
	
	commandShort[4] = GEMDOS_Fattrib;										/* store GEMDOS function number */
	commandShort[5] = 0;									

	memset(pDmaBuffer, 0, DMA_BUFFER_SIZE);
	
	pDmaBuffer[0] = (BYTE) flag;										/* store set / get flag */
	pDmaBuffer[1] = (BYTE) attr;										/* store attributes */
	
	strncpy(((char *) pDmaBuffer) + 2, fileName, DMA_BUFFER_SIZE -1 );	/* copy in the file name */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Fattrib(fileName, flag, attr);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	return res;
}

/* **************************************************************** */
/* those next functions are used for file / dir search */

int32_t custom_fsetdta( void *sp )
{
	pDta = (_DTA *)	*((DWORD *) sp);									/* store the new DTA pointer */

	useOldHandler = 1;													/* call the old handler */
	Fsetdta(pDta);
	useOldHandler = 0;

	// TODO: on application start set the pointer to the default position (somewhere before the app args)
	
	return 0;
}

int32_t custom_fsfirst( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	/* get params */
	char *fspec		= (char *)	*((DWORD *) params);
	params += 4;
	WORD attribs	= (WORD)	*((WORD *)  params);
	
	WORD drive = getDriveFromPath((char *) fspec);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		fsnextIsForUs = FALSE;
	
		useOldHandler = 1;												/* call the old handler */
		res = Fsfirst(fspec, attribs);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}
	
	/* initialize internal variables */
	dtaCurrent			= 0;
	dtaTotal			= 0;
	tryToGetMoreDTAs	= 1;											/* mark that when we run out of DTAs in out buffer, then we should try to get more of them from host */
	fsnextIsForUs		= TRUE;
	
	/* set the params to buffer */
	commandShort[4] = GEMDOS_Fsfirst;										/* store GEMDOS function number */
	commandShort[5] = 0;			
	
	pDmaBuffer[0] = (BYTE) attribs;										/* store attributes */
	strncpy(((char *) pDmaBuffer) + 1, fspec, DMA_BUFFER_SIZE - 1);		/* copy in the file specification */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		fsnextIsForUs = FALSE;
	
		useOldHandler = 1;												/* call the old handler */
		res = Fsfirst(fspec, attribs);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}
	
	if(res != E_OK) {													/* if some other error, just return it */
		return (0xffffff00 | res);										/* but append lots of FFs to make negative integer out of it */
	}

	res = copyNextDtaToAtari();											/* now copy the next possible DTA to atari DTA buffer */
	return res;
}

int32_t custom_fsnext( void *sp )
{
	DWORD res;

	if(!fsnextIsForUs) {												/* if we shouldn't handle this */
		useOldHandler = 1;												/* call the old handler */
		res = Fsnext();
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}

	res = copyNextDtaToAtari();											/* now copy the next possible DTA to atari DTA buffer */
	return res;
}

DWORD copyNextDtaToAtari(void)
{
	DWORD res;

	if(dtaCurrent >= dtaTotal) {										/* if we're out of buffered DTAs */
		if(!tryToGetMoreDTAs) {											/* if shouldn't try to get more DTAs, quit without trying */
			return (0xffffff00 | ENMFIL);
		}
	
		res = getNextDTAsFromHost();									/* now we need to get the buffer of DTAs from host */
		
		if(res != E_OK) {												/* failed to get DTAs from host? */
			tryToGetMoreDTAs = 0;										/* do not try to receive more DTAs from host */
			return (0xffffff00 | ENMFIL);								/* return that we're out of files */
		}
	}

	if(dtaCurrent >= dtaTotal) {										/* still no buffered DTAs? (this shouldn't happen) */
		return (0xffffff00 | ENMFIL);									/* return that we're out of files */
	}

	DWORD dtaOffset		= 2 + (23 * dtaCurrent);						/* calculate the offset for the DTA in buffer */
	BYTE *pCurrentDta	= pDtaBuffer + dtaOffset;						/* and now calculate the new pointer */
		
	memcpy(pDta + 21, pCurrentDta, 23);									/* skip the reserved area of DTA and copy in the current DTA */
	return E_OK;														/* everything went well */
}

BYTE getNextDTAsFromHost(void)
{
	DWORD res;

	/* initialize the interal variables */
	dtaCurrent	= 0;
	dtaTotal	= 0;
	
	commandShort[4] = GEMDOS_Fsnext;											/* store GEMDOS function number */
	commandShort[5] = 1;														/* transfer single sector */
	
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDtaBuffer, 1);				/* send command to host over ACSI */
	
	if(res != E_OK) {													/* if failed to transfer data - no more files */
		return ENMFIL;
	}
	
	/* store the new total DTA number we have buffered */
	dtaTotal  = pDtaBuffer[0];
	dtaTotal  = dtaTotal << 8;
	dtaTotal |= pDtaBuffer[1];
	
	return E_OK;
}

/* **************************************************************** */
/* the following functions work with files and file handles */

int32_t custom_fcreate( void *sp )
{
	WORD handle = 0;
	BYTE *params = (BYTE *) sp;

	/* get params */
	char *fileName	= (char *)	*((DWORD *) params);
	params += 4;
	WORD attr		= (WORD)	*((WORD *)  params);
	
	WORD drive = getDriveFromPath((char *) fileName);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		useOldHandler = 1;												/* call the old handler */
		handle = Fcreate(fileName, attr);
		useOldHandler = 0;
		return handle;													/* return the value returned from old handler */
	}
	
	/* set the params to buffer */
	commandShort[4] = GEMDOS_Fcreate;										/* store GEMDOS function number */
	commandShort[5] = 0;			
	
	pDmaBuffer[0] = (BYTE) attr;										/* store attributes */
	strncpy(((char *) pDmaBuffer) + 1, fileName, DMA_BUFFER_SIZE - 1);	/* copy in the file name */
	
	handle = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);			/* send command to host over ACSI */

	if(handle == E_NOTHANDLED || handle == ERROR) {						/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		handle = Fcreate(fileName, attr);
		useOldHandler = 0;
		return handle;													/* return the value returned from old handler */
	}
	
	if(handle == ENHNDL || handle == EACCDN || handle == EINTRN) {		/* if some other error, just return it */
		return (0xffffff00 | handle);									/* but append lots of FFs to make negative integer out of it */
	}
	
	handle = handleCEtoAtari(handle);									/* convert the CE handle (0 - 46) to Atari handle (150 - 200) */
	return handle;
}

int32_t custom_fopen( void *sp )
{
	WORD handle = 0;
	BYTE *params = (BYTE *) sp;

	/* get params */
	char *fileName	= (char *)	*((DWORD *) params);
	params += 4;
	WORD mode		= (WORD)	*((WORD *)  params);
	
	WORD drive = getDriveFromPath((char *) fileName);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		useOldHandler = 1;												/* call the old handler */
		handle = Fopen(fileName, mode);
		useOldHandler = 0;
		return handle;													/* return the value returned from old handler */
	}
	
	/* set the params to buffer */
	commandShort[4] = GEMDOS_Fopen;											/* store GEMDOS function number */
	commandShort[5] = 0;			
	
	pDmaBuffer[0] = (BYTE) mode;										/* store attributes */
	strncpy(((char *) pDmaBuffer) + 1, fileName, DMA_BUFFER_SIZE - 1);	/* copy in the file name */
	
	handle = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);			/* send command to host over ACSI */

	if(handle == E_NOTHANDLED || handle == ERROR) {						/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		handle = Fopen(fileName, mode);
		useOldHandler = 0;
		return handle;													/* return the value returned from old handler */
	}
	
	if(handle == ENHNDL || handle == EACCDN || handle == EINTRN) {		/* if some other error, just return it */
		return (0xffffff00 | handle);									/* but append lots of FFs to make negative integer out of it */
	}
	
	handle = handleCEtoAtari(handle);									/* convert the CE handle (0 - 46) to Atari handle (150 - 200) */
	return handle;
}

int32_t custom_fclose( void *sp )
{
	DWORD res			= 0;
	WORD atariHandle	= (WORD) *((WORD *) sp);

	/* check if this handle should belong to cosmosEx */
	if(!handleIsFromCE(atariHandle)) {									/* not called with handle belonging to CosmosEx? */
		useOldHandler = 1;												/* call the old handler */
		res = Fclose(atariHandle);
		useOldHandler = 0;
		return res;
	}
	
	WORD ceHandle = handleAtariToCE(atariHandle);						/* convert high atari handle to little CE handle */
	
	/* set the params to buffer */
	commandShort[4] = GEMDOS_Fclose;											/* store GEMDOS function number */
	commandShort[5] = (BYTE) ceHandle;			
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Fclose(atariHandle);
		useOldHandler = 0;
		return res;													/* return the value returned from old handler */
	}
	
	return res;
}

int32_t custom_fread( void *sp )
{
	DWORD res = 0;
	WORD handle = 0;

	// insert code for func param retrieval
	
	if(!handleIsFromCE(handle)) {										/* not called with handle belonging to CosmosEx? */
		useOldHandler = 1;												/* call the old handler */
		// insert the code to call the original handler
		useOldHandler = 0;
		return res;
	}
	
	handle = handleAtariToCE(handle);									/* convert high atari handle to little CE handle */
	
	// insert the code for CosmosEx communication
	
	return res;
}

int32_t custom_fwrite( void *sp )
{
	DWORD res = 0;
	WORD handle = 0;

	// insert code for func param retrieval
	
	if(!handleIsFromCE(handle)) {										/* not called with handle belonging to CosmosEx? */
		useOldHandler = 1;												/* call the old handler */
		// insert the code to call the original handler
		useOldHandler = 0;
		return res;
	}
	
	handle = handleAtariToCE(handle);									/* convert high atari handle to little CE handle */
	
	// insert the code for CosmosEx communication
	
	return res;
}

int32_t custom_fseek( void *sp )
{
	DWORD res = 0;
	BYTE *params = (BYTE *) sp;

	/* get params */
	DWORD offset		= (DWORD)	*((DWORD *) params);
	params += 4;
	WORD atariHandle	= (WORD)	*((WORD *)  params);
	params += 2;
	WORD seekMode		= (WORD)	*((WORD *)  params);
	
	/* check if this handle should belong to cosmosEx */
	if(!handleIsFromCE(atariHandle)) {									/* not called with handle belonging to CosmosEx? */
		useOldHandler = 1;												/* call the old handler */
		res = Fseek(offset, atariHandle, seekMode);
		useOldHandler = 0;
		return res;
	}
	
	WORD ceHandle = handleAtariToCE(atariHandle);						/* convert high atari handle to little CE handle */
	
	/* set the params to buffer */
	commandLong[5] = GEMDOS_Fseek;											/* store GEMDOS function number */
	
	/* store params to command sequence */
	commandLong[6] = (BYTE) (offset >> 24);				
	commandLong[7] = (BYTE) (offset >> 16);
	commandLong[8] = (BYTE) (offset >>  8);
	commandLong[9] = (BYTE) (offset & 0xff);
	
	commandLong[10] = (BYTE) ceHandle;
	commandLong[11] = (BYTE) seekMode;
	
	res = acsi_cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuffer, 1);				/* send command to host over ACSI */

	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		useOldHandler = 1;												/* call the old handler */
		res = Fseek(offset, atariHandle, seekMode);
		useOldHandler = 0;
		return res;														/* return the value returned from old handler */
	}
	
	if(res != E_OK) {													/* if some other error, make negative number out of it */
		return (0xffffff00 | res);
	}
	
	/* If we got here, the seek was succesfull and now we need to return the position in the file. */

	/* construct the new file position from the received data */
	res  = pDmaBuffer[0];
	res  = res << 8;
	res |= pDmaBuffer[1];
	res  = res << 8;
	res |= pDmaBuffer[2];
	res  = res << 8;
	res |= pDmaBuffer[3];
	
	return res;
}

int32_t custom_fdatime( void *sp )
{
	DWORD res = 0;
	BYTE *params = (BYTE *) sp;

	/* get params */
	BYTE *pDatetime		= (BYTE *)		*((DWORD *) params);
	params += 4;
	WORD atariHandle	= (WORD)		*((WORD *)  params);
	params += 2;
	WORD flag			= (WORD)		*((WORD *)  params);
	
	/* check if this handle should belong to cosmosEx */
	if(!handleIsFromCE(atariHandle)) {										/* not called with handle belonging to CosmosEx? */
		useOldHandler = 1;													/* call the old handler */
		res = Fdatime(pDatetime, atariHandle, flag);
		useOldHandler = 0;
		return res;
	}
	
	WORD ceHandle = handleAtariToCE(atariHandle);							/* convert high atari handle to little CE handle */
	
	/* set the params to buffer */
	commandLong[5]  = GEMDOS_Fdatime;										/* store GEMDOS function number */
	commandLong[6]  = (flag << 7) | (ceHandle & 0x7f);						/* flag on highest bit, the rest is handle */
	
	/* store params to command sequence */
	commandLong[7]  = (BYTE) pDatetime[0];									/* store the current date time value */
	commandLong[8]  = (BYTE) pDatetime[1];
	commandLong[9]  = (BYTE) pDatetime[2];
	commandLong[10] = (BYTE) pDatetime[3];

	res = acsi_cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuffer, 1);	/* send command to host over ACSI */
	
	if(flag != 1) {															/* not FD_SET - get date time */
		memcpy(pDatetime, pDmaBuffer, 4);									/* copy in the results */
	}
	
	if(res == E_OK) {														/* good? ok */
		return E_OK;
	}
		
	if(	res == E_NOTHANDLED || res == ERROR || res == EINTRN) {				/* not handled or error? */
		return (0xffffff00 | EINTRN);										/* return internal error */
	}

	return (0xffffff00 | EINTRN);											/* in other cases - Internal Error - this shouldn't happen */
}

/* ------------------------------------------------------------------ */
/* helper functions */
WORD getDriveFromPath(char *path)
{
	if(strlen(path) < 3) {												/* if the path is too short to be full path, e.g. 'C:\DIR', just return currentDrive */
		return currentDrive;
	}
	
	if(path[1] != ':') {												/* if the path isn't full path, e.g. C:\, just return currentDrive */
		return currentDrive;
	}
	
	char letter = path[0];
	
	if(letter >= 'a' && letter <= 'z') {								/* small letter? */
		return (letter - 'a');
	}
	
	if(letter >= 'A' && letter <= 'Z') {								/* capital letter? */
		return (letter - 'A');
	}
	
	return currentDrive;												/* other case? return currentDrive */	
}

BYTE isOurDrive(WORD drive, BYTE withCurrentDrive) 
{
	if(withCurrentDrive) {												/* if the 0 in drive doesn't mean 'A', but 'current drive', then we have to figure out what the drive is */
		if(drive == 0) {												/* asking for current drive? */
			drive = currentDrive;
		} else {														/* asking for normal drive? */
			drive--;
		}
	}
	
	if(drive > 15) {													/* just in case somebody would ask for non-sense */
		return FALSE;
	}
	
	updateCeDrives();													/* update ceDrives variable */
	
	if(ceDrives & (1 << drive)) {										/* is that bit set? */
		return TRUE;
	}

	return FALSE;
}

/* this function updates the ceDrives variable from the status in host, 
but does this only once per 3 seconds, so you can call it often and 
it will quit then sooner withot updating (hoping that nothing changed within 3 seconds) */
void updateCeDrives(void)
{
	DWORD res;
	DWORD now = *HZ_200;

	if((lastCeDrivesUpdate - now) < 600) {								/* if the last update was less than 3 seconds ago, don't update */
		return;
	}
	
	lastCeDrivesUpdate = now;											/* mark that we've just updated the ceDrives */
	
	/* now do the real update */
	commandShort[4] = BIOS_Drvmap;											/* store BIOS function number */
	commandShort[5] = 0;										
	
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */
	
	if(res == E_NOTHANDLED || res == ERROR) {							/* not handled or error? */
		return;														
	}
	
	WORD drives = (WORD) *((WORD *) pDmaBuffer);						/* read drives from dma buffer */
	ceDrives = drives;	
}
/* ------------------------------------------------------------------ */
void initFunctionTable(void)
{
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
}
