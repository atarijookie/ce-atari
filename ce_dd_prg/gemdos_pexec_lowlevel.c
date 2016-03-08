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
#include "hdd_if.h"

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

extern BYTE dmaBuffer[DMA_BUFFER_SIZE + 2];
extern BYTE *pDmaBuffer;

extern BYTE deviceID;
extern BYTE commandShort[CMD_LENGTH_SHORT];
extern BYTE commandLong[CMD_LENGTH_LONG];

extern WORD ceDrives;
extern BYTE currentDrive;

#define PE_LOADGO		0
#define PE_LOAD			3
#define PE_GO			4
#define PE_BASEPAGE		5
#define PE_GOTHENFREE	6

extern WORD pexec_callOrig;

       _BPB  virtualBpbStruct;
extern DWORD virtualBpbPointer;
       
extern WORD virtualDriveIndex;
extern WORD virtualHddEnabled;
extern WORD virtualDriveChanged;

#define PEXEC_CREATE_IMAGE      0
#define PEXEC_GET_BPB           1
#define PEXEC_READ_SECTOR       2

// ------------------------------------------------------------------ 
// LONG Pexec( mode, fname, cmdline, envstr )
int32_t custom_pexec_lowlevel( void *sp )
{
	BYTE *params = (BYTE *) sp;

    WORD  mode;
    char *fname;
    
	// retrieve params from stack 
	mode    = *((WORD *) params);
	params += 2;
    fname	= (char *)	*((DWORD *) params);

    virtualHddEnabled = 0;                                  // disable our custom hard drive
    
	// for any other than these modes don't do anything special, just call the original
	if(mode != PE_LOADGO && mode != PE_LOAD) {              // not one of 2 supported modes? Call original Pexec()
        pexec_callOrig = 1;                                 // will call the original Pexec() handler from asm when this finishes
        return 0;
	}
	
	// if we got here, the mode is PE_LOADGO || PE_LOAD
	WORD drive = getDriveFromPath((char *) fname);

	if(!isOurDrive(drive, 0)) {							    // not our drive? Call original Pexec()
        pexec_callOrig = 1;                                 // will call the original Pexec() handler from asm when this finishes
        return 0;
	}

    //----------
    // tell host to create image from this PRG 
	commandLong[5] = GEMDOS_pexec;                          // store GEMDOS function number 
	commandLong[6] = PEXEC_CREATE_IMAGE;                    // and sub function number
	
	pDmaBuffer[0] = (BYTE) (mode >> 8);                     // store mode
	pDmaBuffer[1] = (BYTE) (mode     );
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
    
    memcpy(&virtualBpbStruct, pDmaBuffer, sizeof(_BPB));    // copy in the BPB
    
    //----------
    // set the variables
    virtualBpbPointer   = (DWORD) &virtualBpbStruct;        // store pointer to virtual drive BPB
    virtualDriveIndex   = drive;                            // store number of drive we should emulate
    virtualHddEnabled   = 1;                                // now enable our custom hard drive
    virtualDriveChanged = 2;                                // mark that the virtual drive has changed (2 = MED_CHANGED)
    
    pexec_callOrig      = 1;                                // now let the original Pexec() do the real work
	return 0;                       
}

//--------------------------------------------------

DWORD myRwabs(BYTE *sp)
{
	BYTE *params = (BYTE *) sp;
    
    WORD  mode   =          *(( WORD *) params);
	params += 2;
    BYTE *buf    = (BYTE *) *((DWORD *) params);
	params += 4;
    WORD  count  =          *(( WORD *) params);
	params += 2;
    WORD  start  =          *(( WORD *) params);
	params += 2;
    WORD  device =          *(( WORD *) params);
    
    if(!isOurDrive(device, 0)) {                    // not our drive? fail
        return -1;
	}
    
    if((start + count) > virtualBpbStruct.numcl) {  // out of range?
        return -1;
    }
    
    if(mode & 1) {                                  // write (from ST to drive)? Not supported.
        return -1;
    } 

    commandLong[ 5] = GEMDOS_pexec;                 // store GEMDOS function number 
	commandLong[ 6] = PEXEC_READ_SECTOR;            // and sub function number
	
    commandLong[ 7] = (BYTE) (start >> 8);
    commandLong[ 8] = (BYTE) (start     );
    
    commandLong[ 9] = (BYTE) (count >> 8);
    commandLong[10] = (BYTE) (count     );
    
	(*hdIf.cmd)(ACSI_READ, commandLong, CMD_LENGTH_LONG, buf, count);   // send command to host over ACSI 

    if(!hdIf.success || hdIf.statusByte != E_OK) {	// not handled or error? 
        return -1;
	}
    
    return 0;
}
//--------------------------------------------------
