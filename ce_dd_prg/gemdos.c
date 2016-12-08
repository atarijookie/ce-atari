#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ce_dd_prg.h"
#include "xbra.h"
#include "acsi.h"
#include "hdd_if.h"
#include "translated.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "bios.h"
#include "main.h"

/* 
 * CosmosEx GEMDOS driver by Jookie, 2013 & 2014
 * GEMDOS hooks part (assembler and C) by MiKRO (Miro Kropacek), 2013
 */ 
 
// ------------------------------------------------------------------ 
// init and hooks part - MiKRO 
extern int16_t useOldGDHandler;											// 0: use new handlers, 1: use old handlers 
extern int16_t useOldBiosHandler;										// 0: use new handlers, 1: use old handlers  

extern int32_t (*gemdos_table[256])( void* sp );
extern int32_t (  *bios_table[256])( void* sp );

// ------------------------------------------------------------------ 
// CosmosEx and Gemdos part - Jookie 

static BYTE *dtaBufferValidForPDta;

BYTE  getNextDTAsFromHost(void);
DWORD copyNextDtaToAtari(void);
void  onFsnext_last(void);

void sendStLog(char *str);

extern TFileBuffer fileBufs[MAX_FILES];

BYTE fseek_cur(int32_t offset, BYTE ceHandle, TFileBuffer *fb);
void fseek_invalSeekStore(int32_t offset, BYTE ceHandle, BYTE seekMode, TFileBuffer *fb);
void fseek_hdif_command(int32_t offset, BYTE ceHandle, BYTE seekMode);

void showWaitSymbol(BYTE showNotHide);
void msleepInSuper(int ms);

// ------------------------------------------------------------------ 
// fot fake Pexec() and Pterm() handling
void    installHddLowLevelDriver(void);
int32_t custom_pexec_lowlevel(void *sp);

// ------------------------------------------------------------------ 
// the custom GEMDOS handlers now follow 

int32_t custom_dgetdrv( void *sp )
{
	DWORD res;

	if(!isOurDrive(currentDrive, 0)) {									// if the current drive is not our drive 
		CALL_OLD_GD_NORET(Dgetdrv);
	
		currentDrive = res;												// store the current drive 
		return res;
	}
	
	commandShort[4] = GEMDOS_Dgetdrv;									// store GEMDOS function number 
	commandShort[5] = 0;										
	
	(*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);  // send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {				// not handled or error? 
		CALL_OLD_GD_NORET(Dgetdrv);

		currentDrive = res;												// store the current drive 
		return res;														// return the value returned from old handler 
	}

	currentDrive = hdIf.statusByte;										// store the current drive 
    return hdIf.statusByte;												// return the result 
}

int32_t custom_dsetdrv( void *sp )
{
	// get the drive # from stack 
	WORD drive = (WORD) *((WORD *) sp);
	currentDrive = drive;												    // store the drive - GEMDOS seems to let you set even invalid drive 
	
    useOldGDHandler = 1;
    Dsetdrv(drive);                                                         // let TOS know the current drive

    commandShort[4] = GEMDOS_Dsetdrv;										// store GEMDOS function number 
    commandShort[5] = (BYTE) drive;											// store drive number 
    (*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);	// send command to host over ACSI 

	DWORD res = Drvmap();											        // BIOS call - get drives bitmap - this will also communicate with CE 
    return res;
}

int32_t custom_dfree( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	BYTE *pDiskInfo	= (BYTE *)	*((DWORD *) params);
	params += 4;
	WORD drive		= (WORD)	*((WORD *)  params);
	
	if(!isOurDrive(drive, 1)) {										    // not our drive? 
		CALL_OLD_GD( Dfree, pDiskInfo, drive);
	}
	
	commandShort[4] = GEMDOS_Dfree;								        // store GEMDOS function number 
	commandShort[5] = drive;									

	(*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);  // send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {		        // not handled or error? 
		CALL_OLD_GD( Dfree, pDiskInfo, drive);
	}

	memcpy(pDiskInfo, pDmaBuffer, 16);									// copy in the results 
    return extendByteToDword(hdIf.statusByte);
}

