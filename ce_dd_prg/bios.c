#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ce_dd_prg.h"
#include "acsi.h"
#include "hdd_if.h"
#include "translated.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "bios.h"
#include "main.h"

extern int16_t useOldBiosHandler;
#ifdef MANUAL_PEXEC /* else it is defined in harddrive_lowlevel.s */
WORD ceDrives;
#endif

int32_t custom_mediach( void *sp )
{
    DWORD res;
    
    WORD drive = (WORD) *((WORD *) sp);

    updateCeDrives();                                                   // update the drives - once per 3 seconds 
    
    if(!isOurDrive(drive, 0)) {                                         // if the drive is not our drive 
        CALL_OLD_BIOS(Mediach, drive);                                  // call the original function 
        return res;
    }
    
    updateCeMediach();                                                  // update the mediach status - once per 3 seconds 
    
    if((ceMediach & (1 << drive)) != 0) {                               // if bit is set, media changed 
        return 2;
    }

    return 0;                                                           // bit not set, media not changed 
}

int32_t custom_drvmap( void *sp )
{
    DWORD res;

    updateCeDrives();           // update the drives - once per 3 seconds 
    
    CALL_OLD_BIOS(Drvmap);      // call the original Drvmap() 

    res = res | ceDrives;       // return original drives + CE drives together 
    return res;
}

int32_t custom_getbpb( void *sp )
{
    DWORD res;
    WORD drive = (WORD) *((WORD *) sp);
    static WORD bpb[20/2];    // declare as WORD to have it aligned to even address

    updateCeDrives();                                                   // update the drives - once per 3 seconds 

    if(!isOurDrive(drive, 0)) {                                         // if the drive is not our drive 
        CALL_OLD_BIOS(Getbpb, drive);                                   // call the original function 
        return res;
    }

    WORD driveMaskInv = ~(1 << drive);
    ceMediach = ceMediach & driveMaskInv;                               // this drive is no longer in MEDIA CHANGED state

    commandShort[4] = BIOS_Getbpb;                                      // store BIOS function number 
    commandShort[5] = (BYTE) drive;                                     
    
    (*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);  // send command to host over ACSI 
    
    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {              // not handled or error? 
        CALL_OLD_BIOS(Getbpb, drive);
        return res;                                                     
    }
    
    ceMediach = ceMediach & (~(1 << drive));                            // remove this bit media changes 
    
    memcpy(bpb, pDmaBuffer, 18);                                       // copy in the results 
    return (DWORD) bpb;
}

/* 
this function updates the ceDrives variable from the status in host, 
but does this only once per 3 seconds, so you can call it often and 
it will quit then sooner without updating (hoping that nothing changed within 3 seconds) 
*/
void updateCeDrives(void)
{
    static DWORD lastCeDrivesUpdate = 0;
    DWORD now = *HZ_200;

    if((now - lastCeDrivesUpdate) < 600) {                                  // if the last update was less than 3 seconds ago, don't update 
        return;
    }
    
    lastCeDrivesUpdate = now;                                               // mark that we've just updated the ceDrives 
    
    // now do the real update 
    commandShort[4] = BIOS_Drvmap;                                          // store BIOS function number 
    commandShort[5] = 0;                                        
    
    (*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);  // send command to host over ACSI 
    
    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {                  // not handled or error? 
        return;                                                     
    }
    
    ceDrives = getWord(pDmaBuffer);                                         // read drives from dma buffer 
}

void updateCeMediach(void)
{
    static DWORD lastMediachUpdate = 0;
    DWORD now = *HZ_200;

    if((now - lastMediachUpdate) < 600) {                                   // if the last update was less than 3 seconds ago, don't update 
        return;
    }
    
    lastMediachUpdate = now;                                                // mark that we've just updated 
    
    // now do the real update 
    commandShort[4] = BIOS_Mediach;                                         // store BIOS function number 
    commandShort[5] = 0;                                        
    
    (*hdIf.cmd)(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);  // send command to host over ACSI 
    
    if(!hdIf.success || hdIf.statusByte == E_NOTHANDLED) {                  // not handled or error? 
        return;                                                     
    }
    
    ceMediach = getWord(pDmaBuffer);                                        // store current media change status 
}
