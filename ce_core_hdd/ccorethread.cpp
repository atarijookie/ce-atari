// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <net/if.h>

#include "global.h"
#include "debug.h"
#include "ccorethread.h"
#include "translated/translateddisk.h"
#include "native/scsi.h"
#include "native/scsi_defs.h"
#include "update.h"
#include "utils.h"
#include "statusreport.h"

#include "extension/extensionhandler.h"

#define DEV_CHECK_TIME_MS       3000
#define UPDATE_CHECK_TIME       1000
#define INET_IFACE_CHECK_TIME   1000
#define UPDATE_SCRIPTS_TIME     10000

extern THwConfig    hwConfig;
extern TFlags       flags;
extern ChipInterface* chipInterface;

extern DebugVars    dbgVars;

extern SharedObjects shared;

struct TLastFwInfoTime {
    uint32_t hans;
    uint32_t franz;
    uint32_t nextDisplay;

    uint32_t hansResetTime;
    uint32_t franzResetTime;

    int     progress;
};

TLastFwInfoTime lastFwInfoTime;
LoadTracker load;

CCoreThread::CCoreThread()
{
    Update::initialize();

    setEnabledIDbits        = false;

    retryMod = new RetryModule();

    dataTrans = new AcsiDataTrans();
    dataTrans->setCommunicationObject(chipInterface);
    dataTrans->setRetryObject(retryMod);

    sharedObjects_create();

    // update CE_DD bootsector with proper SCSI ID
    shared.scsi->updateTranslatedBootMedia();

    // now register all the objects which use some settings in the proxy
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this, SETTINGSUSER_ACSI);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this, SETTINGSUSER_TRANSLATED);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this, SETTINGSUSER_SCSI_IDS);

    settingsReloadProxy.addSettingsUser((ISettingsUser *) shared.scsi, SETTINGSUSER_ACSI);

    settingsReloadProxy.addSettingsUser((ISettingsUser *) TranslatedDisk::getInstance(), SETTINGSUSER_TRANSLATED);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) TranslatedDisk::getInstance(), SETTINGSUSER_SHARED);

    misc.setDataTrans(dataTrans);

    extensionHandler = new ExtensionHandler();
    extensionHandler->setAcsiDataTrans(dataTrans);
}

CCoreThread::~CCoreThread()
{
    delete dataTrans;
    delete retryMod;
    delete extensionHandler;

    sharedObjects_destroy();
}

void CCoreThread::sharedObjects_create(void)
{
    shared.scsi = new Scsi();
    shared.scsi->setAcsiDataTrans(dataTrans);

    TranslatedDisk * translated = TranslatedDisk::createInstance(dataTrans);
    translated->setSettingsReloadProxy(&settingsReloadProxy);
}

void CCoreThread::sharedObjects_destroy(void)
{
    delete shared.scsi;
    shared.scsi = NULL;

    TranslatedDisk::deleteInstance();
}

void CCoreThread::run(void)
{
    // inBuff might contain whole written floppy sector + header size
    uint8_t inBuff[INBUF_SIZE], outBuf[INBUF_SIZE];

    memset(outBuf, 0, INBUF_SIZE);
    memset(inBuff, 0, INBUF_SIZE);

    loadSettings();

    //------------------------------

    lastFwInfoTime.nextDisplay = Utils::getEndTime(1000);
    lastFwInfoTime.hansResetTime = Utils::getCurrentMs();
    lastFwInfoTime.franzResetTime = Utils::getCurrentMs();

    bool needsAction;

    load.clear();                               // clear load counter

    // The loop count and no-sleep-loops-count will allow us to avoid handling other stuff and sleeping
    // when there was a command from HDD or FDD, which we want to handle without further delays.
    #define NO_SLEEP_LOOPS_AFTER_COMMAND    10000
    int loopCount = 0;

    while(sigintReceived == 0) {
        bool gotAcsiCommand = false;

        load.busy.markStart();                  // mark the start of the busy part of the code

        needsAction = chipInterface->actionNeeded(inBuff);

        if(needsAction) {                       // hard drive needs action?
            gotAcsiCommand = handleHdd(inBuff);
        }

        load.busy.markEnd();                    // mark the end of the busy part of the code

        // If HDD or FDD command was received, w're restarting the loop count to zero.
        if(gotAcsiCommand) {
            loopCount = 0;
        }

        // If we're in this period after last command, just increase the loop count and don't sleep.
        // We're doing this to avoid running the other stuff and sleeping when we need low latency to respond to sequence of commands.
        if(loopCount < NO_SLEEP_LOOPS_AFTER_COMMAND) {
            loopCount++;
        } else {        // There was no command in the last few loops, so we can now hande other stuff and sleep a little.
            handleOtherStuff();         // handle the other stuff, which doesn't need to be called when we're transferring data
            Utils::sleepMs(1);          // wait 1 ms...
        }
    }
}