int32_t custom_dcreate( void *sp )
{
	DWORD res;
    char *pPath	= (char *) *((DWORD *) sp);

	WORD drive = getDriveFromPath((char *) pPath);
	
	if(!isOurDrive(drive, 0)) {											    // not our drive? 
		CALL_OLD_GD( Dcreate, pPath);
	}
	
	commandShort[4] = GEMDOS_Dcreate;										// store GEMDOS function number 
	commandShort[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		    // copy in the path 
	
	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1); // send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {				    // not handled or error? 
		CALL_OLD_GD( Dcreate, pPath);
	}

    return extendByteToDword(hdIf.statusByte);
}

int32_t custom_ddelete( void *sp )
{
	DWORD res;
    char *pPath	= (char *) *((DWORD *) sp);
	
	WORD drive = getDriveFromPath((char *) pPath);
	
	if(!isOurDrive(drive, 0)) {											    // not our drive? 
		CALL_OLD_GD( Ddelete, pPath);
	}
	
	commandShort[4] = GEMDOS_Ddelete;										// store GEMDOS function number 
	commandShort[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		    // copy in the path 
	
	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1); // send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {		            // not handled or error? 
		CALL_OLD_GD(Ddelete, pPath);
	}

    return extendByteToDword(hdIf.statusByte);
}

int32_t custom_fdelete( void *sp )
{
	DWORD res;
    char *pPath	= (char *) *((DWORD *) sp);
	
	WORD drive = getDriveFromPath((char *) pPath);
	
	if(!isOurDrive(drive, 0)) {											    // not our drive? 
		CALL_OLD_GD( Fdelete, pPath);
	}
	
	commandShort[4] = GEMDOS_Fdelete;										// store GEMDOS function number 
	commandShort[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		    // copy in the path 
	
	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1); // send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {			        // not handled or error? 
		CALL_OLD_GD( Fdelete, pPath);
	}

    return extendByteToDword(hdIf.statusByte);
}

int32_t custom_dsetpath( void *sp )
{
	DWORD res;
    char *pPath	= (char *) *((DWORD *) sp);
	
	WORD drive = getDriveFromPath((char *) pPath);
	
	if(!isOurDrive(drive, 0)) {											    // not our drive? 
		CALL_OLD_GD( Dsetpath, pPath);
	}
    
	commandShort[4] = GEMDOS_Dsetpath;										// store GEMDOS function number 
	commandShort[5] = 0;									
	
	memset(pDmaBuffer, 0, 512);
	strncpy((char *) pDmaBuffer, (char *) pPath, DMA_BUFFER_SIZE);		    // copy in the path 
	
    while(1) {
        (*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1); // send command to host over ACSI 

        if(hdIf.success && hdIf.statusByte == E_WAITING_FOR_MOUNT) {            // if waiting for mount
            showWaitSymbol(1);
        
            msleepInSuper(500);
            continue;
        }
        
        if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {				    // not handled or error? 
            showWaitSymbol(0);
            CALL_OLD_GD( Dsetpath, pPath);
        }
        
        break;
    }

    showWaitSymbol(0);
    return extendByteToDword(hdIf.statusByte);
}

int32_t custom_dgetpath( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

    char *buffer	= (char *)	*((DWORD *) params);
	params += 4;
	WORD drive		= (WORD)	*((WORD *)  params);
	
	if(!isOurDrive(drive, 1)) {											    // not our drive? 
		CALL_OLD_GD( Dgetpath, buffer, drive);
	}
	
	commandShort[4] = GEMDOS_Dgetpath;										// store GEMDOS function number 
	commandShort[5] = drive;									

	(*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);  // send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {				    // not handled or error? 
		CALL_OLD_GD( Dgetpath, buffer, drive);
	}

	strncpy((char *)buffer, (char *)pDmaBuffer, DMA_BUFFER_SIZE);		    // copy in the results 
    return extendByteToDword(hdIf.statusByte);
}

