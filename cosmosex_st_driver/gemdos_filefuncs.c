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
#include "helpers.h"

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
	
	BYTE *pDmaBuff = getDmaBufferPointer();
	pDmaBuff[0] = (BYTE) attr;										/* store attributes */
	strncpy(((char *) pDmaBuff) + 1, fileName, DMA_BUFFER_SIZE - 1);	/* copy in the file name */
	
	handle = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuff, 1);			/* send command to host over ACSI */

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
	
	BYTE *pDmaBuff = getDmaBufferPointer();
	pDmaBuff[0] = (BYTE) mode;										/* store attributes */
	strncpy(((char *) pDmaBuff) + 1, fileName, DMA_BUFFER_SIZE - 1);	/* copy in the file name */
	
	handle = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuff, 1);			/* send command to host over ACSI */

    if(handle == E_NOTHANDLED || handle == ACSIERROR) {						/* not handled or error? */
		CALL_OLD_GD( Fopen, fileName, mode);
	}
	
	if(handle == ENHNDL || handle == EACCDN || handle == EINTRN) {		/* if some other error, just return it */
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
	
	BYTE *pDmaBuff = getDmaBufferPointer();
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuff, 1);				/* send command to host over ACSI */

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
	
	BYTE *pDmaBuff = getDmaBufferPointer();
	res = acsi_cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuff, 1);				/* send command to host over ACSI */

    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_GD( Fseek, offset, atariHandle, seekMode);
	}
	
	if(res != E_OK) {													/* if some other error, make negative number out of it */
        return extendByteToDword(res);
	}
	
	/* If we got here, the seek was successful and now we need to return the position in the file. */

	/* construct the new file position from the received data */
	res  = pDmaBuff[0];
	res  = res << 8;
	res |= pDmaBuff[1];
	res  = res << 8;
	res |= pDmaBuff[2];
	res  = res << 8;
	res |= pDmaBuff[3];
	
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

	BYTE *pDmaBuff = getDmaBufferPointer();
	res = acsi_cmd(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuff, 1);	/* send command to host over ACSI */
	
	if(flag != 1) {															/* not FD_SET - get date time */
		memcpy(pDatetime, pDmaBuff, 4);									/* copy in the results */
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
		return getSetCurrentDrive(GET_CURRENTDRIVE);
	}
	
	if(path[1] != ':') {												/* if the path isn't full path, e.g. C:\, just return currentDrive */
		return getSetCurrentDrive(GET_CURRENTDRIVE);
	}
	
	char letter = path[0];
	
	if(letter >= 'a' && letter <= 'z') {								/* small letter? */
		return (letter - 'a');
	}
	
	if(letter >= 'A' && letter <= 'Z') {								/* capital letter? */
		return (letter - 'A');
	}
	
	return getSetCurrentDrive(GET_CURRENTDRIVE);						/* other case? return currentDrive */	
}

BYTE isOurDrive(WORD drive, BYTE withCurrentDrive) 
{
	if(withCurrentDrive) {												/* if the 0 in drive doesn't mean 'A', but 'current drive', then we have to figure out what the drive is */
		if(drive == 0) {												/* asking for current drive? */
			drive = getSetCurrentDrive(GET_CURRENTDRIVE);
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
	
	if(getSetCeDrives(GET_CEDRIVES) & (1 << drive)) {										/* is that bit set? */
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
