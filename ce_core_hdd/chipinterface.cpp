#include <string.h>

#include "chipinterface.h"
#include "global.h"
#include "debug.h"
#include "utils.h"

extern THwConfig  hwConfig;
extern TFlags     flags;

ChipInterface::ChipInterface()
{
    instanceIndex = -1;
}

void ChipInterface::convertXilinxInfo(uint8_t xilinxInfo)
{
    THwConfig hwConfigOld = hwConfig;

    switch(xilinxInfo) {
        // GOOD
        case 0x21:  hwConfig.version        = 2;                        // v.2
                    hwConfig.hddIface       = HDD_IF_ACSI;              // HDD int: ACSI
                    hwConfig.fwMismatch     = false;
                    break;

        // GOOD
        case 0x22:  hwConfig.version        = 2;                        // v.2
                    hwConfig.hddIface       = HDD_IF_SCSI;              // HDD int: SCSI
                    hwConfig.fwMismatch     = false;
                    break;

        // BAD: SCSI HW, ACSI FW
        case 0x29:  hwConfig.version        = 2;                        // v.2
                    hwConfig.hddIface       = HDD_IF_SCSI;              // HDD int: SCSI
                    hwConfig.fwMismatch     = true;                     // HW + FW mismatch!
                    break;

        // BAD: ACSI HW, SCSI FW
        case 0x2a:  hwConfig.version        = 2;                        // v.2
                    hwConfig.hddIface       = HDD_IF_ACSI;              // HDD int: ACSI
                    hwConfig.fwMismatch     = true;                     // HW + FW mismatch!
                    break;

        // GOOD
        case 0x31:  hwConfig.version        = 3;                        // v.3
                    hwConfig.hddIface       = HDD_IF_ACSI;              // HDD int: ACSI
                    hwConfig.fwMismatch     = false;
                    break;

        // GOOD
        case 0x32:  hwConfig.version        = 3;                        // v.3
                    hwConfig.hddIface       = HDD_IF_SCSI;              // HDD int: SCSI
                    hwConfig.fwMismatch     = false;
                    break;

        // GOOD
        case 0x41:  hwConfig.version        = 4;                        // v.4
                    hwConfig.hddIface       = HDD_IF_ACSI;              // HDD int: ACSI
                    hwConfig.fwMismatch     = false;
                    break;

        // GOOD
        case 0x42:  hwConfig.version        = 4;                        // v.4
                    hwConfig.hddIface       = HDD_IF_SCSI;              // HDD int: SCSI
                    hwConfig.fwMismatch     = false;
                    break;

        // GOOD
        case 0x82:  hwConfig.version        = 8;                        // RaSCSI
                    hwConfig.hddIface       = HDD_IF_SCSI;              // HDD int: SCSI
                    hwConfig.fwMismatch     = false;
                    break;

        // GOOD
        case 0x11:  // use this for v.1
        default:    // and also for all other cases
                    hwConfig.version        = 1;
                    hwConfig.hddIface       = HDD_IF_ACSI;
                    hwConfig.fwMismatch     = false;
                    break;
    }

    // if the HD IF changed (received the 1st HW info) and we're on SCSI bus, we need to send the new (limited) SCSI IDs to Hans, so he won't answer on Initiator SCSI ID
    if((hwConfigOld.hddIface != hwConfig.hddIface) && hwConfig.hddIface == HDD_IF_SCSI) {
        hwConfig.changed = true;
        Debug::out(LOG_DEBUG, "Found out that we're running on SCSI bus - will resend the ID bits configuration to Hans");
    }

    if(memcmp(&hwConfigOld, &hwConfig, sizeof(THwConfig)) != 0) {    // config changed? save it
        hwConfig.changed = true;
    }
}

void ChipInterface::responseStart(int bufferLengthInBytes)        // use this to start creating response (commands) to Hans or Franz
{
    response.bfrLengthInBytes   = bufferLengthInBytes;
    response.currentLength      = 0;
}

void ChipInterface::responseAddWord(uint8_t *bfr, uint16_t value)        // add a uint16_t to the response (command) to Hans or Franz
{
    if(response.currentLength >= response.bfrLengthInBytes) {
        return;
    }

    Utils::storeWord(&bfr[response.currentLength], value);
    response.currentLength += 2;
}

bool ChipInterface::responseAddByte(uint8_t *bfr, uint8_t value)        // add a uint8_t to the response (command) to Hans or Franz
{
    if(response.currentLength >= response.bfrLengthInBytes) {
        return false;
    }

    bfr[response.currentLength] = value;
    response.currentLength++;
    return true;
}

void ChipInterface::setHDDconfig(uint8_t hddEnabledIDs)
{
    memset(fwResponseBfr, 0, HDD_FW_RESPONSE_LEN);
    responseStart(HDD_FW_RESPONSE_LEN);                         // init the response struct

    responseAddByte(fwResponseBfr, CMD_ACSI_CONFIG);             // CMD: send acsi config
    responseAddByte(fwResponseBfr, hddEnabledIDs);               // store ACSI enabled IDs
}