int32_t custom_frename( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	params += 2;														    // skip reserved WORD 
	char *oldName	= (char *)	*((DWORD *) params);
	params += 4;
	char *newName	= (char *)	*((DWORD *) params);
	
	WORD drive = getDriveFromPath((char *) oldName);
	
	if(!isOurDrive(drive, 0)) {											    // not our drive? 
		CALL_OLD_GD( Frename, 0, oldName, newName);
	}
	
	commandShort[4] = GEMDOS_Frename;										// store GEMDOS function number 
	commandShort[5] = 0;									

	memset(pDmaBuffer, 0, DMA_BUFFER_SIZE);
	strncpy((char *) pDmaBuffer, oldName, (DMA_BUFFER_SIZE / 2) - 2);	    // copy in the old name	
	
	int oldLen = strlen((char *) pDmaBuffer);							    // get the length of old name 
	
	char *pDmaNewName = ((char *) pDmaBuffer) + oldLen + 1;
	strncpy(pDmaNewName, newName, (DMA_BUFFER_SIZE / 2) - 2);			    // copy in the new name	

	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1); // send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {				    // not handled or error? 
		CALL_OLD_GD( Frename, 0, oldName, newName);
	}

    return extendByteToDword(hdIf.statusByte);
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
	
	if(!isOurDrive(drive, 0)) {											    // not our drive? 
		CALL_OLD_GD( Fattrib, fileName, flag, attr);
	}
	
	commandShort[4] = GEMDOS_Fattrib;										// store GEMDOS function number 
	commandShort[5] = 0;									

	memset(pDmaBuffer, 0, DMA_BUFFER_SIZE);
	
	pDmaBuffer[0] = (BYTE) flag;										    // store set / get flag 
	pDmaBuffer[1] = (BYTE) attr;										    // store attributes 
	
	strncpy(((char *) pDmaBuffer) + 2, fileName, DMA_BUFFER_SIZE -1 );	    // copy in the file name 
	
	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1); // send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {                  // not handled or error? 
		CALL_OLD_GD( Fattrib, fileName, flag, attr);
	}

    return extendByteToDword(hdIf.statusByte);
}

// **************************************************************** 
// those next functions are used for file / dir search 

int32_t custom_fsetdta( void *sp )
{
    pDta = (BYTE *) *((DWORD *) sp);									// store the new DTA pointer 

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

	// get params 
	char *fspec		= (char *)	*((DWORD *) params);
	params += 4;
	WORD attribs	= (WORD)	*((WORD *)  params);
	
	WORD drive = getDriveFromPath((char *) fspec);
	
	if(!isOurDrive(drive, 0)) {											// not our drive? 
		fsnextIsForUs = FALSE;
	
		CALL_OLD_GD( Fsfirst, fspec, attribs);
	}
	
	// initialize internal variables 
	fsnextIsForUs		= TRUE;
	
    //------------
	// initialize the reserved section of DTA, which will contain a pointer to the DTA address and index of the dir/file we returned last
	useOldGDHandler = 1;
    pDta = (BYTE *) Fgetdta();											// retrieve the current DTA pointer - this might have changed 
	
	pDta[0] = ((DWORD) pDta) >> 24;										// first store the pointer to this DTA (to enable continuing of Fsnext() in case this DTA buffer would be copied otherwise and then set with Fsetdta())
	pDta[1] = ((DWORD) pDta) >> 16;
	pDta[2] = ((DWORD) pDta) >>  8;
	pDta[3] = ((DWORD) pDta)      ;
	
    SET_WORD(pDta+4, 0);                                                // index of the dir/file we've returned so far
    SET_WORD(pDta+6, 0);                                                // current DTA index in currently buffered DTAs (0 .. 21)
    SET_WORD(pDta+8, 0);                                                // total DTAs there are in currently buffered DTAs (0 .. 22)
    
    pDta[10] = 1;                   								    // mark that when we run out of DTAs in out buffer, then we should try to get more of them from host 
    //------------
	
	// set the params to buffer 
	commandShort[4] = GEMDOS_Fsfirst;									// store GEMDOS function number
	commandShort[5] = 0;			
	
	int i;
	for(i=0; i<4; i++) {
		pDmaBuffer[i] = pDta[i];										// 1st 4 bytes will now be the pointer to DTA on which the Fsfirst() was called
	}
	
	pDmaBuffer[4] = (BYTE) attribs;										// store attributes 
	strncpy(((char *) pDmaBuffer) + 5, fspec, DMA_BUFFER_SIZE - 1);		// copy in the file specification 
	
	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1); // send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {                  // not handled or error? 
		fsnextIsForUs = FALSE;
	
		CALL_OLD_GD( Fsfirst, fspec, attribs);
	}
	
	if(hdIf.statusByte != E_OK) {										// if some other error, just return it 
        return extendByteToDword(hdIf.statusByte);						// but append lots of FFs to make negative integer out of it 
	}

	res = copyNextDtaToAtari();											// now copy the next possible DTA to atari DTA buffer 
    return extendByteToDword(res);
}

