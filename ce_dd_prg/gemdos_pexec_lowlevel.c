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
#include "../libacsiscsi/hdd_if.h"
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

extern uint16_t pexec_callOrig;

#define PEXEC_CREATE_IMAGE      0
#define PEXEC_GET_BPB           1
#define PEXEC_READ_SECTOR       2

static uint8_t readRwabsSectors(uint16_t startingSector, uint16_t sectorCount, uint8_t *pBuffer);
extern uint8_t FastRAMBuffer[];

static char fakePrgPath[256];

// ------------------------------------------------------------------
// LONG Pexec( mode, fname, cmdline, envstr )
int32_t custom_pexec_lowlevel( void *sp )
{
	uint8_t *params = (uint8_t *) sp;

    uint16_t  mode;
    char *fname;

	// retrieve params from stack
	mode    = *((uint16_t *) params);
	params += 2;
    fname	= (char *)	*((uint32_t *) params);

	// for any other than these modes don't do anything special, just call the original
	if(mode != PE_LOADGO && mode != PE_LOAD) {              // not one of 2 supported modes? Call original Pexec()
        pexec_callOrig = 1;                                 // will call the original Pexec() handler from asm when this finishes
        return 0;
	}

	// if we got here, the mode is PE_LOADGO || PE_LOAD
	uint16_t drive = getDriveFromPath((char *) fname);

	if(!isOurDrive(drive, 0)) {							    // not our drive? Call original Pexec()
        pexec_callOrig = 1;                                 // will call the original Pexec() handler from asm when this finishes
        return 0;
	}

    //----------
    // tell host to create image from this PRG
	commandLong[5] = GEMDOS_pexec;                          // store GEMDOS function number
	commandLong[6] = PEXEC_CREATE_IMAGE;                    // and sub function number

	pDmaBuffer[0] = (uint8_t) (mode >> 8);                     // store mode
	pDmaBuffer[1] = (uint8_t) (mode     );
	strncpy(((char *) pDmaBuffer) + 2, fname, DMA_BUFFER_SIZE - 2);         // copy in the file name

	(*hdIf.cmd)(ACSI_WRITE, commandLong, CMD_LENGTH_LONG, pDmaBuffer, 1);   // send command to host over ACSI

    if(!hdIf.success || hdIf.statusByte != E_OK) {			// not handled or error?
        pexec_callOrig = 1;                                 // will call the original Pexec() handler from asm when this finishes
        return 0;
	}

    //----------
    // retrieve BPB structure, which we will return on each request

	commandLong[5] = GEMDOS_pexec;                          // store GEMDOS function number
	commandLong[6] = PEXEC_GET_BPB;                         // and sub function number

	(*hdIf.cmd)(ACSI_READ, commandLong, CMD_LENGTH_LONG, pDmaBuffer, 1);    // send command to host over ACSI

    if(!hdIf.success || hdIf.statusByte != E_OK) {		    // not handled or error?
        pexec_callOrig = 1;                                 // will call the original Pexec() handler from asm when this finishes
        return 0;
	}

    //----------
    // set the variables
    int virtualDriveIndex = pDmaBuffer[18];                 // store number of drive we should emulate (should be same as 'drive' variable)

    updateCeMediach();                                      // update the mediach status - once per 3 seconds
    ceMediach       |= (1 << virtualDriveIndex);            // this drive has in MEDIA CHANGE

    //----------
#define PEXEC_FAKE_ROOT_PATH

#ifdef PEXEC_FAKE_ROOT_PATH
    // if should fake root path location of PRG, then copy the new fake path to original path
    char *pFakePath = (char *) pDmaBuffer + 384;
    int   fakeLen   = strlen(pFakePath);

    memcpy(fakePrgPath, pFakePath, fakeLen + 1);            // copy it to temporary buffer

    uint32_t *pSpFakePath  = (uint32_t *) (((uint8_t *) sp) + 2);    // pointer to stack, where the pointer to filename is
    *pSpFakePath        = (uint32_t) fakePrgPath;              // now update the value under the pointer, so the filename on stack will now point to fakePrgPath instead of original pointer
#endif

    //----------
    // set DRVBITS variable
    #define DRVBITS     ((uint32_t *) 0x4c2)
    uint32_t drvBits    = *DRVBITS;                            // read DRVBITS
    drvBits         |= (1 << virtualDriveIndex);            // add our Pexec() RAW drive
    *DRVBITS         = drvBits;                             // put it back updated

    //----------
    pexec_callOrig      = 1;                                // now let the original Pexec() do the real work
	return 0;
}