void CCoreThread::handleOtherStuff(void)
{
    static bool initialized = false;

    if(!initialized) {          // timers and flags not initialized yet?
        initialized = true;
    }

    uint32_t now = Utils::getCurrentMs();

    if(now >= lastFwInfoTime.nextDisplay) {
        lastFwInfoTime.nextDisplay  = Utils::getEndTime(1000);
        displayStatusToConsole(now);
    }
}

bool CCoreThread::handleHdd(uint8_t* inBuff)
{
    bool isAcsiCommand = false;
    uint32_t now = Utils::getCurrentMs();

    switch(inBuff[3]) {
        case ATN_FW_VERSION:
            statuses.hans.aliveTime = now;
            statuses.hans.aliveSign = ALIVE_FWINFO;

            lastFwInfoTime.hans = now;
            handleFwVersion_hans();
            break;

        case ATN_ACSI_COMMAND:
            isAcsiCommand = true;

            dbgVars.isInHandleAcsiCommand = 1;

            statuses.hdd.aliveTime  = now;
            statuses.hdd.aliveSign  = ALIVE_RW;

            statuses.hans.aliveTime = now;
            statuses.hans.aliveSign = ALIVE_CMD;

            handleAcsiCommand(inBuff + 8);

            dbgVars.isInHandleAcsiCommand = 0;
        break;

    default:
        Debug::out(LOG_ERROR, "CCoreThread received weird ATN code %02x waitForAtn()", inBuff[3]);
        break;
    }

    return isAcsiCommand;
}

void CCoreThread::displayStatusToConsole(uint32_t now)
{
    char progChars[4] = {'|', '/', '-', '\\'};

    //-------------
    // calculate load, show message if needed
    load.calculate();                                           // calculate load

    if(load.suspicious) {                                       // load is suspiciously high?
        Debug::out(LOG_DEBUG, ">>> Suspicious core cycle load -- load: %3d %%", load.loadPercents);
        printf(">>> Suspicious core cycle load -- load: %3d %%\n", load.loadPercents);

        lastFwInfoTime.hansResetTime    = now;
        lastFwInfoTime.franzResetTime   = now;
        }
    //-------------

    float hansTime  = ((float)(now - lastFwInfoTime.hans))  / 1000.0f;
    float franzTime = ((float)(now - lastFwInfoTime.franz)) / 1000.0f;

    hansTime  = (hansTime  < 15.0f) ? hansTime  : 15.0f;
    franzTime = (franzTime < 15.0f) ? franzTime : 15.0f;

    printf("\033[2K  [ %c ]  CE HDD core is running\033[A\n", progChars[lastFwInfoTime.progress]);
    lastFwInfoTime.progress = (lastFwInfoTime.progress + 1) % 4;

    load.clear();                       // clear load counter
}

