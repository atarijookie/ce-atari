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
#include "../libacsiscsi/acsi.h"
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
	uint32_t lowtpa;
	uint32_t hitpa;
	uint32_t tbase;
	uint32_t tlen;
	uint32_t dbase;
	uint32_t dlen;
	uint32_t bbase;
	uint32_t blen;
	uint32_t dta;
	uint32_t parent;
	uint32_t reserved;
	uint32_t env;
} TBasePage;

typedef struct __attribute__ ((__packed__))
{
	uint16_t  magic;
	uint32_t tsize;
	uint32_t dsize;
	uint32_t bsize;
	uint32_t ssize;
	uint32_t res1;
	uint32_t prgFlags;
	uint16_t  absFlag;
} TPrgHead;

void freeTheBasePage(TBasePage *basePage);

static uint8_t *pLastBasePage = 0;

extern uint16_t pexec_postProc;
extern uint16_t pexec_callOrig;

int32_t custom_pexec2(void *res);

// the following global variables are used in both custom_pexec and custom_pexec2
static char *fname, *cmdline, *envstr;
static uint8_t *pPrgStart;
static uint16_t mode;

// ------------------------------------------------------------------
// LONG Pexec( mode, fname, cmdline, envstr )
int32_t custom_pexec( void *sp )
{
	uint8_t *params = (uint8_t *) sp;

	// retrieve params from stack
	mode    = *((uint16_t *) params);
	params += 2;
	fname	= (char *)	*((uint32_t *) params);
	params += 4;
	cmdline	= (char *)	*((uint32_t *) params);
	params += 4;
	envstr	= (char *)	*((uint32_t *) params);

	// for any other than these modes don't do anything special, just call the original
	if(mode != PE_LOADGO && mode != PE_LOAD) {                              // not one of 2 supported modes? Call original Pexec()
        pexec_callOrig = 1;                                                 // will call the original Pexec() handler from asm when this finishes
        return 0;
	}

	// if we got here, the mode is PE_LOADGO || PE_LOAD
	uint16_t drive = getDriveFromPath((char *) fname);

	if(!isOurDrive(drive, 0)) {												// not our drive? Call original Pexec()
        pexec_callOrig = 1;                                                 // will call the original Pexec() handler from asm when this finishes
        return 0;
	}

    // Do PE_LOAD, and if this was PE_LOADGO, then the GO part will be done in gemdos_asm.s

	// if we got here, then it's a PRG on our drive...

	uint8_t prgStart[32];

	pPrgStart = &prgStart[4];
	pPrgStart = (uint8_t *) (((uint32_t) pPrgStart) & 0xfffffffc);				// make temp buffer pointer to be at multiple of 4

	// create base page
    uint8_t *pBasePage = (uint8_t *) Pexec(PE_BASEPAGE, 0, cmdline, envstr);

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

	uint32_t diskProgSize = Fseek(0, file, 2);									// seek to end, returns file size (bytes before end)
	Fseek(0, file, 0);														// seek to the start

	if(diskProgSize < 28) {													// if the program is too small, this wouldn't work
		Fclose(file);														// close the file
		freeTheBasePage(sBasePage);											// free the base page
		return EPLFMT;														// be Invalid program load format
	}

	Fread(file, 28, pPrgStart);												// read first 28 bytes to this buffer
	TPrgHead *prgHead = (TPrgHead  *) pPrgStart;

	// get file size, see if it will fit in the free memory
	uint32_t memProgSize		= prgHead->tsize + prgHead->dsize + prgHead->bsize + prgHead->ssize;	// calculate the program size in RAM as size of text + data + bss + symbols
	uint32_t memoryAvailable	= sBasePage->hitpa - sBasePage->lowtpa;									// calculate how much memory we have for the program

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
	uint8_t *fixups		= (uint8_t *) (sBasePage->tbase + prgHead->tsize + prgHead->dsize + prgHead->ssize);
	uint32_t fixupOffset	= *((uint32_t *) fixups);

    uint8_t fixup = 0;

	if(fixupOffset != 0) {						                    // if fixup needed?
		uint8_t *pWhereToFix;

		pWhereToFix	= (uint8_t *) (sBasePage->tbase + fixupOffset);	// calculate the first uint32_t position that needs to be fixed
		fixups += 4;												// move to the fixups array

		while(1) {
			uint32_t oldVal = *((uint32_t *)pWhereToFix);
			uint32_t newVal = oldVal + sBasePage->tbase;

            if(fixup != 1) {                                        // fix the value only if the fixup isn't ONE
                *((uint32_t *)pWhereToFix) = newVal;
            }

			fixup = *fixups;
			fixups++;

			if(fixup == 0) {                                        // terminate fixup
				break;
			}

			if(fixup == 1) {                                        // just move forward by 0xfe
				pWhereToFix += 0xfe;
			} else {                                                // move forward and fixup
                pWhereToFix += (uint32_t) fixup;
            }
		}
	}

	memset((uint8_t *) sBasePage->bbase, 0, sBasePage->blen);			// clear BSS section

    if(mode == PE_LOADGO) {                                         // if we're doing PE_LOADGO, then we're going to freeTheBasePage()
        pLastBasePage = pBasePage;
		pexec_postProc = 1;											// mark that after this function ends, the asm handler should do PE_GO part...
    }

    // Return the pointer to basepage.
    // for PE_LOAD this will be used as return value.
    // for PE_LOADGO this will be used to call Pexec(PE_GO) in gemdos_asm.s after this custom function.
	return (uint32_t) pBasePage;										// the PE_LOAD was successful
}

int32_t custom_pterm( void *sp )
{
	uint32_t res = 0;
	uint16_t result		= *((uint16_t *) sp);

    if(pLastBasePage != 0) {                                        // did we allocate this?
        freeTheBasePage((TBasePage *) pLastBasePage);               // free the base page
        pLastBasePage = 0;
    }

    CALL_OLD_GD_VOIDRET(Pterm, result);

	return res;
}

int32_t custom_pterm0( void *sp )
{
	uint32_t res = 0;

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