int32_t custom_fsnext( void *sp )
{
	DWORD res;

	if(!fsnextIsForUs) {												// if we shouldn't handle this 
		CALL_OLD_GD( Fsnext);
	}

	//-----------------
	// the following is here because of Fsfirst() / Fsnext() nesting
	useOldGDHandler = 1;
    pDta = (BYTE *) Fgetdta();											// retrieve the current DTA pointer - this might have changed 
	
	if(dtaBufferValidForPDta != pDta) {									// if DTA changed, we need to retrieve the search results again (the current search results are for a different dir)
		res = getNextDTAsFromHost();									// now we need to get the buffer of DTAs from host 
		
		if(res != E_OK) {												// failed to get DTAs from host? 
			onFsnext_last();											// tell host that we're done with this pDTA
		
			pDta[10] = 0;										        // do not try to receive more DTAs from host 
            return extendByteToDword(ENMFIL);							// return that we're out of files 
		}
	}
	//----------

	res = copyNextDtaToAtari();											// now copy the next possible DTA to atari DTA buffer 
	return res;
}

DWORD copyNextDtaToAtari(void)
{
	DWORD res;

    //------------
    // retrieve the current DTA pointer - this might have changed 
	useOldGDHandler = 1;
    pDta = (BYTE *) Fgetdta();
    //------------
    WORD dtaCurrent, dtaTotal;
    dtaCurrent  = GET_WORD(pDta + 6);                                   // restore variables from memory
    dtaTotal    = GET_WORD(pDta + 8);                                   // restore variables from memory
    
	if(dtaCurrent >= dtaTotal) {										// if we're out of buffered DTAs 
		if(!pDta[10]) {											        // if shouldn't try to get more DTAs, quit without trying 
			onFsnext_last();											// tell host that we're done with this pDTA

            return extendByteToDword(ENMFIL);
		}
	
		res = getNextDTAsFromHost();									// now we need to get the buffer of DTAs from host 

        dtaCurrent  = GET_WORD(pDta + 6);                               // restore variables from memory
        dtaTotal    = GET_WORD(pDta + 8);                               // restore variables from memory
		
		if(res != E_OK) {												// failed to get DTAs from host? 
			onFsnext_last();											// tell host that we're done with this pDTA

			pDta[10] = 0;										        // do not try to receive more DTAs from host 
            return extendByteToDword(ENMFIL);							// return that we're out of files 
		}
	}

	if(dtaCurrent >= dtaTotal) {										// still no buffered DTAs? (this shouldn't happen) 
		onFsnext_last();												// tell host that we're done with this pDTA
        return extendByteToDword(ENMFIL);								// return that we're out of files 
	}

	DWORD dtaOffset		= 2 + (23 * dtaCurrent);						// calculate the offset for the DTA in buffer 
	BYTE *pCurrentDta	= pDtaBuffer + dtaOffset;						// and now calculate the new pointer 
	
	dtaCurrent++;														// move to the next DTA 
		
    SET_WORD(pDta + 6, dtaCurrent);                                     // update current DTA index in currently buffered DTAs (0 .. 21)
	//--------------
	// update the item index in the reserved part of DTA
	WORD itemIndex	= GET_WORD(pDta + 4);                               // get the current dir 
	itemIndex++;
    SET_WORD(pDta + 4, itemIndex);                                      // update current itemIndex
	//--------------
	
	memcpy(pDta + 21, pCurrentDta, 23);									// skip the reserved area of DTA and copy in the current DTA 
	return E_OK;														// everything went well 
}

BYTE getNextDTAsFromHost(void)
{
	// initialize the internal variables 
    SET_WORD(pDta + 6, 0);                                                  // current DTA index in currently buffered DTAs (0 .. 21)
    SET_WORD(pDta + 8, 0);                                                  // total DTAs there are in currently buffered DTAs (0 .. 22)
    
	commandLong[5] = GEMDOS_Fsnext;											// store GEMDOS function number 

	int i;
	for(i=0; i<6; i++) {													// commandLong 6,7,8,9 are the pointer to DTA on which Fsfirst() was called, and commandLong 10,11 contain index of the dir/file we've returned so far
		commandLong[6 + i] = pDta[i];
	}
	
	dtaBufferValidForPDta = pDta;											// store that the current buffer will be valid for this DTA pointer
	
	(*hdIf.cmd)(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDtaBuffer, 1);	// send command to host over ACSI
	
	if(!hdIf.success || hdIf.statusByte != E_OK) {							// if failed to transfer data - no more files
        return ENMFIL;
	}
	
	// store the new total DTA number we have buffered 
    pDta[8] = pDtaBuffer[0];                                                // total DTAs there are in currently buffered DTAs (0 .. 22)
    pDta[9] = pDtaBuffer[1];
    
	return E_OK;
}

