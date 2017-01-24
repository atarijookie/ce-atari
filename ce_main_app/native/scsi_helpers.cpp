// vim: expandtab shiftwidth=4 tabstop=4 softtabstop=4
#include <string.h>
#include <stdio.h>

#include "scsi_defs.h"
#include "scsi.h"
#include "../global.h"
#include "../debug.h"
#include "devicemedia.h"
#include "imagefilemedia.h"

//----------------------------------------------
bool Scsi::isICDcommand(void)
{
    if((cmd[0] & 0x1f)==0x1f) {              // if the command is '0x1f'
        return true;
    }

    return false;
}

//----------------------------------------------
void Scsi::storeSenseAndSendStatus(BYTE status, BYTE senseKey, BYTE additionalSenseCode, BYTE ascq)
{
    // store sense for REQUEST SENSE command
    devInfo[acsiId].LastStatus  = status;
    devInfo[acsiId].SCSI_SK     = senseKey;
    devInfo[acsiId].SCSI_ASC    = additionalSenseCode;
    devInfo[acsiId].SCSI_ASCQ   = ascq;

    // send current status
    dataTrans->setStatus(status);
}

//----------------------------------------------
void Scsi::returnInvalidCommand(void)
{
    storeSenseAndSendStatus(SCSI_ST_CHECK_CONDITION, SCSI_E_IllegalRequest, SCSI_ASC_InvalidCommandOperationCode, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
}

//----------------------------------------------
void Scsi::ReturnUnitAttention(void)
{
    dataMedia->setMediaChanged(false);

    storeSenseAndSendStatus(SCSI_ST_CHECK_CONDITION, SCSI_E_UnitAttention, SCSI_ASC_NOT_READY_TO_READY_TRANSITION, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
}

//----------------------------------------------
void Scsi::ReturnStatusAccordingToIsInit(void)
{
    if(dataMedia->isInit()) {
        SendOKstatus();
    } else {
        storeSenseAndSendStatus(SCSI_ST_CHECK_CONDITION, SCSI_E_NotReady, SCSI_ASC_MEDIUM_NOT_PRESENT, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
    }
}

//----------------------------------------------
void Scsi::SendOKstatus(void)
{
    storeSenseAndSendStatus(SCSI_ST_OK, SCSI_E_NoSense, SCSI_ASC_NO_ADDITIONAL_SENSE, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
}

//----------------------------------------------
void Scsi::ClearTheUnitAttention(void)
{
    storeSenseAndSendStatus(SCSI_ST_OK, SCSI_E_NoSense, SCSI_ASC_NO_ADDITIONAL_SENSE, SCSI_ASCQ_NO_ADDITIONAL_SENSE);
    dataMedia->setMediaChanged(false);
}

//----------------------------------------------
void Scsi::showCommand(WORD id, WORD length, WORD errCode)
{
    char tmp[64];

    memset(tmp, 0, 64);
    sprintf(tmp, "%d - ", id);

    WORD i;

    for(i=0; i<length; i++) {
        sprintf(tmp + 4 + i*3, "%02x ", cmd[i]);
    }

    int len = strlen(tmp);
    sprintf(tmp + len, "- %02x", errCode);
}
//----------------------------------------------
void Scsi::ICD7_to_SCSI6(void)
{
    cmd[0] = cmd[1];
    cmd[1] = cmd[2];
    cmd[2] = cmd[3];
    cmd[3] = cmd[4];
    cmd[4] = cmd[5];
    cmd[5] = cmd[6];
}

//---------------------------------------------
const char * Scsi::SourceTypeStr(int sourceType)
{
    switch(sourceType) {
        case SOURCETYPE_NONE:                   return "NONE";
        case SOURCETYPE_IMAGE:                  return "IMAGE";
        case SOURCETYPE_IMAGE_TRANSLATEDBOOT:   return "TRANSLATEDBOOT";
        case SOURCETYPE_DEVICE:                 return "DEVICE";
        case SOURCETYPE_SD_CARD:                return "SD_CARD";
        case SOURCETYPE_TESTMEDIA:              return "TESTMEDIA";
        default:                                return "*UNKNOWN*";
    }
}