void CCoreThread::handleAcsiCommand(uint8_t *bufIn)
{
    Debug::out(LOG_DEBUG, "\n");

    dbgVars.prevAcsiCmdTime = dbgVars.thisAcsiCmdTime;
    dbgVars.thisAcsiCmdTime = Utils::getCurrentMs();

    uint8_t justCmd, tag1, tag2, module;
    uint8_t *pCmd;
    uint8_t isIcd = false;
    uint8_t wasHandled = false;

    uint8_t acsiId = bufIn[0] >> 5;                            // get just ACSI ID
    if(acsiIdInfo.acsiIDdevType[acsiId] == DEVTYPE_OFF) {    // if this ACSI ID is off, reply with error and quit
        Debug::cmdStart(bufIn, "DEV OFF");

        Debug::out(LOG_WARNING, "CCoreThread::handleAcsiCommand - acsiId %d is OFF, sending CHECK CONDITION", acsiId);
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
        Debug::out(LOG_DEBUG, "handleAcsiCommand: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x isIcd",
                   bufIn[0], bufIn[1], bufIn[2], bufIn[3], bufIn[4], bufIn[5], bufIn[6], bufIn[7], bufIn[8], bufIn[9], bufIn[10], bufIn[11], bufIn[12], bufIn[13]);
    } else {
        Debug::out(LOG_DEBUG, "handleAcsiCommand: %02x %02x %02x %02x %02x %02x", bufIn[0], bufIn[1], bufIn[2], bufIn[3], bufIn[4], bufIn[5]);
    }

    // if it's the retry module (highest bit in HOSTMOD is set), let it go before the big switch for modules,
    // because it will change the command and let it possibly run trough the correct module
    if(justCmd == 0 && tag1 == 'C' && tag2 == 'E' && (module & 0x80) != 0) {                         // if it's a RETRY attempt...
        Debug::cmdStart(bufIn, "CE_RETRY");

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

            Debug::out(LOG_DEBUG, "handleAcsiCommand -- doing retry for cmd: %02x %02x %02x %02x %02x %02x", bufIn[0], bufIn[1], bufIn[2], bufIn[3], bufIn[4], bufIn[5]);

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
        Debug::out(LOG_DEBUG, "handleAcsiCommand - CE specific command - module: %02x", module);

        switch(module) {

        case HOSTMOD_TRANSLATED_DISK:                   // translated disk command?
            Debug::cmdStart(bufIn, "CE_TRANS");
            wasHandled = true;

            {
                TranslatedDisk * translated = TranslatedDisk::getInstance();
                if(translated) {
                    translated->mutexLock();
                    translated->processCommand(pCmd);
                    translated->mutexUnlock();
                }
            }
            break;

        case HOSTMOD_MISC:
            Debug::cmdStart(bufIn, "CE_MISC");
            wasHandled = true;
            misc.processCommand(pCmd);
            break;
        }
    }

    // if this command belongs to extensions, let it to ExtensionHandler
    if(extensionHandler->isExtensionCall(justCmd)) {
        Debug::cmdStart(bufIn, "CE_EXT");
        extensionHandler->processCommand(pCmd);
        wasHandled = true;
    }

    if(!wasHandled) {           // if the command was not previously handled, it's probably just some SCSI command
        Debug::cmdStart(bufIn, "RAW");
        pthread_mutex_lock(&shared.mtxHdd);
        shared.scsi->processCommand(bufIn);                    // process the command
        pthread_mutex_unlock(&shared.mtxHdd);
    }
}

void CCoreThread::reloadSettings(int type)
{
    // if just SCSI IDs changed (
    if(type == SETTINGSUSER_SCSI_IDS) {
        Debug::out(LOG_DEBUG, "CCoreThread::reloadSettings() - received SETTINGSUSER_SCSI_IDS, will resend enabled SCSI IDs");

        setEnabledIDbits = true;
        return;
    }

    // then load the new settings
    loadSettings();
}

