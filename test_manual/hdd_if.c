#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "acsi.h"
#include "hdd_if.h"

THddIfCmd           hddIfCmd                = NULL;
TsetReg             pSetReg                 = NULL;
TgetReg             pGetReg                 = NULL;

TdmaDataTx_prepare  pDmaDataTx_prepare      = NULL;
TdmaDataTx_do       pDmaDataTx_do           = NULL;

void hdd_if_select(int ifType)
{
    switch(ifType) {
        case IF_ACSI:           // for ACSI
            hddIfCmd            = (THddIfCmd) acsi_cmd;
            pSetReg             = NULL;
            pGetReg             = NULL;
            pDmaDataTx_prepare  = NULL;
            pDmaDataTx_do       = NULL;
            break;

        case IF_SCSI_TT:        // for TT SCSI
            hddIfCmd            = (THddIfCmd)           scsi_cmd_TT;
            pSetReg             = (TsetReg)             scsi_setReg_TT;
            pGetReg             = (TgetReg)             scsi_getReg_TT;
            pDmaDataTx_prepare  = (TdmaDataTx_prepare)  dmaDataTx_prepare_TT;
            pDmaDataTx_do       = (TdmaDataTx_do)       dmaDataTx_do_TT;
            break;

        case IF_SCSI_FALCON:    // for Falcon SCSI
            hddIfCmd            = (THddIfCmd) scsi_cmd_TT;
//          hddIfCmd            = (THddIfCmd) scsi_cmd_Falcon;

            pSetReg             = (TsetReg) scsi_setReg_Falcon;
            pGetReg             = (TgetReg) scsi_getReg_Falcon;
            pDmaDataTx_prepare  = (TdmaDataTx_prepare)  dmaDataTx_prepare_Falcon;
            pDmaDataTx_do       = (TdmaDataTx_do)       dmaDataTx_do_Falcon;
            break;
            
        default:
            hddIfCmd    = NULL;
            pSetReg     = NULL;
            pGetReg     = NULL;
            break;            
	} 
}

