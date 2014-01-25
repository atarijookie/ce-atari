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

#include "extern_vars.h"
 
BYTE getNextDTAsFromHost(void);
DWORD copyNextDtaToAtari(void);

/* ------------------------------------------------------------------ */
/* the custom GEMDOS handlers now follow */

int32_t custom_dgetdrv( void *sp )
{
	DWORD res;

	if(!isOurDrive(currentDrive, 0)) {									/* if the current drive is not our drive */
		CALL_OLD_GD_NORET(Dgetdrv);
	
		currentDrive = res;												/* store the current drive */
		return res;
	}
	
	commandShort[4] = GEMDOS_Dgetdrv;										/* store GEMDOS function number */
	commandShort[5] = 0;										
	
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD_NORET(Dgetdrv);

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
        // note: even though we now know that this drive is not ours, we will let the host know that we've changed the drive
        commandShort[4] = GEMDOS_Dsetdrv;										/* store GEMDOS function number */
        commandShort[5] = (BYTE) drive;											/* store drive number */
        acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);	/* send command to host over ACSI */

        CALL_OLD_GD_NORET(Dsetdrv, drive);
	
		res = res | ceDrives;											/* mounted drives = original drives + ceDrives */	
		return res;
	}

    // in this case the drive is ours, now we even care for the result
    commandShort[4] = GEMDOS_Dsetdrv;										/* store GEMDOS function number */
    commandShort[5] = (BYTE) drive;											/* store drive number */
    res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD_NORET(Dsetdrv, drive);

		res = res | ceDrives;											/* mounted drives = original drives + ceDrives */	
		return res;														/* return the value returned from old handler */
	}
	
	WORD drivesMap = Drvmap();											/* BIOS call - get drives bitmap - this will also communicate with CE */
	
	return drivesMap;													/* result = original + my drives bitmap */
}

int32_t custom_dfree( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	BYTE *pDiskInfo	= (BYTE *)	*((DWORD *) params);
	params += 4;
	WORD drive		= (WORD)	*((WORD *)  params);
	
	if(!isOurDrive(drive, 1)) {											/* not our drive? */
		CALL_OLD_GD( Dfree, pDiskInfo, drive);
	}
	
	commandShort[4] = GEMDOS_Dfree;											/* store GEMDOS function number */
	commandShort[5] = drive;									

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD( Dfree, pDiskInfo, drive);
	}

	memcpy(pDiskInfo, pDmaBuffer, 16);									/* copy in the results */
    return extendByteToDword(res);
}

int32_t custom_dcreate( void *sp )
{
	DWORD res;
    char *pPath	= (char *) *((DWORD *) sp);

	WORD drive = getDriveFromPath((char *) pPath);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		CALL_OLD_GD( Dcreate, pPath);
	}
	
	commandShort[4] = GEMDOS_Dcreate;										/* store GEMDOS function number */
	commandShort[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD( Dcreate, pPath);
	}

    return extendByteToDword(res);
}

int32_t custom_ddelete( void *sp )
{
	DWORD res;
    char *pPath	= (char *) *((DWORD *) sp);
	
	WORD drive = getDriveFromPath((char *) pPath);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		CALL_OLD_GD( Ddelete, pPath);
	}
	
	commandShort[4] = GEMDOS_Ddelete;										/* store GEMDOS function number */
	commandShort[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD(Ddelete, pPath);
	}

    return extendByteToDword(res);
}

int32_t custom_fdelete( void *sp )
{
	DWORD res;
    char *pPath	= (char *) *((DWORD *) sp);
	
	WORD drive = getDriveFromPath((char *) pPath);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		CALL_OLD_GD( Fdelete, pPath);
	}
	
	commandShort[4] = GEMDOS_Fdelete;										/* store GEMDOS function number */
	commandShort[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD( Fdelete, pPath);
	}

    return extendByteToDword(res);
}

int32_t custom_dsetpath( void *sp )
{
	DWORD res;
    char *pPath	= (char *) *((DWORD *) sp);
	
	WORD drive = getDriveFromPath((char *) pPath);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		CALL_OLD_GD( Dsetpath, pPath);
	}
	
	commandShort[4] = GEMDOS_Dsetpath;										/* store GEMDOS function number */
	commandShort[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		/* copy in the path */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD( Dsetpath, pPath);
	}

    return extendByteToDword(res);
}

int32_t custom_dgetpath( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

    char *buffer	= (char *)	*((DWORD *) params);
	params += 4;
	WORD drive		= (WORD)	*((WORD *)  params);
	
	if(!isOurDrive(drive, 1)) {											/* not our drive? */
		CALL_OLD_GD( Dgetpath, buffer, drive);
	}
	
	commandShort[4] = GEMDOS_Dgetpath;										/* store GEMDOS function number */
	commandShort[5] = drive;									

	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD( Dgetpath, buffer, drive);
	}

	strncpy((char *)buffer, (char *)pDmaBuffer, DMA_BUFFER_SIZE);		/* copy in the results */
    return extendByteToDword(res);
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
		CALL_OLD_GD( Frename, 0, oldName, newName);
	}
	
	commandShort[4] = GEMDOS_Frename;										/* store GEMDOS function number */
	commandShort[5] = 0;									

	memset(pDmaBuffer, 0, DMA_BUFFER_SIZE);
	strncpy((char *) pDmaBuffer, oldName, (DMA_BUFFER_SIZE / 2) - 2);	/* copy in the old name	*/
	
	int oldLen = strlen((char *) pDmaBuffer);							/* get the length of old name */
	
	char *pDmaNewName = ((char *) pDmaBuffer) + oldLen + 1;
	strncpy(pDmaNewName, newName, (DMA_BUFFER_SIZE / 2) - 2);			/* copy in the new name	*/

	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD( Frename, 0, oldName, newName);
	}

    return extendByteToDword(res);
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
		CALL_OLD_GD( Fattrib, fileName, flag, attr);
	}
	
	commandShort[4] = GEMDOS_Fattrib;										/* store GEMDOS function number */
	commandShort[5] = 0;									

	memset(pDmaBuffer, 0, DMA_BUFFER_SIZE);
	
	pDmaBuffer[0] = (BYTE) flag;										/* store set / get flag */
	pDmaBuffer[1] = (BYTE) attr;										/* store attributes */
	
	strncpy(((char *) pDmaBuffer) + 2, fileName, DMA_BUFFER_SIZE -1 );	/* copy in the file name */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD( Fattrib, fileName, flag, attr);
	}

    return extendByteToDword(res);
}
/* ------------------------------------------------------------------ */