void onFsnext_last(void)
{
	commandLong[5] = GD_CUSTOM_Fsnext_last;									// store GEMDOS function number 

	int i;
	for(i=0; i<4; i++) {													// commandLong 6,7,8,9 are the pointer to DTA on which Fsfirst() was called
		commandLong[6 + i] = pDta[i];
	}
	
	(*hdIf.cmd)(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDtaBuffer, 1);	// send command to host over ACSI
}

// **************************************************************** 
// the following functions work with files and file handles 

int32_t custom_fcreate( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	// get params 
	char *fileName	= (char *)	*((DWORD *) params);
	params += 4;
	WORD attr		= (WORD)	*((WORD *)  params);
	
	WORD drive = getDriveFromPath((char *) fileName);
	
	if(!isOurDrive(drive, 0)) {											    // not our drive? 
		CALL_OLD_GD( Fcreate, fileName, attr);
	}
	
	// set the params to buffer 
	commandShort[4] = GEMDOS_Fcreate;									    // store GEMDOS function number 
	commandShort[5] = 0;			
	
	pDmaBuffer[0] = (BYTE) attr;										    // store attributes 
	strncpy(((char *) pDmaBuffer) + 1, fileName, DMA_BUFFER_SIZE - 1);	    // copy in the file name 
	
	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1); // send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {				    // not handled or error? 
		CALL_OLD_GD( Fcreate, fileName, attr);
	}
	
	if(hdIf.statusByte == ENHNDL || hdIf.statusByte == EACCDN || hdIf.statusByte == EINTRN) {   // if some other error, just return it 
        return extendByteToDword(hdIf.statusByte);									            // but append lots of FFs to make negative integer out of it 
	}
    
    WORD ceHandle = hdIf.statusByte;                    // this is CE handle (0 - 40)
    
    if(ceHandle >= MAX_FILES) {                         // if it's out of range, internal error!
        return EINTRN;
    }
	
    fileBufs[ceHandle].isOpen       = TRUE;             // it's now OPEN
    fileBufs[ceHandle].currentPos   = 0;                // current position - file start
    
	WORD atariHandle = handleCEtoAtari(ceHandle);       // convert the CE handle (0 - 40) to Atari handle (80 - 120)
	return atariHandle;
}

int32_t custom_fopen( void *sp )
{
    DWORD res;
	BYTE *params = (BYTE *) sp;

	// get params 
	char *fileName	= (char *)	*((DWORD *) params);
	params += 4;
	WORD mode		= (WORD)	*((WORD *)  params);
	
	WORD drive = getDriveFromPath((char *) fileName);
	
	if(!isOurDrive(drive, 0)) {											// not our drive? 
		CALL_OLD_GD( Fopen, fileName, mode);
	}
	
	// set the params to buffer 
	commandShort[4] = GEMDOS_Fopen;											// store GEMDOS function number 
	commandShort[5] = 0;			
	
	pDmaBuffer[0] = (BYTE) mode;										// store attributes 
	strncpy(((char *) pDmaBuffer) + 1, fileName, DMA_BUFFER_SIZE - 1);	// copy in the file name 
	
	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);			// send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {				// not handled or error? 
		CALL_OLD_GD( Fopen, fileName, mode);
	}
	
	if(hdIf.statusByte == ENHNDL || hdIf.statusByte == EACCDN || hdIf.statusByte == EINTRN || hdIf.statusByte == EFILNF) {		// if some other error, just return it 
        return extendByteToDword(hdIf.statusByte);								    // but append lots of FFs to make negative integer out of it 
	}
    
    // if we got here, the result is the real ceHandle
    WORD ceHandle       = (WORD) hdIf.statusByte;
	WORD atariHandle    = handleCEtoAtari(ceHandle);                    // convert the CE handle (0 - 40) to Atari handle (80 - 120)
    
    fileBufs[ceHandle].isOpen       = TRUE;                             // file is open!
    fileBufs[ceHandle].currentPos   = 0;                                // current position - file start
    getBytesToEof(ceHandle);                                            // retrieve the count of bytes until the end of file
	
	return atariHandle;
}

