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

extern BYTE *pDta;
extern BYTE tempDta[45];

extern WORD dtaCurrent, dtaTotal;
extern BYTE dtaBuffer[DTA_BUFFER_SIZE + 2];
extern BYTE *pDtaBuffer;
extern BYTE fsnextIsForUs, tryToGetMoreDTAs;

BYTE getNextDTAsFromHost(void);
DWORD copyNextDtaToAtari(void);

extern WORD ceDrives;
extern WORD ceMediach;
extern BYTE currentDrive;

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
        acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);	/* send command to host over ACSI */

        CALL_OLD_GD_NORET(Dsetdrv, drive);
	
		res = res | ceDrives;											/* mounted drives = original drives + ceDrives */	
		return res;
	}

    // in this case the drive is ours, now we even care for the result
    commandShort[4] = GEMDOS_Dsetdrv;										/* store GEMDOS function number */
    commandShort[5] = (BYTE) drive;											/* store drive number */
    res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

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

/* **************************************************************** */
/* those next functions are used for file / dir search */

int32_t custom_fsetdta( void *sp )
{
    pDta = (BYTE *) *((DWORD *) sp);									/* store the new DTA pointer */

    useOldGDHandler = 1;
    Fsetdta( (_DTA *) pDta );
    useOldGDHandler = 0;

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
	
		CALL_OLD_GD( Fsfirst, fspec, attribs);
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

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		fsnextIsForUs = FALSE;
	
		CALL_OLD_GD( Fsfirst, fspec, attribs);
	}
	
	if(res != E_OK) {													/* if some other error, just return it */
        return extendByteToDword(res);									/* but append lots of FFs to make negative integer out of it */
	}

	res = copyNextDtaToAtari();											/* now copy the next possible DTA to atari DTA buffer */
    return extendByteToDword(res);
}

int32_t custom_fsnext( void *sp )
{
	DWORD res;

	if(!fsnextIsForUs) {												/* if we shouldn't handle this */
		CALL_OLD_GD( Fsnext);
	}

	res = copyNextDtaToAtari();											/* now copy the next possible DTA to atari DTA buffer */
	return res;
}

DWORD copyNextDtaToAtari(void)
{
	DWORD res;

	if(dtaCurrent >= dtaTotal) {										/* if we're out of buffered DTAs */
		if(!tryToGetMoreDTAs) {											/* if shouldn't try to get more DTAs, quit without trying */
            return extendByteToDword(ENMFIL);
		}
	
		res = getNextDTAsFromHost();									/* now we need to get the buffer of DTAs from host */
		
		if(res != E_OK) {												/* failed to get DTAs from host? */
			tryToGetMoreDTAs = 0;										/* do not try to receive more DTAs from host */
            return extendByteToDword(ENMFIL);								/* return that we're out of files */
		}
	}

	if(dtaCurrent >= dtaTotal) {										/* still no buffered DTAs? (this shouldn't happen) */
        return extendByteToDword(ENMFIL);									/* return that we're out of files */
	}

	DWORD dtaOffset		= 2 + (23 * dtaCurrent);						/* calculate the offset for the DTA in buffer */
	BYTE *pCurrentDta	= pDtaBuffer + dtaOffset;						/* and now calculate the new pointer */
	
	dtaCurrent++;														/* move to the next DTA */
		
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
	DWORD res;

	WORD handle = 0;
	BYTE *params = (BYTE *) sp;

	/* get params */
	char *fileName	= (char *)	*((DWORD *) params);
	params += 4;
	WORD attr		= (WORD)	*((WORD *)  params);
	
	WORD drive = getDriveFromPath((char *) fileName);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		CALL_OLD_GD( Fcreate, fileName, attr);
	}
	
	/* set the params to buffer */
	commandShort[4] = GEMDOS_Fcreate;										/* store GEMDOS function number */
	commandShort[5] = 0;			
	
	pDmaBuffer[0] = (BYTE) attr;										/* store attributes */
	strncpy(((char *) pDmaBuffer) + 1, fileName, DMA_BUFFER_SIZE - 1);	/* copy in the file name */
	
	handle = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);			/* send command to host over ACSI */

    if(handle == E_NOTHANDLED || handle == ACSIERROR) {						/* not handled or error? */
		CALL_OLD_GD( Fcreate, fileName, attr);
	}
	
	if(handle == ENHNDL || handle == EACCDN || handle == EINTRN) {		/* if some other error, just return it */
        return extendByteToDword(handle);									/* but append lots of FFs to make negative integer out of it */
	}
	
	handle = handleCEtoAtari(handle);									/* convert the CE handle (0 - 46) to Atari handle (150 - 200) */
	return handle;
}

