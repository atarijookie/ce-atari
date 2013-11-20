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
extern int16_t useOldHandler;											/* 0: use new handlers, 1: use old handlers */
extern int32_t (*gemdos_table[256])( void* sp );

/* ------------------------------------------------------------------ */
/* CosmosEx and Gemdos part - Jookie */

extern BYTE dmaBuffer[DMA_BUFFER_SIZE + 2];
extern BYTE *pDmaBuffer;

extern BYTE deviceID;
extern BYTE command[6];

extern _DTA *pDta;
extern BYTE tempDta[45];

// TODO: init ceDrives and currentDrive
// TODO: vymysliet ako CE oznami ze mu prisiel / odisiel drive 

extern WORD ceDrives;
extern BYTE currentDrive;
extern DWORD lastCeDrivesUpdate;

extern BYTE switchToSuper;

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
	
	command[4] = GEMDOS_Dgetdrv;										/* store GEMDOS function number */
	command[5] = 0;										
	
	res = acsi_cmd(ACSI_READ, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

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

	command[4] = GEMDOS_Dsetdrv;										/* store GEMDOS function number */
	command[5] = (BYTE) drive;											/* store drive number */
	
	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

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
	
	command[4] = GEMDOS_Dfree;											/* store GEMDOS function number */
	command[5] = drive;									

	res = acsi_cmd(ACSI_READ, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

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
	
	command[4] = GEMDOS_Dcreate;										/* store GEMDOS function number */
	command[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

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
	
	command[4] = GEMDOS_Ddelete;										/* store GEMDOS function number */
	command[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

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
	
	command[4] = GEMDOS_Fdelete;										/* store GEMDOS function number */
	command[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

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
	
	command[4] = GEMDOS_Dsetpath;										/* store GEMDOS function number */
	command[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

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
	
	command[4] = GEMDOS_Dgetpath;										/* store GEMDOS function number */
	command[5] = drive;									

	res = acsi_cmd(ACSI_READ, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

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
	
	command[4] = GEMDOS_Frename;										/* store GEMDOS function number */
	command[5] = 0;									

	memset(pDmaBuffer, 0, DMA_BUFFER_SIZE);
	strncpy((char *) pDmaBuffer, oldName, (DMA_BUFFER_SIZE / 2) - 2);	/* copy in the old name	*/
	
	int oldLen = strlen((char *) pDmaBuffer);							/* get the length of old name */
	
	char *pDmaNewName = ((char *) pDmaBuffer) + oldLen + 1;
	strncpy(pDmaNewName, newName, (DMA_BUFFER_SIZE / 2) - 2);			/* copy in the new name	*/

	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

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
	
	command[4] = GEMDOS_Fattrib;										/* store GEMDOS function number */
	command[5] = 0;									

	memset(pDmaBuffer, 0, DMA_BUFFER_SIZE);
	
	pDmaBuffer[0] = (BYTE) flag;										/* store set / get flag */
	pDmaBuffer[1] = (BYTE) attr;										/* store attributes */
	
	strncpy(((char *) pDmaBuffer) + 2, fileName, DMA_BUFFER_SIZE -1 );	/* copy in the file name */
	
	res = acsi_cmd(ACSI_WRITE, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */

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
	/* not handled */
	return 0;
}

int32_t custom_fsnext( void *sp )
{
	/* not handled */
	return 0;
}

/* **************************************************************** */
/* the following functions work with files and file handles */

int32_t custom_fcreate( void *sp )
{
	/* not handled */
	return 0;
}

int32_t custom_fopen( void *sp )
{
	/* not handled */
	return 0;
}

int32_t custom_fclose( void *sp )
{
	/* not handled */
	return 0;
}

int32_t custom_fread( void *sp )
{
	/* not handled */
	return 0;
}

int32_t custom_fwrite( void *sp )
{
	/* not handled */
	return 0;
}

int32_t custom_fseek( void *sp )
{
	/* not handled */
	return 0;
}

int32_t custom_fdatime( void *sp )
{
	/* not handled */
	return 0;
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
	command[4] = BIOS_Drvmap;											/* store BIOS function number */
	command[5] = 0;										
	
	res = acsi_cmd(ACSI_READ, command, 6, pDmaBuffer, 1);				/* send command to host over ACSI */
	
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
