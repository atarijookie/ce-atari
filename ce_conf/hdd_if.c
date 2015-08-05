#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "acsi.h"
#include "hdd_if.h"

THDif hdIf;

void hdd_if_select(int ifType)
{
    switch(ifType) {
        case IF_ACSI:           // for ACSI
            hdIf.cmd                = (THddIfCmd) acsi_cmd;
            hdIf.pSetReg            = NULL;
            hdIf.pGetReg            = NULL;
            hdIf.pDmaDataTx_prepare = NULL;
            hdIf.pDmaDataTx_do      = NULL;
            hdIf.scsiHostId         = 0xff;
            break;

        case IF_SCSI_TT:        // for TT SCSI
            hdIf.cmd                = (THddIfCmd)           scsi_cmd_TT;
            hdIf.pSetReg            = (TsetReg)             scsi_setReg_TT;
            hdIf.pGetReg            = (TgetReg)             scsi_getReg_TT;
            hdIf.pDmaDataTx_prepare = (TdmaDataTx_prepare)  dmaDataTx_prepare_TT;
            hdIf.pDmaDataTx_do      = (TdmaDataTx_do)       dmaDataTx_do_TT;

            hdIf.scsiHostId         = 7;               // SCSI ID 7 is reserved by host 
            break;

        case IF_SCSI_FALCON:    // for Falcon SCSI
            hdIf.cmd                = (THddIfCmd) scsi_cmd_TT;

            hdIf.pSetReg            = (TsetReg)             scsi_setReg_Falcon;
            hdIf.pGetReg            = (TgetReg)             scsi_getReg_Falcon;
            hdIf.pDmaDataTx_prepare = (TdmaDataTx_prepare)  dmaDataTx_prepare_Falcon;
            hdIf.pDmaDataTx_do      = (TdmaDataTx_do)       dmaDataTx_do_Falcon;

            hdIf.scsiHostId         = 0;               // SCSI ID 0 is reserved by host 
            break;
            
        default:
            hdIf.cmd        = NULL;
            hdIf.pSetReg    = NULL;
            hdIf.pGetReg    = NULL;
            hdIf.scsiHostId = 0xff;
            break;            
	} 
}

