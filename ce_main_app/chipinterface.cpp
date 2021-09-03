#include <string.h>

#include "chipinterface.h"
#include "global.h"
#include "debug.h"

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

    bfr[response.currentLength + 0] = (uint8_t) (value >> 8);
    bfr[response.currentLength + 1] = (uint8_t) (value & 0xff);
    response.currentLength += 2;
}

void ChipInterface::responseAddByte(uint8_t *bfr, uint8_t value)        // add a uint8_t to the response (command) to Hans or Franz
{
    if(response.currentLength >= response.bfrLengthInBytes) {
        return;
    }

    bfr[response.currentLength] = value;
    response.currentLength++;
}

void ChipInterface::setHDDconfig(uint8_t hddEnabledIDs, uint8_t sdCardId, uint8_t fddEnabledSlots, bool setNewFloppyImageLed, uint8_t newFloppyImageLed)
{
    memset(fwResponseBfr, 0, HDD_FW_RESPONSE_LEN);

    // uint16_t sent (bytes shown): 01 23 45 67

    responseStart(HDD_FW_RESPONSE_LEN);                         // init the response struct

    hansConfigWords.next.acsi   = MAKEWORD(hddEnabledIDs, sdCardId);
    hansConfigWords.next.fdd    = MAKEWORD(fddEnabledSlots, 0);

    if( (hansConfigWords.next.acsi  != hansConfigWords.current.acsi) ||
        (hansConfigWords.next.fdd   != hansConfigWords.current.fdd )) {

        // hansConfigWords.skipNextSet - it's a flag used for skipping one config sending, because we send the new config now, but receive it processed in the next (not this) fw version packet

        if(!hansConfigWords.skipNextSet) {
            responseAddWord(fwResponseBfr, CMD_ACSI_CONFIG);             // CMD: send acsi config
            responseAddWord(fwResponseBfr, hansConfigWords.next.acsi);   // store ACSI enabled IDs and which ACSI ID is used for SD card
            responseAddWord(fwResponseBfr, hansConfigWords.next.fdd);    // store which floppy images are enabled

            hansConfigWords.skipNextSet = true;                 // we have just sent the config, skip the next sending, so we won't send it twice in a row
        } else {                                                // if we should skip sending config this time, then don't skip it next time (if needed)
            hansConfigWords.skipNextSet = false;
        }
    }

    //--------------
    if(flags.deviceGetLicense) {                                // should the device get new hw license?
        flags.deviceGetLicense = false;
        responseAddWord(fwResponseBfr, CMD_GET_LICENSE);
    }

    if(flags.deviceDoUpdate) {                                  // should the device do the ?
        flags.deviceDoUpdate = false;
        responseAddWord(fwResponseBfr, CMD_DO_UPDATE);
    }

    //--------------
    if(setNewFloppyImageLed) {
        responseAddWord(fwResponseBfr, CMD_FLOPPY_SWITCH);               // CMD: set new image LED (bytes 8 & 9)
        responseAddWord(fwResponseBfr, MAKEWORD(fddEnabledSlots, newFloppyImageLed));  // store which floppy images LED should be on
    }
}

void ChipInterface::setFDDconfig(bool setFloppyConfig, bool fddEnabled, int id, int writeProtected, bool setDiskChanged, bool diskChanged)
{
    memset(fwResponseBfr, 0, FDD_FW_RESPONSE_LEN);

    responseStart(FDD_FW_RESPONSE_LEN);                             // init the response struct

    if(setFloppyConfig) {                                   // should set floppy config?
        responseAddByte(fwResponseBfr, ( fddEnabled     ? CMD_DRIVE_ENABLED     : CMD_DRIVE_DISABLED) );
        responseAddByte(fwResponseBfr, ((id == 0)       ? CMD_SET_DRIVE_ID_0    : CMD_SET_DRIVE_ID_1) );
        responseAddByte(fwResponseBfr, ( writeProtected ? CMD_WRITE_PROTECT_ON  : CMD_WRITE_PROTECT_OFF) );
    }

    if(setDiskChanged) {
        responseAddByte(fwResponseBfr, ( diskChanged    ? CMD_DISK_CHANGE_ON    : CMD_DISK_CHANGE_OFF) );
    }
}

void ChipInterface::setInstanceIndex(int index)
{
    instanceIndex = index;
}

int  ChipInterface::getInstanceIndex(void)
{
    return instanceIndex;
}