//--------------------------------------------------

uint32_t myCRwabs(uint8_t *sp)
{
	uint8_t *params = (uint8_t *) sp;

    uint16_t  mode              =          *(( uint16_t *) params);
	params += 2;
    uint8_t *pBuffer           = (uint8_t *) *((uint32_t *) params);
	params += 4;
    uint16_t  sectorCount       =          *(( uint16_t *) params);
	params += 2;
    uint16_t  startingSector    =          *(( uint16_t *) params);
	params += 2;
    uint16_t  device            =          *(( uint16_t *) params);

    if((ceDrives & (1 << device)) == 0) {   // not our drive? fail
        return -5;                          // Bad request
    }

    if(ceMediach & (1 << device)) {         // this drive has changed?
        return -14;                         // Media change detected
    }

    if(mode & 1) {                          // write (from ST to drive)? Not supported.
        return -12;                         // Device is write protected
    }

    uint8_t  toFastRam     =  (((uint32_t) pBuffer) >= 0x1000000) ? TRUE  : FALSE;        // flag: are we reading to FAST RAM?
    uint8_t  bufAddrIsOdd  = ((((uint32_t) pBuffer) & 1) == 0)    ? FALSE : TRUE;         // flag: buffer pointer is on ODD address?
    uint8_t  useMidBuffer  = (toFastRam || bufAddrIsOdd);                              // flag: is load to fast ram or on odd address, use middle buffer

    uint32_t maxSectorCount = useMidBuffer ? (FASTRAM_BUFFER_SIZE / 512) : MAXSECTORS; // how many sectors we can read at once - if going through middle buffer then middle buffer size, otherwise max sector coun
    uint8_t res;

    while(sectorCount > 0) {
        uint32_t thisSectorCount   = (sectorCount < maxSectorCount) ? sectorCount : maxSectorCount;    // will the needed read size be within the blockSize, or not?
        uint32_t thisByteCount     = thisSectorCount << 9;

        if(useMidBuffer) {          // through middle buffer?
            res = readRwabsSectors(startingSector, thisSectorCount, FastRAMBuffer);
            memcpy(pBuffer, FastRAMBuffer, thisByteCount);
        } else {                    // directly to final buffer?
            res = readRwabsSectors(startingSector, thisSectorCount, pBuffer);
        }

        if(!res) {      // if failed, fail and quit
            return -11; // Read fault
        }

        sectorCount     -= thisSectorCount;         // now we need to read less sectors
        startingSector  += thisSectorCount;         // advance to next sectors

        pBuffer         += thisByteCount;           // advance in the buffer
    }

    return 0;           // success
}
//--------------------------------------------------
uint8_t readRwabsSectors(uint16_t startingSector, uint16_t sectorCount, uint8_t *pBuffer)
{
    commandLong[ 5] = GEMDOS_pexec;                 // store GEMDOS function number
	commandLong[ 6] = PEXEC_READ_SECTOR;            // and sub function number

    commandLong[ 7] = (uint8_t) (startingSector >> 8);
    commandLong[ 8] = (uint8_t) (startingSector     );

    commandLong[ 9] = (uint8_t) (sectorCount    >> 8);
    commandLong[10] = (uint8_t) (sectorCount        );

	(*hdIf.cmd)(ACSI_READ, commandLong, CMD_LENGTH_LONG, pBuffer, sectorCount); // send command to host over ACSI

    if(!hdIf.success || hdIf.statusByte != E_OK) {	// not handled or error?
        return 0;   // bad
	}

    return 1;       // good
}
//--------------------------------------------------
