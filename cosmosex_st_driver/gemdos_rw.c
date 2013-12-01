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

#define RW_BUFFER_SIZE		512

typedef struct 
{
	WORD rCount;
	BYTE rBuf[RW_BUFFER_SIZE];
	
	WORD wCount;
	BYTE wBuf[RW_BUFFER_SIZE];
	
} TFileBuffer;

TFileBuffer fileBufs[MAX_FILES];

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
	BYTE *params = (BYTE *) sp;

	WORD atariHandle	= (WORD)	*((WORD *) params);
	params += 2;
	DWORD count			= (DWORD)	*((DWORD *) params);
	params += 4;
	BYTE *buffer		= (BYTE *)	*((DWORD *) params);

	/* check if this handle should belong to cosmosEx */
	if(!handleIsFromCE(atariHandle)) {									/* not called with handle belonging to CosmosEx? */
		CALL_OLD_GD(Fwrite, atariHandle, count, buffer);
	}
	
	WORD ceHandle = handleAtariToCE(atariHandle);						/* convert high atari handle to little CE handle */
	
	TFileBuffer *fb = &fileBufs[ceHandle];								/* to shorten the following operations use this pointer */
	WORD spaceLeft = (RW_BUFFER_SIZE - fb->wCount);						/* how much space we have in the buffer left? */
	
	if(count <= RW_BUFFER_SIZE) {										/* if writing less than size of our buffer */
		if(count <= spaceLeft) {										/* the whole new data would fit in our buffer */
			memcpy(&fb->wBuf[ fb->wCount ], buffer, count);				/* copy in the data, update the data counter, and return the written count */
			fb->wCount += count;
			return count;
		} else {														/* the new data won't fit in the current (not empty) buffer */
			WORD firstPart	= spaceLeft;								/* store to buffer what we could store to make it full */
			WORD rest		= count - firstPart;						/* and calculate what will then stay after write in buffer as rest */

			memcpy(&fb->wBuf[ fb->wCount ], buffer, firstPart);			/* copy the 1st part to buffer, update data counter */
			fb->wCount += firstPart;
			
			commitChanges(ceHandle);									/* commit the current write buffer */
			
			memcpy(&fb->wBuf[0], buffer + firstPart, rest);				/* copy the rest to buffer, update data counter */
			fb->wCount = rest;
			
			return count;
		}		
	} else {															/* if writing more than size of our buffer */
		DWORD dwBuffer = (DWORD) buffer;
		BYTE  spaceLeftIsOdd = spaceLeft & 1;
		
		DWORD bytesWritten = 0;
		
		if((dwBuffer & 1) == 0) {										/* should write from EVEN address */
			if(spaceLeftIsOdd) {										/* and space left is odd number - make it also even */
				spaceLeft--;
			}		
		} else {														/* should write from ODD address */
			if(!spaceLeftIsOdd) {										/* and space left is even number - make it also ODD, so it would cancel out the ODD shit */
				if(spaceLeft > 0) {										/* can we make ODD number out of it? */
					spaceLeft--;
				} else {												/* if can't make ODD number out of 0 */
					commitChanges(ceHandle);							/* empty the buffer - now the spaceLeft should be 512 again */
					
					spaceLeft = (RW_BUFFER_SIZE - fb->wCount);			/* recalculate the space left variable */
					spaceLeft--;										/* and make ODD number out of it */
				}
			}
		}
	
		memcpy(&fb->wBuf[ fb->wCount ], buffer, spaceLeft);				/* copy some data to current buffer to use it as much as possible */
		fb->wCount	+= spaceLeft;										/* calculate the new data count */
		buffer		+= spaceLeft;										/* and calculate the new pointer to data, which should be now an EVEN number! */
	
		res = commitChanges(ceHandle);									/* empty the current buffer */

		if(!res) {														/* failed to write data? no data written yet */
			return 0;
		}
		
		bytesWritten += spaceLeft;										/* until now we've written this many bytes */
		/* --------------------- */
		/* at this point 'buffer' points to the remaining data and it should be an EVEN number! */
		DWORD rCount = count - spaceLeft;								/* this much data is remaining to be written */
		
		if(rCount < RW_BUFFER_SIZE) {									/* if the remaining data count is less than what we should buffer */
			memcpy(&fb->wBuf[ 0 ], buffer, rCount);						/* copy data to current buffer */
			fb->wCount += rCount;										/* calculate the new data count */

			return count;												
		} else {														/* if the remaining data count is more that we can buffer */
			/* transfer the remaining data in a loop */
			while(rCount > 0) {											/* while there's something to send */
				/* calculate how much data we should transfer in this loop - with respect to MAX SECTORS we can transfer at once */
				DWORD dataNow = (rCount <= (MAXSECTORS*512)) ? rCount : (MAXSECTORS*512);
			
				res = writeData(ceHandle, buffer, dataNow);				/* send the data */

				if(res != TRUE) {										/* failed to write more data? return the count of data written */
					return bytesWritten;
				}
				
				bytesWritten	+= dataNow;								/* update bytes written variable */
				buffer			+= dataNow;								/* and move pointer in buffer further */
				rCount			-= dataNow;								/* decrease the count that is remaining */
			}		
			
			return bytesWritten;										/* return how much we've written total */
		}
	}
	
	/* this should never happen */
	return EINTRN;
}

BYTE writeData(BYTE ceHandle, BYTE *bfr, DWORD cnt)
{
	commandLong[5] = GEMDOS_Fwrite;										/* store GEMDOS function number */
	commandLong[6] = ceHandle;											/* store file handle */
	
	commandLong[7] = cnt >> 16;											/* store byte count */
	commandLong[8] = cnt >>  8;
	commandLong[9] = cnt  & 0xff;
	
	WORD sectorCount = cnt / 512;										/* calculate how many sectors should we transfer */
	
	if((cnt % 512) != 0) {												/* and if we have more than full sector(s) in buffer, send one more! */
		sectorCount++;
	}
	
	BYTE res = acsi_cmd(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, bfr, sectorCount);	/* send command to host over ACSI */
	
	if(res == RW_ALL_TRANSFERED) {										/* good result? ok! */
		return TRUE;
	}

	return FALSE;														/* fail otherwise */
}

/* call this function on fclose, fseek to write the rest of write buffer to the file */
BYTE commitChanges(WORD ceHandle)
{
	BYTE res;

	if(ceHandle >= MAX_FILES) {											/* would be out of index? quit - with error */
		return FALSE;
	}

	if(fileBufs[ceHandle].wCount == 0) {								/* nothing stored in write cache? quit - with no problem */
		return TRUE;
	}
	
	res = writeData(ceHandle, fileBufs[ceHandle].wBuf, fileBufs[ceHandle].wCount);
	return res;
}

/* call this on start to init all, or on fclose / fopen / fcreate to init it */
void initFileBuffer(WORD ceHandle) 
{
	if(ceHandle >= MAX_FILES) {											/* would be out of index? quit */
		return;
	}

	fileBufs[ceHandle].rCount = 0;
	fileBufs[ceHandle].wCount = 0;
}

/* ------------------------------------------------------------------ */