int32_t custom_fclose( void *sp )
{
    DWORD res;
	WORD atariHandle	= (WORD) *((WORD *) sp);

	// check if this handle should belong to cosmosEx 
	if(!handleIsFromCE(atariHandle)) {									// not called with handle belonging to CosmosEx? 
		CALL_OLD_GD(Fclose, atariHandle);
	}
	
	WORD ceHandle = handleAtariToCE(atariHandle);						// convert high atari handle to little CE handle 
	
    if(!fileBufs[ceHandle].isOpen) {                                    // file not open? fail - INVALID HANDLE
        return extendByteToDword(EIHNDL);
    }
    
	commitChanges(ceHandle);											// flush write buffer if needed 
	initFileBuffer(ceHandle);											// init the file buffer like it was never used (and closed)
	
	// set the params to buffer 
	commandShort[4] = GEMDOS_Fclose;										// store GEMDOS function number 
	commandShort[5] = (BYTE) ceHandle;			
	
	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1); // send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {			        // not handled or error? 
		CALL_OLD_GD( Fclose, atariHandle);
	}
	
    return extendByteToDword(hdIf.statusByte);
}

int32_t custom_fseek( void *sp )
{
	DWORD res = 0;
	BYTE *params = (BYTE *) sp;

	// get params 
	DWORD offset		= (DWORD)	*((DWORD *) params);
	params += 4;
	WORD atariHandle	= (WORD)	*((WORD *)  params);
	params += 2;
	WORD seekMode		= (WORD)	*((WORD *)  params);
	
	// check if this handle should belong to cosmosEx 
	if(!handleIsFromCE(atariHandle)) {									// not called with handle belonging to CosmosEx? 
		CALL_OLD_GD( Fseek, offset, atariHandle, seekMode);
	}
	
	WORD        ceHandle    = handleAtariToCE(atariHandle);             // convert high atari handle to little CE handle 
	TFileBuffer *fb         = &fileBufs[ceHandle];

    if(!fb->isOpen) {                                                   // file not open? fail - INVALID HANDLE
        return extendByteToDword(EIHNDL);
    }
	
	commitChanges(ceHandle);											// flush write buffer if needed 
    
    if(seekMode == SEEK_CUR) {                                          // SEEK_CUR -- try seek in local buffer, and it that's not enough, do real seek
        BYTE doRealSeek = fseek_cur(offset, ceHandle, fb);
        
        if(doRealSeek) {                                                // if seek in local buffer didn't work, do real seek
            fseek_invalSeekStore(offset, ceHandle, seekMode, fb);
        }
    } else {                                                            // SEEK_SET and SEEK_END - just invalidate local buffer and to real seek
        fseek_invalSeekStore(offset, ceHandle, seekMode, fb);
    }
        
	if(hdIf.statusByte != E_OK) {				    					// if some other error, make negative number out of it 
        return extendByteToDword(hdIf.statusByte);
	}
	
	return fb->currentPos;                                              // return the position in file
}

BYTE fseek_cur(int32_t offset, BYTE ceHandle, TFileBuffer *fb)
{
	int32_t newPos = ((int32_t) fb->rStart) + offset;					// calculate the new position
	
	if(newPos < 0 || newPos >= fb->rCount) {						    // the seek would go outside of local buffer?
        fseek_hdif_command(fb->currentPos, ceHandle, SEEK_SET);         // tell the host where we are - synchronize local pointer with host pointer
        return TRUE;                                                    // do fseek_invalSeekStore() after this
	} 
    
    // seek fits in the local buffer?
    fb->currentPos  += offset;                                          // update absolute position in stream
    fb->rStart       = newPos;                                          // store the new position in the buffer	
    return FALSE;                                                       // don't do fseek_invalSeekStore() after this
}

