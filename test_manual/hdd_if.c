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
            hdIf.hddIfCmd            = (THddIfCmd) acsi_cmd;
            hdIf.pSetReg             = NULL;
            hdIf.pGetReg             = NULL;
            hdIf.pDmaDataTx_prepare  = NULL;
            hdIf.pDmaDataTx_do       = NULL;
            break;

        case IF_SCSI_TT:        // for TT SCSI
            hdIf.hddIfCmd            = (THddIfCmd)           scsi_cmd_TT;
            hdIf.pSetReg             = (TsetReg)             scsi_setReg_TT;
            hdIf.pGetReg             = (TgetReg)             scsi_getReg_TT;
            hdIf.pDmaDataTx_prepare  = (TdmaDataTx_prepare)  dmaDataTx_prepare_TT;
            hdIf.pDmaDataTx_do       = (TdmaDataTx_do)       dmaDataTx_do_TT;
            break;

        case IF_SCSI_FALCON:    // for Falcon SCSI
            hdIf.hddIfCmd            = (THddIfCmd) scsi_cmd_TT;
//          hdIf.hddIfCmd            = (THddIfCmd) scsi_cmd_Falcon;

            hdIf.pSetReg             = (TsetReg) scsi_setReg_Falcon;
            hdIf.pGetReg             = (TgetReg) scsi_getReg_Falcon;
            hdIf.pDmaDataTx_prepare  = (TdmaDataTx_prepare)  dmaDataTx_prepare_Falcon;
            hdIf.pDmaDataTx_do       = (TdmaDataTx_do)       dmaDataTx_do_Falcon;
            break;
            
        default:
            hdIf.hddIfCmd    = NULL;
            hdIf.pSetReg     = NULL;
            hdIf.pGetReg     = NULL;
            break;            
	} 
}

