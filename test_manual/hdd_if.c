#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdint.h>
#include <stdio.h>

#include "acsi.h"
#include "hdd_if.h"

THddIfCmd   hddIfCmd    = NULL;
TsetReg     pSetReg     = NULL;
TgetReg     pGetReg     = NULL;

void hdd_if_select(int ifType)
{
    switch(ifType) {
        case IF_ACSI:           // for ACSI
            hddIfCmd    = (THddIfCmd) acsi_cmd;
            pSetReg     = NULL;
            pGetReg     = NULL;
            break;

        case IF_SCSI_TT:        // for TT SCSI
            hddIfCmd    = (THddIfCmd) scsi_cmd_TT;
            pSetReg     = (TsetReg) scsi_setReg_TT;
            pGetReg     = (TgetReg) scsi_getReg_TT;
            break;

        case IF_SCSI_FALCON:    // for Falcon SCSI
            hddIfCmd    = (THddIfCmd) scsi_cmd_Falcon;
            pSetReg     = (TsetReg) scsi_setReg_Falcon;
            pGetReg     = (TgetReg) scsi_getReg_Falcon;
            break;
            
        default:
            hddIfCmd    = NULL;
            pSetReg     = NULL;
            pGetReg     = NULL;
            break;            
	} 
}

