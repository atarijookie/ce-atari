#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <net/if.h>

#include "misc/global.h"
#include "misc/debug.h"
#include "hdd/hddclient.h"
#include "translated/translateddisk.h"
#include "native/scsi.h"
#include "native/scsi_defs.h"
#include "../misc/utils.h"
#include "../misc/statusreport.h"

#define DEV_CHECK_TIME_MS       3000
#define UPDATE_CHECK_TIME       1000
#define INET_IFACE_CHECK_TIME   1000
#define UPDATE_SCRIPTS_TIME     10000

extern DebugVars    dbgVars;

HddClient::HddClient(ChipInterface* chipInterfaceIn, ClientInfo* ci, int aSharedMemFd, sem_t* aSharedMemSemaphore, uint8_t* aSharedMemPointer)
{
    chipInterface = chipInterfaceIn;
    this->ci = ci;

    hwConfig.changed = false;

    retryMod = new RetryModule();

    dataTrans = new AcsiDataTrans();
    dataTrans->setCommunicationObject(chipInterfaceIn, &ci->fdClient);
    dataTrans->setRetryObject(retryMod);

    scsi = new Scsi();
    scsi->setAcsiDataTrans(dataTrans);
    scsi->updateTranslatedBootMedia();   // update CE_DD bootsector with proper SCSI ID

    translated = new TranslatedDisk(dataTrans, &hwConfig, aSharedMemFd, aSharedMemSemaphore, aSharedMemPointer);

    memset(outBuf, 0, INBUF_SIZE);
    memset(inBuff, 0, INBUF_SIZE);
}

HddClient::~HddClient()
{
    delete dataTrans;
    delete retryMod;

    delete scsi;
    scsi = NULL;

    delete translated;
}

bool HddClient::handleHdd(uint8_t* inBuff)
{
    bool isAcsiCommand = false;

    switch(inBuff[3]) {
        case ATN_FW_VERSION:
            handleFwVersion_hans();
            break;

        case ATN_ACSI_COMMAND:
            isAcsiCommand = true;
            dbgVars.isInHandleAcsiCommand = 1;
            handleAcsiCommand(inBuff + 8);
            dbgVars.isInHandleAcsiCommand = 0;
        break;

    default:
        logHdd(LOG_ERROR, "HddClient received weird ATN code %02x waitForAtn()", inBuff[3]);
        break;
    }

    return isAcsiCommand;
}

void HddClient::handleAcsiCommand(uint8_t *bufIn)
{
    logHdd(LOG_DEBUG, "\n");

    dbgVars.prevAcsiCmdTime = dbgVars.thisAcsiCmdTime;
    dbgVars.thisAcsiCmdTime = Utils::getCurrentMs();

    uint8_t justCmd, tag1, tag2, module;
    uint8_t *pCmd;
    uint8_t isIcd = false;
    uint8_t wasHandled = false;

    uint8_t acsiId = bufIn[0] >> 5;                            // get just ACSI ID
    if(acsiIdInfo.acsiIDdevType[acsiId] == DEVTYPE_OFF) {    // if this ACSI ID is off, reply with error and quit
        logHdd(LOG_WARNING, "HddClient::handleAcsiCommand - acsiId %d is OFF, sending CHECK CONDITION", acsiId);
        dataTrans->setStatus(SCSI_ST_CHECK_CONDITION);
        dataTrans->sendDataAndStatus();
        return;
    }

    isIcd   = ((bufIn[0] & 0x1f) == 0x1f);              // it's an ICD command, if lowest 5 bits are all set in the cmd[0]
    pCmd    = (!isIcd) ? bufIn : (bufIn + 1);           // get the pointer to where the command starts

    justCmd = pCmd[0] & 0x1f;                           // get only command

    tag1    = pCmd[1];                                  // CE tag ('C', 'E') can be found on position 2 and 3
    tag2    = pCmd[2];

    module  = pCmd[3];                                  // get the host module ID

    if(isIcd){
        logHdd(LOG_DEBUG, "handleAcsiCommand: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x isIcd",
                   bufIn[0], bufIn[1], bufIn[2], bufIn[3], bufIn[4], bufIn[5], bufIn[6], bufIn[7], bufIn[8], bufIn[9], bufIn[10], bufIn[11], bufIn[12], bufIn[13]);
    } else {
        logHdd(LOG_DEBUG, "handleAcsiCommand: %02x %02x %02x %02x %02x %02x", bufIn[0], bufIn[1], bufIn[2], bufIn[3], bufIn[4], bufIn[5]);
    }

    // if it's the retry module (highest bit in HOSTMOD is set), let it go before the big switch for modules,
    // because it will change the command and let it possibly run trough the correct module
    if(justCmd == 0 && tag1 == 'C' && tag2 == 'E' && (module & 0x80) != 0) {                         // if it's a RETRY attempt...
        module = module & 0x7f;                 // remove RETRY bit from module

        if(!isIcd) {                            // short command?
            bufIn[3] = bufIn[3] & 0x7f;         // remove RETRY bit from HOSTMOD_ code
        } else {                                // long command?
            bufIn[4] = bufIn[4] & 0x7f;         // remove RETRY bit from HOSTMOD_ code
        }

        bool gotThisCmd = retryMod->gotThisCmd(bufIn, isIcd);       // first check if we got this command buffered, or not

        if(gotThisCmd) {                                    // if got this command buffered
            retryMod->restoreCmdFromCopy(bufIn, isIcd, justCmd, tag1, tag2, module);
            pCmd = (!isIcd) ? bufIn : (bufIn + 1);          // get the pointer to where the command starts

            logHdd(LOG_DEBUG, "handleAcsiCommand -- doing retry for cmd: %02x %02x %02x %02x %02x %02x", bufIn[0], bufIn[1], bufIn[2], bufIn[3], bufIn[4], bufIn[5]);

            int dataDir = retryMod->getDataDirection();
            if(dataDir == DATA_DIRECTION_READ) {            // if it's READ operation, retry using stored data and don't let the right module to handle it
                dataTrans->sendDataAndStatus(true);         // send data and status using data stored in RETRY module
                wasHandled = true;
                return;
            }

            // if it got here, it's a WRITE operation, let the right module to handle it
        } else {                                            // if this command was not buffered, we have received it for the first time and we need to process it like usually
            // as we didn't process it yet, make copy of it
            retryMod->makeCmdCopy(bufIn, isIcd, justCmd, tag1, tag2, module);
        }
    } else {            // if it's not retry module, make copy of everything
        retryMod->makeCmdCopy(bufIn, isIcd, justCmd, tag1, tag2, module);
    }

    // ok, so the ID is right, let's see what we can do
    if(justCmd == 0 && tag1 == 'C' && tag2 == 'E') {    // if the command is 0 (TEST UNIT READY) and there's this CE tag
        logHdd(LOG_DEBUG, "handleAcsiCommand - CE specific command - module: %02x", module);

        switch(module) {

        case HOSTMOD_TRANSLATED_DISK:                   // translated disk command?
            wasHandled = true;
            translated->processCommand(pCmd);
            break;
        }
    }

    if(!wasHandled) {           // if the command was not previously handled, it's probably just some SCSI command
        scsi->processCommand(bufIn);                    // process the command
    }
}

