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

// * CosmosEx GEMDOS driver by Jookie, 2013 & 2014
// * GEMDOS hooks part (assembler and C) by MiKRO (Miro Kropacek), 2013
 
#include "extern_vars.h"

// ------------------------------------------------------------------ 
// CosmosEx and Gemdos part - Jookie 

#define PE_LOADGO		0
#define PE_LOAD			3
#define PE_GO			4
#define PE_BASEPAGE		5
#define PE_GOTHENFREE	6

typedef struct __attribute__ ((__packed__)) 
{
	DWORD lowtpa;
	DWORD hitpa;
	DWORD tbase;
	DWORD tlen;
	DWORD dbase;
	DWORD dlen;
	DWORD bbase;
	DWORD blen;
	DWORD dta;
	DWORD parent;
	DWORD reserved;
	DWORD env;
} TBasePage;

typedef struct __attribute__ ((__packed__)) 
{
	WORD  magic;
	DWORD tsize;
	DWORD dsize;
	DWORD bsize;
	DWORD ssize;
	DWORD res1;
	DWORD prgFlags;
	WORD  absFlag;
} TPrgHead;

void freeTheBasePage(TBasePage *basePage);

// the following defines and variable will help to determine how the prog was terminated after Pexec and thus to know if should or shouldn't Mfree the RAM
#define TERMINATEDBY_GET		-1
#define TERMINATEDBY_UNKNOWN	0
#define TERMINATEDBY_PTERM		1
#define TERMINATEDBY_PTERMRES	2


BYTE getSetTerminatedBy(BYTE val)
{
	static BYTE terminatedBy = 0;
	
	if(val != TERMINATEDBY_GET) {		// if not get, then set 
		terminatedBy = val;
	}
	
	return terminatedBy;
}