// fseek - invalidate local buffer, then do real seek, then store current position
void fseek_invalSeekStore(int32_t offset, BYTE ceHandle, BYTE seekMode, TFileBuffer *fb)
{
    // invalidate local buffer - now we don't have any data left
    fb->rCount = 0;
    fb->rStart = 0;
    
    fseek_hdif_command(offset, ceHandle, seekMode);
    
    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {			        // not handled or error? fail
		return;
	}
	
	if(hdIf.statusByte != E_OK) {				    					    // if some other error, fail
        return;
	}
	
	// If we got here, the seek was successful and now we need to return the position in the file. 
	fb->currentPos          = getDword(pDmaBuffer);     // get the new file position from the received data
    fb->bytesToEOF          = getDword(pDmaBuffer + 4); // also get the count of bytes until the EOF (we shouldn't read past that)
    fb->bytesToEOFinvalid   = 0;                        // mark that the bytesToEOF is valid    
}

void fseek_hdif_command(int32_t offset, BYTE ceHandle, BYTE seekMode)
{
    // set the params to buffer 
	commandLong[5] = GEMDOS_Fseek;											// store GEMDOS function number 
	
	// store params to command sequence 
	commandLong[6] = (BYTE) (offset >> 24);				
	commandLong[7] = (BYTE) (offset >> 16);
	commandLong[8] = (BYTE) (offset >>  8);
	commandLong[9] = (BYTE) (offset & 0xff);
	
	commandLong[10] = (BYTE) ceHandle;
	commandLong[11] = (BYTE) seekMode;
	
	(*hdIf.cmd)(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuffer, 1);    // send command to host over ACSI 
}

int32_t custom_fdatime( void *sp )
{
	DWORD res = 0;
	BYTE *params = (BYTE *) sp;

	// get params 
	BYTE *pDatetime		= (BYTE *)		*((DWORD *) params);
	params += 4;
	WORD atariHandle	= (WORD)		*((WORD *)  params);
	params += 2;
	WORD flag			= (WORD)		*((WORD *)  params);
	
	// check if this handle should belong to cosmosEx 
	if(!handleIsFromCE(atariHandle)) {										// not called with handle belonging to CosmosEx? 
		CALL_OLD_GD( Fdatime, pDatetime, atariHandle, flag);
	}
	
	WORD ceHandle = handleAtariToCE(atariHandle);							// convert high atari handle to little CE handle 
	
    if(!fileBufs[ceHandle].isOpen) {                                    // file not open? fail - INVALID HANDLE
        return extendByteToDword(EIHNDL);
    }
    
	// set the params to buffer 
	commandLong[5]  = GEMDOS_Fdatime;										// store GEMDOS function number 
	commandLong[6]  = (flag << 7) | (ceHandle & 0x7f);						// flag on highest bit, the rest is handle 
	
	// store params to command sequence 
	commandLong[7]  = (BYTE) pDatetime[0];									// store the current date time value 
	commandLong[8]  = (BYTE) pDatetime[1];
	commandLong[9]  = (BYTE) pDatetime[2];
	commandLong[10] = (BYTE) pDatetime[3];

	(*hdIf.cmd)(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuffer, 1);	// send command to host over ACSI 
	
	if(flag != 1) {															// not FD_SET - get date time 
		memcpy(pDatetime, pDmaBuffer, 4);									// copy in the results 
	}
	
	if(hdIf.success && hdIf.statusByte == E_OK) {						    // good? ok 
		return E_OK;
	}
		
    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED || hdIf.statusByte == EINTRN) {    // not handled or error? 
        return extendByteToDword(EINTRN);										// return internal error 
	}

    return extendByteToDword(EINTRN);											// in other cases - Internal Error - this shouldn't happen 
}

void sendStLog(char *str)
{
	// set the params to buffer 
	commandShort[4] = ST_LOG_TEXT;											    // store GEMDOS function number 
	commandShort[5] = 0;			
	
    strncpy((char *) pDmaBuffer, str, 512);
	(*hdIf.cmd)(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);	    // send command to host over ACSI 
}

// ------------------------------------------------------------------ 
// helper functions 
WORD getDriveFromPath(char *path)
{
	if(strlen(path) < 3) {												// if the path is too short to be full path, e.g. 'C:\DIR', just return currentDrive 
		return currentDrive;
	}
	
	if(path[1] != ':') {												// if the path isn't full path, e.g. C:\, just return currentDrive 
		return currentDrive;
	}
	
	char letter = path[0];
	
	if(letter >= 'a' && letter <= 'z') {								// small letter? 
		return (letter - 'a');
	}
	
	if(letter >= 'A' && letter <= 'Z') {								// capital letter? 
		return (letter - 'A');
	}
	
	return currentDrive;												// other case? return currentDrive 	
}

