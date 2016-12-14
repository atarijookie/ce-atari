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
#include "translated.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "bios.h"
#include "main.h"

// * CosmosEx GEMDOS driver by Jookie, 2013 & 2014
// * GEMDOS hooks part (assembler and C) by MiKRO (Miro Kropacek), 2013

// ------------------------------------------------------------------
// init and hooks part - MiKRO
extern int16_t useOldGDHandler;											// 0: use new handlers, 1: use old handlers
extern int16_t useOldBiosHandler;										// 0: use new handlers, 1: use old handlers

extern int32_t (*gemdos_table[256])( void* sp );
extern int32_t (  *bios_table[256])( void* sp );

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

static BYTE *pLastBasePage = 0;

extern WORD pexec_postProc;
extern WORD pexec_callOrig;

int32_t custom_pexec2(void *res);

// the following global variables are used in both custom_pexec and custom_pexec2
static char *fname, *cmdline, *envstr;
static BYTE *pPrgStart;
static WORD mode;

// ------------------------------------------------------------------
// LONG Pexec( mode, fname, cmdline, envstr )
int32_t custom_pexec( void *sp )
{
	BYTE *params = (BYTE *) sp;

	// retrieve params from stack
	mode    = *((WORD *) params);
	params += 2;
	fname	= (char *)	*((DWORD *) params);
	params += 4;
	cmdline	= (char *)	*((DWORD *) params);
	params += 4;
	envstr	= (char *)	*((DWORD *) params);

	// for any other than these modes don't do anything special, just call the original
	if(mode != PE_LOADGO && mode != PE_LOAD) {                              // not one of 2 supported modes? Call original Pexec()
        pexec_callOrig = 1;                                                 // will call the original Pexec() handler from asm when this finishes
        return 0;
	}

	// if we got here, the mode is PE_LOADGO || PE_LOAD
	WORD drive = getDriveFromPath((char *) fname);

	if(!isOurDrive(drive, 0)) {												// not our drive? Call original Pexec()
        pexec_callOrig = 1;                                                 // will call the original Pexec() handler from asm when this finishes
        return 0;
	}

    // Do PE_LOAD, and if this was PE_LOADGO, then the GO part will be done in gemdos_asm.s

	// if we got here, then it's a PRG on our drive...

	BYTE prgStart[32];

	pPrgStart = &prgStart[4];
	pPrgStart = (BYTE *) (((DWORD) pPrgStart) & 0xfffffffc);				// make temp buffer pointer to be at multiple of 4

	// create base page
    BYTE *pBasePage = (BYTE *) Pexec(PE_BASEPAGE, 0, cmdline, envstr);

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

    BYTE fixup = 0;

	if(fixupOffset != 0) {						                    // if fixup needed?
		BYTE *pWhereToFix;

		pWhereToFix	= (BYTE *) (sBasePage->tbase + fixupOffset);	// calculate the first DWORD position that needs to be fixed
		fixups += 4;												// move to the fixups array

		while(1) {
			DWORD oldVal = *((DWORD *)pWhereToFix);
			DWORD newVal = oldVal + sBasePage->tbase;

            if(fixup != 1) {                                        // fix the value only if the fixup isn't ONE
                *((DWORD *)pWhereToFix) = newVal;
            }

			fixup = *fixups;
			fixups++;

			if(fixup == 0) {                                        // terminate fixup
				break;
			}

			if(fixup == 1) {                                        // just move forward by 0xfe
				pWhereToFix += 0xfe;
			} else {                                                // move forward and fixup
                pWhereToFix += (DWORD) fixup;
            }
		}
	}

	memset((BYTE *) sBasePage->bbase, 0, sBasePage->blen);			// clear BSS section

    if(mode == PE_LOADGO) {                                         // if we're doing PE_LOADGO, then we're going to freeTheBasePage()
        pLastBasePage = pBasePage;
		pexec_postProc = 1;											// mark that after this function ends, the asm handler should do PE_GO part...
    }

    // Return the pointer to basepage.
    // for PE_LOAD this will be used as return value.
    // for PE_LOADGO this will be used to call Pexec(PE_GO) in gemdos_asm.s after this custom function.
	return (DWORD) pBasePage;										// the PE_LOAD was successful
}

int32_t custom_pterm( void *sp )
{
	DWORD res = 0;
	WORD result		= *((WORD *) sp);

    if(pLastBasePage != 0) {                                        // did we allocate this?
        freeTheBasePage((TBasePage *) pLastBasePage);               // free the base page
        pLastBasePage = 0;
    }

    CALL_OLD_GD_VOIDRET(Pterm, result);

	return res;
}

int32_t custom_pterm0( void *sp )
{
	DWORD res = 0;

    if(pLastBasePage != 0) {                                        // did we allocate this?
        freeTheBasePage((TBasePage *) pLastBasePage);               // free the base page
        pLastBasePage = 0;
    }

    CALL_OLD_GD_VOIDRET(Pterm0);

	return res;
}

void freeTheBasePage(TBasePage *basePage)
{
	Mfree(basePage->env);											// free the environment
	Mfree(basePage);												// free the base page
}