void CCoreThread::fillDisplayLines(void)
{
    char tmpLine1[256], tmpLine2[256], tmp[32];

    #define MAX_DEV_TYPES       4
    const char *devTypeString[MAX_DEV_TYPES] = {"OFF", "SD", "RAW", "CE"};

    if(hwConfig.hddIface == HDD_IF_ACSI) {  // is ACSI?
        strcpy(tmpLine1, "ACSI: ");
    } else {                                // is SCSI?
        strcpy(tmpLine1, "SCSI: ");
    }

    strcpy(tmpLine2, tmpLine1);

    int i;
    for(i=0; i<8; i++) {
        // first HDD line: just enabled IDs
        strcpy(tmp, "x ");
        tmp[0] = (acsiIdInfo.enabledIDbits & (1 << i)) ? '0' + i : '-';    // show ID if enabled, dash if not enabled
        strcat(tmpLine1, tmp);     // add to 1st line

        // second HDD line: IDs vs. device types
        int devType = acsiIdInfo.acsiIDdevType[i];
        if(devType != DEVTYPE_OFF && devType > 0 && devType < MAX_DEV_TYPES) {
            const char* devTypeStr = devTypeString[devType];    // convert type int into string
            sprintf(tmp, "%d:%s ", i, devTypeStr);
            strcat(tmpLine2, tmp);
        }
    }

    // now set the constructed strings
    // TODO: store display data elsewhere
    // display_setLine(DISP_LINE_HDD_IDS, tmpLine1);
    // display_setLine(DISP_LINE_HDD_TYPES, tmpLine2);
}

void CCoreThread::loadSettings(void)
{
    Debug::out(LOG_DEBUG, "CCoreThread::loadSettings");

    Settings s;
    s.loadAcsiIDs(&acsiIdInfo);

    fillDisplayLines();     // fill lines for front display

    setEnabledIDbits = true;
}

void CCoreThread::handleFwVersion_hans(void)
{
    uint8_t fwVer[16];
    memset(fwVer, 0, 16);

    uint8_t enabledIDbits = getIdBits();     // get the enabled IDs

    chipInterface->setHDDconfig(enabledIDbits);
    chipInterface->getFWversion(fwVer);

    //----------------------------------
    // if HW info changed
    if(hwConfig.changed) {
        hwConfig.changed = false;

        setEnabledIDbits = true;                    // resend config 

        pthread_mutex_lock(&shared.mtxHdd);
        shared.scsi->updateTranslatedBootMedia();   // also update CE_DD bootsector with proper SCSI ID
        pthread_mutex_unlock(&shared.mtxHdd);

        fillDisplayLines();                         // fill lines for front display
        saveHwConfig();                             // save the new config
    }

    Debug::out(LOG_DEBUG, "FW: %d-%02d-%02d", Update::versions.hans.getYear(), Update::versions.hans.getMonth(), Update::versions.hans.getDay());
}

uint8_t CCoreThread::getIdBits(void)
{
    // get the bits from struct
    uint8_t enabledIDbits = acsiIdInfo.enabledIDbits;

    if(hwConfig.hddIface != HDD_IF_SCSI) {          // not SCSI? Don't change anything
//        Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on ACSI");
        return enabledIDbits;
    }

    // if we're on SCSI bus, remove ID bits if they are used for SCSI Initiator on that machine (ID 7 on TT, ID 0 on Falcon)
    switch(hwConfig.scsiMachine) {
        case SCSI_MACHINE_TT:                       // TT? remove bit 7
//            Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on TT, will remove ID 7 from enabled ID bits");

            enabledIDbits = enabledIDbits & 0x7F;
            break;

        //------------
        case SCSI_MACHINE_FALCON:                   // Falcon? remove bit 0
//            Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on Falcon, will remove ID 0 from enabled ID bits");

            enabledIDbits = enabledIDbits & 0xFE;
            break;

        //------------
        default:
        case SCSI_MACHINE_UNKNOWN:                  // unknown machine? remove both bits 7 and 0
//            Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on unknown machine, will remove ID 7 and ID 0 from enabled ID bits");

            enabledIDbits = enabledIDbits & 0x7E;
            break;
    }

    return enabledIDbits;
}

void CCoreThread::saveHwConfig(void)
{

}