// ------------------------------------------------------------------ 
// LONG Pexec( mode, fname, cmdline, envstr )
int32_t custom_pexec( void *sp )
{
	DWORD res;
	BYTE *params = (BYTE *) sp;

	// retrieve params from stack 
	WORD mode		= *((WORD *) params);
	params += 2;
	char *fname		= (char *)	*((DWORD *) params);
	params += 4;
	char *cmdline	= (char *)	*((DWORD *) params);
	params += 4;
	char *envstr	= (char *)	*((DWORD *) params);

	// for any other than these modes don't do anything special, just call the original
	if(mode != PE_LOADGO && mode != PE_LOAD) {
		CALL_OLD_GD( Pexec, mode, fname, cmdline, envstr);
	}
	
	// if we got here, the mode is PE_LOADGO || PE_LOAD
	WORD drive = getDriveFromPath((char *) fname);

	if(!isOurDrive(drive, 0)) {												/* not our drive? */
		CALL_OLD_GD( Pexec, mode, fname, cmdline, envstr);
	} 
	
	// if we got here, then it's a PRG on our drive...
	BYTE *pBasePage;														// pointer to base page address

	BYTE prgStart[32];
	BYTE *pPrgStart;

	pPrgStart = &prgStart[4];
	pPrgStart = (BYTE *) (((DWORD) pPrgStart) & 0xfffffffc);				// make temp buffer pointer to be at multiple of 4

	// create base page, allocate stuff
	CALL_OLD_GD_NORET(Pexec, PE_BASEPAGE, 0, cmdline, envstr);					
	pBasePage = (BYTE *) res;
	
	if((int) pBasePage < 1000) {											// Pexec seems to failed -- insufficient memory
		return ENSMEM;
	}

	TBasePage *sBasePage = (TBasePage *) pBasePage;							// this is now the pointer to the basePage structure
	
	// load the file to memory
	int32_t file = Fopen(fname, 0);											// try to open the file
	if(file < 0) {															// if the handle is negative, fail -- file not found
		freeTheBasePage(sBasePage);											// free the base page
		return EFILNF;
	}

	DWORD diskProgSize = Fseek(0, file, 2);									// seek to end, returns file size (bytes before end)
	Fseek(0, file, 0);														// seek to the start
	
	if(diskProgSize < 28) {													// if the program is too small, this wouldn't work
		Fclose(file);														// close the file
		freeTheBasePage(sBasePage);											// free the base page
		return EPLFMT;														// be Invalid program load format
	}
	
	Fread(file, 28, pPrgStart);												// read first 28 bytes to this buffer
	TPrgHead *prgHead = (TPrgHead  *) pPrgStart;
	
	// get file size, see if it will fit in the free memory
	DWORD memProgSize		= prgHead->tsize + prgHead->dsize + prgHead->bsize + prgHead->ssize;	// calculate the program size in RAM as size of text + data + bss + symbols
	DWORD memoryAvailable	= sBasePage->hitpa - sBasePage->lowtpa;									// calculate how much memory we have for the program 
	
	if(memoryAvailable < memProgSize || memoryAvailable < diskProgSize) {	// if the program (in RAM or on disk) is bigger than the available free memory
		Fclose(file);														// close the file
		freeTheBasePage(sBasePage);											// free the base page
		return ENSMEM;														// error: insufficient memory
	}

	Fread(file, diskProgSize - 28, pBasePage + 0x100);						// now read the rest of the file
	Fclose(file);
	
	// fill the base page
	sBasePage->tbase	= sBasePage->lowtpa + 0x100;
	sBasePage->tlen		= prgHead->tsize;
	sBasePage->dbase	= sBasePage->tbase + sBasePage->tlen;
	sBasePage->dlen		= prgHead->dsize;
	sBasePage->bbase	= sBasePage->dbase + sBasePage->dlen;
	sBasePage->blen		= prgHead->bsize;
	
	// do the addresses fixup if needed
	BYTE *fixups		= (BYTE *) (sBasePage->tbase + prgHead->tsize + prgHead->dsize + prgHead->ssize);
	DWORD fixupOffset	= *((DWORD *) fixups);
	
	if(fixupOffset != 0) {						// if fixup needed?
		BYTE *pWhereToFix;

		pWhereToFix	= (BYTE *) (sBasePage->tbase + fixupOffset);			// calculate the first DWORD position that needs to be fixed
		fixups += 4;													// move to the fixups array

		while(1) {
			DWORD oldVal = *((DWORD *)pWhereToFix);
			DWORD newVal = oldVal + sBasePage->tbase;
			*((DWORD *)pWhereToFix) = newVal;

			BYTE fixup = *fixups;
			fixups++;
	
			if(fixup == 0) {
				break;
			}
			
			if(fixup == 1) {
				pWhereToFix += 0xfe;
				continue;
			}
			
			pWhereToFix += (DWORD) fixup;
		}
	}
	
	memset((BYTE *) sBasePage->bbase, 0, sBasePage->blen);			// clear BSS section
	
	// do the rest depending on the mode
	if(mode == PE_LOADGO) {											// if we should also run the program
		getSetTerminatedBy(TERMINATEDBY_UNKNOWN);						// mark that we don't know how the program will be / was terminated
	
		CALL_OLD_GD_NORET(Pexec, PE_GO, 0, pBasePage, 0);			// run the program

		if(getSetTerminatedBy(TERMINATEDBY_GET) != TERMINATEDBY_PTERMRES) {					// if the program ended with something different than Ptermres, free the memory
			freeTheBasePage(sBasePage);								// free the base page
		}
		
		return res;													// return the result of PE_LOADGO
	}

	// for mode PE_LOAD -- return value is the starting address of the child processes' base page
	return (DWORD) pBasePage;										// the PE_LOAD was successful 	
}

void freeTheBasePage(TBasePage *basePage)
{
	Mfree(basePage->env);											// free the environment
	Mfree(basePage);												// free the base page	
}

int32_t custom_pterm( void *sp )
{
	getSetTerminatedBy(TERMINATEDBY_PTERM);								// mark that Pterm was used and the memory should be freed
	
	WORD retCode = (WORD) *((WORD *) sp);
	CALL_OLD_GD_VOIDRET(Pterm, retCode);
	
	return 0;
}

int32_t custom_pterm0( void *sp )
{
	getSetTerminatedBy(TERMINATEDBY_PTERM);								// mark that Pterm was used and the memory should be freed
	
	CALL_OLD_GD_VOIDRET(Pterm0);

	return 0;
}

int32_t custom_ptermres( void *sp )
{
	BYTE *params = (BYTE *) sp;

	getSetTerminatedBy(TERMINATEDBY_PTERMRES);							// mark that Ptermres was used and the memory should NOT be freed

	DWORD keep		= *((DWORD *) params);
	params += 4;
	WORD retcode	= *((WORD *) params);
	
	CALL_OLD_GD_VOIDRET(Ptermres, keep, retcode);

	return 0;
}
