#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "acsi.h"
#include "hdd_if.h"
#include "translated.h"
#include "stdlib.h"
#include "mutex.h"

THDif hdIf;
extern volatile mutex mtx;  

void hddIfCmd_withRetries_lock(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
	hddIfCmd_withRetries_worker(readNotWrite, cmd, cmdLength, buffer, sectorCount, 1);
}

void hddIfCmd_withRetries_nolock(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount)
{
	hddIfCmd_withRetries_worker(readNotWrite, cmd, cmdLength, buffer, sectorCount, 0);
}

void hddIfCmd_withRetries_worker(BYTE readNotWrite, BYTE *cmd, BYTE cmdLength, BYTE *buffer, WORD sectorCount, BYTE lock)
{
	if( lock ){
		while( mtx!=0 ){}
		mutex_trylock( &mtx );
	}
	
    hdIf.retriesDoneCount = 0;              // set retries count to zero
 
    //--------------
    // first check if it's not the HOST ID, and if it is, just quit - it will never work :)
    BYTE scsiId = (cmd[0] >> 5);            // get only drive ID bits

    if(scsiId == hdIf.scsiHostId) {         // Trying to access reserved SCSI ID? Fail... (skip)
        hdIf.success = FALSE;
		if( lock ){
			mutex_unlock(&mtx);
		}
        return;
    }
    
    //--------------
    // now do the normal command
    (*hdIf.cmd_intern)(readNotWrite, cmd, cmdLength, buffer, sectorCount);      // try the correct command for the first time
    
    if(hdIf.success) {                      // if succeeded on the 1st time, quit
		if( lock ){
			mutex_unlock(&mtx);
		}
        return;
    }
    
    if(hdIf.maxRetriesCount < 1) {          // retries are disabled? quit
		if( lock ){
			mutex_unlock(&mtx);
		}
        return;
    }
    
    //--------------
    // if we got here, the original / normal command failed, so it's time to do the retries...
    BYTE retryCmd[32];
    memcpy(retryCmd, cmd, cmdLength);       // make copy of the original command
    
    if(cmdLength == 6) {                    // short command?
        retryCmd[3] = retryCmd[3] | 0x80;   // add highest bit to HOSTMOD_* to let device know it's a retry
    } else {                                // long command?
        retryCmd[4] = retryCmd[4] | 0x80;   // add highest bit to HOSTMOD_* to let device know it's a retry
    }
    
    while(1) {
        if(hdIf.retriesDoneCount >= hdIf.maxRetriesCount) {     // did we reach the maximum number of retries? quit
			if( lock ){
				mutex_unlock(&mtx);
			}
            return;
        }
    
        hdIf.retriesDoneCount++;            // increment the count of retries we have done
    
        (*hdIf.cmd_intern)(readNotWrite, retryCmd, cmdLength, buffer, sectorCount);      // try the retry command until we succeed
        
        if(hdIf.success) {                  // if succeeded, quit
			if( lock ){
				mutex_unlock(&mtx);
			}
            return;
        }
    }
}

void hdd_if_select(int ifType)
{
    hdIf.cmd = (THddIfCmd) hddIfCmd_withRetries_lock;
    hdIf.cmd_nolock = (THddIfCmd) hddIfCmd_withRetries_nolock;

    switch(ifType) {
        case IF_ACSI:           // for ACSI
            hdIf.cmd_intern         = (THddIfCmd) acsi_cmd;
            hdIf.pSetReg            = NULL;
            hdIf.pGetReg            = NULL;
            hdIf.pDmaDataTx_prepare = NULL;
            hdIf.pDmaDataTx_do      = NULL;
            hdIf.scsiHostId         = 0xff;
            break;

        case IF_SCSI_TT:        // for TT SCSI
            hdIf.cmd_intern         = (THddIfCmd)           scsi_cmd_TT;
            hdIf.pSetReg            = (TsetReg)             scsi_setReg_TT;
            hdIf.pGetReg            = (TgetReg)             scsi_getReg_TT;
            hdIf.pDmaDataTx_prepare = (TdmaDataTx_prepare)  dmaDataTx_prepare_TT;
            hdIf.pDmaDataTx_do      = (TdmaDataTx_do)       dmaDataTx_do_TT;

            hdIf.scsiHostId         = 7;               // SCSI ID 7 is reserved by host 
            break;

        case IF_SCSI_FALCON:    // for Falcon SCSI
            hdIf.cmd_intern         = (THddIfCmd) scsi_cmd_TT;

            hdIf.pSetReg            = (TsetReg)             scsi_setReg_Falcon;
            hdIf.pGetReg            = (TgetReg)             scsi_getReg_Falcon;
            hdIf.pDmaDataTx_prepare = (TdmaDataTx_prepare)  dmaDataTx_prepare_Falcon;
            hdIf.pDmaDataTx_do      = (TdmaDataTx_do)       dmaDataTx_do_Falcon;

            hdIf.scsiHostId         = 0;               // SCSI ID 0 is reserved by host 
            break;
            
        default:
            hdIf.cmd_intern = NULL;
            hdIf.pSetReg    = NULL;
            hdIf.pGetReg    = NULL;
            hdIf.scsiHostId = 0xff;
            break;            
	} 
}