BYTE isOurDrive(WORD drive, BYTE withCurrentDrive) 
{
	if(withCurrentDrive) {												// if the 0 in drive doesn't mean 'A', but 'current drive', then we have to figure out what the drive is 
		if(drive == 0) {												// asking for current drive? 
			drive = currentDrive;
		} else {														// asking for normal drive? 
			drive--;
		}
	}
	
	if(drive > 15) {													// just in case somebody would ask for non-sense 
		return FALSE;
	}
	
	if(drive < 2) {														// asking for drive A or B? not ours! 
		return FALSE;
	}
	
	updateCeDrives();													// update ceDrives variable 
	
	if(ceDrives & (1 << drive)) {										// is that bit set? 
		return TRUE;
	}

	return FALSE;
}

void showWaitSymbol(BYTE showNotHide)
{
    static BYTE isShown = 0;
    static char progChars[4] = {'|', '/', '-', '\\'};
    static BYTE progress = 0;

    if(showNotHide) {   // show wait symbol
        isShown = 1;

        (void) Cconws("\33Y  \33p");
        Cconout(progChars[progress]);
        (void) Cconws("\33q");
        
        progress = (progress + 1) & 0x03;
    } else {            // hide wait symbol
        if(isShown) {
            isShown = 0;
            (void) Cconws("\33Y  \33q ");
        }
    }
}

// ------------------------------------------------------------------ 
void initFunctionTable(void)
{
	// fill the table with pointers to functions 
    
    // path functions
	gemdos_table[GEMDOS_Dsetdrv]    = custom_dsetdrv;
	gemdos_table[GEMDOS_Dgetdrv]    = custom_dgetdrv;
	gemdos_table[GEMDOS_Dsetpath]   = custom_dsetpath;
	gemdos_table[GEMDOS_Dgetpath]   = custom_dgetpath;

    // directory & file search
    gemdos_table[GEMDOS_Fsetdta]    = custom_fsetdta;
	gemdos_table[GEMDOS_Fsfirst]    = custom_fsfirst;
	gemdos_table[GEMDOS_Fsnext]     = custom_fsnext;

    // file and directory manipulation
	gemdos_table[GEMDOS_Dfree]      = custom_dfree;
	gemdos_table[GEMDOS_Dcreate]    = custom_dcreate;
	gemdos_table[GEMDOS_Ddelete]    = custom_ddelete;
	gemdos_table[GEMDOS_Frename]    = custom_frename;
	gemdos_table[GEMDOS_Fdatime]    = custom_fdatime;
	gemdos_table[GEMDOS_Fdelete]    = custom_fdelete;
	gemdos_table[GEMDOS_Fattrib]    = custom_fattrib;

    // file content functions
	gemdos_table[GEMDOS_Fcreate]    = custom_fcreate;
	gemdos_table[GEMDOS_Fopen]      = custom_fopen;
	gemdos_table[GEMDOS_Fclose]     = custom_fclose;
	gemdos_table[GEMDOS_Fread]      = custom_fread;
	gemdos_table[GEMDOS_Fwrite]     = custom_fwrite;
	gemdos_table[GEMDOS_Fseek]      = custom_fseek;

    // program execution functions
    
#ifdef MANUAL_PEXEC
    // manually handling Pexec() and Pterm*() functions
	gemdos_table[GEMDOS_pexec]      = custom_pexec;
	gemdos_table[GEMDOS_pterm]      = custom_pterm;
	gemdos_table[GEMDOS_pterm0]     = custom_pterm0;
#else
    // letting TOS handle Pexec() and Pterm*() function by faking native drive
	gemdos_table[GEMDOS_pexec]      = custom_pexec_lowlevel;
	gemdos_table[GEMDOS_pterm]      = 0;
	gemdos_table[GEMDOS_pterm0]     = 0;

    Supexec(installHddLowLevelDriver);
#endif
    
    // BIOS functions we need to support
	bios_table[0x0a] = custom_drvmap; 
	bios_table[0x09] = custom_mediach;
	bios_table[0x07] = custom_getbpb;
}
