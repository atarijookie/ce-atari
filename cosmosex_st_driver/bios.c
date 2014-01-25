#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "acsi.h"
#include "translated.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "bios.h"
#include "main.h"

#include "extern_vars.h"
#include "helpers.h"

int32_t custom_mediach( void *sp )
{
	DWORD res;
	
	WORD drive = (WORD) *((WORD *) sp);

	updateCeDrives();													/* update the drives - once per 3 seconds */
	updateCeMediach();													/* update the mediach status - once per 3 seconds */
	
	if(!isOurDrive(drive, 0)) {											/* if the drive is not our drive */
		CALL_OLD_BIOS(Mediach, drive);									/* call the original function */
		return res;
	}
	
	if((getSetCeMediach(GET_MEDIACH) & (1 << drive)) != 0) {					/* if bit is set, media changed */
		return 2;
	}

	return 0;															/* bit not set, media not changed */
}

int32_t custom_drvmap( void *sp )
{
	DWORD res;

	updateCeDrives();			/* update the drives - once per 3 seconds */
	
	CALL_OLD_BIOS(Drvmap);		/* call the original Drvmap() */

	res = res | getSetCeDrives(GET_CEDRIVES);		/* return original drives + CE drives together */
	return res;
}

int32_t custom_getbpb( void *sp )
{
	DWORD res;
	WORD drive = (WORD) *((WORD *) sp);
	static BYTE bpb[20];	

	updateCeDrives();													/* update the drives - once per 3 seconds */

	if(!isOurDrive(drive, 0)) {											/* if the drive is not our drive */
		CALL_OLD_BIOS(Getbpb, drive);									/* call the original function */
		return res;
	}

	/* create pointer to even address */
	DWORD dwBpb = (DWORD) &bpb[1];
	BYTE *pBpb = (BYTE *) (dwBpb & 0xfffffffe);
	
	commandShort[4] = BIOS_Getbpb;										/* store BIOS function number */
	commandShort[5] = (BYTE) drive;										
	
	BYTE *pDmaBuff = getDmaBufferPointer();
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuff, 1);				/* send command to host over ACSI */
	
    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		CALL_OLD_BIOS(Getbpb, drive);
		return res;														
	}
	
	WORD newMediach = getSetCeMediach(GET_MEDIACH) & (~(1 << drive));							/* remove this bit media changes */
	getSetCeMediach(newMediach);
	
	memcpy(pBpb, pDmaBuff, 18);										/* copy in the results */
	return (DWORD) pBpb;
}

/* this function updates the ceDrives variable from the status in host, 
but does this only once per 3 seconds, so you can call it often and 
it will quit then sooner without updating (hoping that nothing changed within 3 seconds) */
void updateCeDrives(void)
{
	DWORD res;
	static DWORD lastCeDrivesUpdate = 0;
	DWORD now = *HZ_200;

	if((now - lastCeDrivesUpdate) < 600) {								/* if the last update was less than 3 seconds ago, don't update */
		return;
	}
	
	lastCeDrivesUpdate = now;											/* mark that we've just updated the ceDrives */
	
	/* now do the real update */
	commandShort[4] = BIOS_Drvmap;											/* store BIOS function number */
	commandShort[5] = 0;										
	
	BYTE *pDmaBuff = getDmaBufferPointer();
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuff, 1);				/* send command to host over ACSI */
	
    if(res == E_NOTHANDLED || res == ACSIERROR) {							/* not handled or error? */
		return;														
	}
	
    getSetCeDrives(getWord(pDmaBuff));                                         /* read drives from dma buffer */
}

void updateCeMediach(void)
{
	DWORD res;
	static DWORD lastMediachUpdate = 0;
	DWORD now = *HZ_200;

	if((now - lastMediachUpdate) < 600) {										/* if the last update was less than 3 seconds ago, don't update */
		return;
	}
	
	lastMediachUpdate = now;													/* mark that we've just updated */
	
	/* now do the real update */
	commandShort[4] = BIOS_Mediach;												/* store BIOS function number */
	commandShort[5] = 0;										
	
	BYTE *pDmaBuff = getDmaBufferPointer();
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuff, 1);	/* send command to host over ACSI */
	
    if(res == E_NOTHANDLED || res == ACSIERROR) {									/* not handled or error? */
		return;														
	}
	
    getSetCeMediach(getWord(pDmaBuff));									/* store current media change status */
}

WORD getSetCeMediach(WORD newVal)
{
	static WORD ceMediach = 0;

	if(newVal != GET_MEDIACH) {											// it's not GET?
		ceMediach = newVal;
	}
	
	return ceMediach;
}

WORD getSetCeDrives(WORD newVal)
{
	static WORD ceDrives = 0;

	if(newVal != GET_CEDRIVES) {
		ceDrives = newVal;
	}
	
	return ceDrives;
}

