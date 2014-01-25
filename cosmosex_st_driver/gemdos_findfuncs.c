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

BYTE getNextDTAsFromHost(void);
DWORD copyNextDtaToAtari(void);
 
BYTE fsnextIsForUs, tryToGetMoreDTAs;
WORD dtaCurrent, dtaTotal;

BYTE dtaBuffer[DTA_BUFFER_SIZE + 2];
BYTE *pDtaBuffer;

/* **************************************************************** */
/* those next functions are used for file / dir search */

int32_t custom_fsetdta( void *sp )
{
    BYTE *newPDta = (BYTE *) *((DWORD *) sp);									/* store the new DTA pointer */
	getSetPDta(newPDta);
	
    useOldGDHandler = 1;
    Fsetdta( (_DTA *) newPDta );
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

	BYTE *pDmaBuff = getDmaBufferPointer();	
	pDmaBuff[0] = (BYTE) attribs;										/* store attributes */
	strncpy(((char *) pDmaBuff) + 1, fspec, DMA_BUFFER_SIZE - 1);		/* copy in the file specification */
	
	res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuff, 1);				/* send command to host over ACSI */

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
		
	memcpy(getSetPDta(PDTA_GET) + 21, pCurrentDta, 23);									/* skip the reserved area of DTA and copy in the current DTA */
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

