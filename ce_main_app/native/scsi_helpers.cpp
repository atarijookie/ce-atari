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
void Scsi::storeSenseAndSendStatus(uint8_t status, uint8_t senseKey, uint8_t additionalSenseCode, uint8_t ascq)
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
void Scsi::showCommand(uint16_t id, uint16_t length, uint16_t errCode)
{
    char tmp[64];

    memset(tmp, 0, 64);
    sprintf(tmp, "%d - ", id);

    uint16_t i;

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

const char * Scsi::getCommandName(uint8_t cmd)
{
    switch(cmd) {
        case SCSI_C_WRITE6:             return "WRITE(6)";
        case SCSI_C_READ6:              return "READ(6)";
        case SCSI_C_MODE_SENSE6:        return "MODE_SENSE";
        case SCSI_C_START_STOP_UNIT:    return "START_STOP_UNIT";
        case SCSI_C_FORMAT_UNIT:        return "FORMAT_UNIT";
        case SCSI_C_INQUIRY:            return "INQUIRY";
        case SCSI_C_REQUEST_SENSE:      return "REQUEST_SENSE";
        case SCSI_C_TEST_UNIT_READY:    return "TEST_UNIT_READY";
        case SCSI_C_SEND_DIAGNOSTIC:    return "SEND_DIAGNOSTIC";
        case SCSI_C_RESERVE:            return "RESERVE";
        case SCSI_C_RELEASE:            return "RELEASE";
        case SCSI_C_WRITE10:            return "WRITE(10)";
        case SCSI_C_READ10:             return "READ(10)";
        case SCSI_C_WRITE_LONG:         return "WRITE_LONG";
        case SCSI_C_READ_LONG:          return "READ_LONG";
        case SCSI_C_READ_CAPACITY:      return "READ_CAPACITY";
        case SCSI_C_VERIFY:             return "VERIFY";
        default:                        return "UNKNOWN";
    }
}