int32_t custom_fopen( void *sp )
{
	DWORD res;

	WORD handle = 0;
	BYTE *params = (BYTE *) sp;

	/* get params */
	char *fileName	= (char *)	*((DWORD *) params);
	params += 4;
	WORD mode		= (WORD)	*((WORD *)  params);
	
	WORD drive = getDriveFromPath((char *) fileName);
	
	if(!isOurDrive(drive, 0)) {											/* not our drive? */
		CALL_OLD_GD( Fopen, fileName, mode);
	}
	
	/* set the params to buffer */
	commandShort[4] = GEMDOS_Fopen;											/* store GEMDOS function number */
	commandShort[5] = 0;			
	
	pDmaBuffer[0] = (BYTE) mode;										/* store attributes */
	strncpy(((char *) pDmaBuffer) + 1, fileName, DMA_BUFFER_SIZE - 1);	/* copy in the file name */
	
	handle = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);			/* send command to host over ACSI */

    if(handle == E_NOTHANDLED || handle == ACSIERROR) {						/* not handled or error? */
		CALL_OLD_GD( Fopen, fileName, mode);
	}
	
	if(handle == ENHNDL || handle == EACCDN || handle == EINTRN || handle == EFILNF) {		/* if some other error, just return it */
        return extendByteToDword(handle);								/* but append lots of FFs to make negative integer out of it */
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
		CALL_OLD_GD(Fclose, atariHandle);
	}
	
	WORD ceHandle = handleAtariToCE(atariHandle);						/* convert high atari handle to little CE handle */
	
	commitChanges(ceHandle);											/* flush write buffer if needed */
	initFileBuffer(ceHandle);											/* init the file buffer like it was never used */
	
	/* set the params to buffer */
	commandShort[4] = GEMDOS_Fclose;											/* store GEMDOS function number */
	commandShort[5] = (BYTE) ceHandle;			
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);				/* send command to host over ACSI */

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD( Fclose, atariHandle);
	}
	
    return extendByteToDword(res);
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
		CALL_OLD_GD( Fseek, offset, atariHandle, seekMode);
	}
	
	WORD ceHandle = handleAtariToCE(atariHandle);						/* convert high atari handle to little CE handle */
	
	commitChanges(ceHandle);											/* flush write buffer if needed */
	seekInFileBuffer(ceHandle, offset, seekMode);						// update the file buffer by seeking if possible
	
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

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD( Fseek, offset, atariHandle, seekMode);
	}
	
	if(res != E_OK) {													/* if some other error, make negative number out of it */
        return extendByteToDword(res);
	}
	
	/* If we got here, the seek was successful and now we need to return the position in the file. */

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
		CALL_OLD_GD( Fdatime, pDatetime, atariHandle, flag);
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
		
    if(	res == E_NOTHANDLED || res == ACSIERROR || res == EINTRN) {				/* not handled or error? */
        return extendByteToDword(EINTRN);										/* return internal error */
	}

    return extendByteToDword(EINTRN);											/* in other cases - Internal Error - this shouldn't happen */
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
	
	if(drive < 2) {														/* asking for drive A or B? not ours! */
		return FALSE;
	}
	
	updateCeDrives();													/* update ceDrives variable */
	
	if(ceDrives & (1 << drive)) {										/* is that bit set? */
		return TRUE;
	}

	return FALSE;
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

	gemdos_table[0x4b] = custom_pexec;
	gemdos_table[0x4c] = custom_pterm;
	gemdos_table[0000] = custom_pterm0;
	gemdos_table[0x31] = custom_ptermres;	

	gemdos_table[0x4e] = custom_fsfirst;
	gemdos_table[0x4f] = custom_fsnext;
	gemdos_table[0x56] = custom_frename;
	gemdos_table[0x57] = custom_fdatime;
	
	bios_table[0x07] = custom_getbpb;
	bios_table[0x09] = custom_mediach;
	bios_table[0x0a] = custom_drvmap; 
}