void HddClient::onMacUpdated(void)
{
    StatusReport::storeTosAndMachine(ci->mac, hwConfig.tosVersion, hwConfig.scsiMachine);

    scsi->setMac(ci->mac);
    translated->setMac(ci->mac);
}

void HddClient::handleFwVersion_hans(void)
{
    uint8_t xilinxInfo = chipInterface->getFWversionHdd(ci);
    extractInterfaceInfo(xilinxInfo);

    onMacUpdated();

    // send enabled hdd ids to device
    uint8_t config[2];
    config[0] = CMD_ACSI_CONFIG;
    config[1] = getIdBits();        // get the enabled IDs
    chipInterface->sendHeaderAndDataToChip(ci->fdClient, CMD_ACSI_CONFIG, config, 2);

    //----------------------------------
    // if HW info changed
    if(hwConfig.changed) {
        hwConfig.changed = false;

        scsi->updateTranslatedBootMedia();   // also update CE_DD bootsector with proper SCSI ID

        saveHwConfig();                             // save the new config
    }
}

uint8_t HddClient::getIdBits(void)
{
    // get the bits from struct
    uint8_t enabledIDbits = acsiIdInfo.enabledIDbits;

    if(hwConfig.hddIface != HDD_IF_SCSI) {          // not SCSI? Don't change anything
//        logHdd(LOG_DEBUG, "HddClient::getIdBits() -- we're running on ACSI");
        return enabledIDbits;
    }

    // if we're on SCSI bus, remove ID bits if they are used for SCSI Initiator on that machine (ID 7 on TT, ID 0 on Falcon)
    switch(hwConfig.scsiMachine) {
        case SCSI_MACHINE_TT:                       // TT? remove bit 7
//            logHdd(LOG_DEBUG, "HddClient::getIdBits() -- we're running on TT, will remove ID 7 from enabled ID bits");

            enabledIDbits = enabledIDbits & 0x7F;
            break;

        //------------
        case SCSI_MACHINE_FALCON:                   // Falcon? remove bit 0
//            logHdd(LOG_DEBUG, "HddClient::getIdBits() -- we're running on Falcon, will remove ID 0 from enabled ID bits");

            enabledIDbits = enabledIDbits & 0xFE;
            break;

        //------------
        default:
        case SCSI_MACHINE_UNKNOWN:                  // unknown machine? remove both bits 7 and 0
//            logHdd(LOG_DEBUG, "HddClient::getIdBits() -- we're running on unknown machine, will remove ID 7 and ID 0 from enabled ID bits");

            enabledIDbits = enabledIDbits & 0x7E;
            break;
    }

    return enabledIDbits;
}

void HddClient::saveHwConfig(void)
{

}

void HddClient::extractInterfaceInfo(uint8_t xilinxInfo)
{
    THwConfig hwConfigOld = hwConfig;

    switch(xilinxInfo) {
        // GOOD
        case 0x41:  hwConfig.version        = 4;                        // v.4
                    hwConfig.hddIface       = HDD_IF_ACSI;              // HDD int: ACSI
                    break;

        // GOOD
        case 0x42:  hwConfig.version        = 4;                        // v.4
                    hwConfig.hddIface       = HDD_IF_SCSI;              // HDD int: SCSI
                    break;
    }

    // if the HD IF changed (received the 1st HW info) and we're on SCSI bus, we need to send the new (limited) SCSI IDs to Hans, so he won't answer on Initiator SCSI ID
    if((hwConfigOld.hddIface != hwConfig.hddIface) && hwConfig.hddIface == HDD_IF_SCSI) {
        hwConfig.changed = true;
        logHdd(LOG_DEBUG, "Found out that we're running on SCSI bus - will resend the ID bits configuration to Hans");
    }

    if(memcmp(&hwConfigOld, &hwConfig, sizeof(THwConfig)) != 0) {    // config changed? save it
        hwConfig.changed = true;
    }
}

void HddClient::reloadDrivesRaw(void)
{
    scsi->findAttachedDisks();
}

void HddClient::reloadDrivesTranslated(void)
{
    translated->findAttachedDisks();
}
