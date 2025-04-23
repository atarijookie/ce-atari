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

    // give floppy setup everything it needs
    // TODO: rework
    // floppySetup.setAcsiDataTrans(dataTrans);
    // floppySetup.setSettingsReloadProxy(&settingsReloadProxy);

    // the floppy image silo might change settings (when images are changes), add settings reload proxy
    // shared.imageSilo->setSettingsReloadProxy(&settingsReloadProxy);
    // settingsReloadProxy.reloadSettings(SETTINGSUSER_FLOPPYIMGS);            // mark that floppy settings changed (when imageSilo loaded the settings)

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

    // if(events.insertSpecialFloppyImageId != 0) {            // very stupid way of letting web IF to insert special image
    //     insertSpecialFloppyImage(events.insertSpecialFloppyImageId);
    //     events.insertSpecialFloppyImageId = 0;
    // }

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
        case HOSTMOD_CONFIG:                            // config console command?
            Debug::cmdStart(bufIn, "CE_CONF");
            wasHandled = true;
            // NOP: no longer supported
            break;

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

        case HOSTMOD_FDD_SETUP:                         // floppy setup command?
            // TODO: rework
            Debug::cmdStart(bufIn, "CE_FDD");
            wasHandled = true;
            // pthread_mutex_lock(&shared.mtxImages);      // lock floppy images shared objects -- now used by floppy setup object
            // floppySetup.processCommand(pCmd);
            // pthread_mutex_unlock(&shared.mtxImages);    // unlock floppy images shared objects
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

    if(type == SETTINGSUSER_TRANSLATED) {
        Settings s;
        bool newMountRawNotTrans = s.getBool("MOUNT_RAW_NOT_TRANS", 0);

        if(shared.mountRawNotTrans != newMountRawNotTrans) {       // mount strategy changed?
            shared.mountRawNotTrans = newMountRawNotTrans;

            Debug::out(LOG_DEBUG, "CCoreThread::reloadSettings -- USB media mount strategy changed, remounting");
        }

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

    shared.mountRawNotTrans = s.getBool("MOUNT_RAW_NOT_TRANS", 0);

    setEnabledIDbits = true;
}

void CCoreThread::handleFwVersion_hans(void)
{
    uint8_t fwVer[16];
    memset(fwVer, 0, 16);

    uint8_t enabledIDbits, sdCardAcsiId;
    getIdBits(enabledIDbits, sdCardAcsiId);     // get the enabled IDs

    chipInterface->setHDDconfig(enabledIDbits, sdCardAcsiId, 0, false, false);
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

    //----------------------------------
    // do the following only for chip interface v1 v2
    int currentLed = fwVer[4];

    char recoveryLevel = fwVer[9];
    if(recoveryLevel != 0) {                                                        // if the recovery level is not empty
        if(recoveryLevel == 'R' || recoveryLevel == 'S' || recoveryLevel == 'T') {  // and it's a valid recovery level
            handleRecoveryCommands(recoveryLevel - 'Q');                            // handle recovery action
        }
    }

    Debug::out(LOG_DEBUG, "FW: Hans,  %d-%02d-%02d, LED is: %d", Update::versions.hans.getYear(), Update::versions.hans.getMonth(), Update::versions.hans.getDay(), currentLed);
}

void CCoreThread::getIdBits(uint8_t &enabledIDbits, uint8_t &sdCardAcsiId)
{
    // get the bits from struct
    enabledIDbits  = acsiIdInfo.enabledIDbits;
    sdCardAcsiId   = acsiIdInfo.sdCardAcsiId;

    if(hwConfig.hddIface != HDD_IF_SCSI) {          // not SCSI? Don't change anything
//        Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on ACSI");
        return;
    }

    // if we're on SCSI bus, remove ID bits if they are used for SCSI Initiator on that machine (ID 7 on TT, ID 0 on Falcon)
    switch(hwConfig.scsiMachine) {
        case SCSI_MACHINE_TT:                       // TT? remove bit 7
//            Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on TT, will remove ID 7 from enabled ID bits");

            enabledIDbits = enabledIDbits & 0x7F;
            if(sdCardAcsiId == 7) {
                sdCardAcsiId = 0xff;
            }
            break;

        //------------
        case SCSI_MACHINE_FALCON:                   // Falcon? remove bit 0
//            Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on Falcon, will remove ID 0 from enabled ID bits");

            enabledIDbits = enabledIDbits & 0xFE;
            if(sdCardAcsiId == 0) {
                sdCardAcsiId = 0xff;
            }
            break;

        //------------
        default:
        case SCSI_MACHINE_UNKNOWN:                  // unknown machine? remove both bits 7 and 0
//            Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on unknown machine, will remove ID 7 and ID 0 from enabled ID bits");

            enabledIDbits = enabledIDbits & 0x7E;
            if(sdCardAcsiId == 0 || sdCardAcsiId == 7) {
                sdCardAcsiId = 0xff;
            }
            break;
    }
}

void CCoreThread::saveHwConfig(void)
{
    Settings s;

    int ver, hddIf, scsiMch;

    // get current values for these configs
    ver     = s.getInt("HW_VERSION",       1);
    hddIf   = s.getInt("HW_HDD_IFACE",     HDD_IF_ACSI);
    scsiMch = s.getInt("HW_SCSI_MACHINE",  SCSI_MACHINE_UNKNOWN);

    // store value only if it has changed
    if(ver != hwConfig.version) {
        s.setInt("HW_VERSION", ver);
    }

    if(hddIf != hwConfig.hddIface) {
        s.setInt("HW_HDD_IFACE", hddIf);
    }

    if(scsiMch != hwConfig.scsiMachine) {
        s.setInt("HW_SCSI_MACHINE", scsiMch);
    }
}

void CCoreThread::showHwVersion(void)
{
    char tmp[256];

    Debug::out(LOG_INFO, "Reporting this as HW INFO...");   // show in log file

    // HW version is 1 | 2 | 3, and in other cases defaults to 1
    int hwVer = 1;

    if(hwConfig.version >= 1 && hwConfig.version <=3) {     // if HW version is within valid values, use it
        hwVer = hwConfig.version;
    }
    
    sprintf(tmp, "HW_VER: %d", hwVer);
    printf("\n%s\n", tmp);                  // show on stdout
    Debug::out(LOG_INFO, "   %s", tmp);    // show in log file

    // HDD interface is either SCSI, or defaults to ACSI
    sprintf(tmp, "HDD_IF: %s", (hwConfig.hddIface == HDD_IF_SCSI) ? "SCSI" : "ACSI");
    printf("\n%s\n", tmp);                  // show on stdout
    Debug::out(LOG_INFO, "   %s", tmp);    // show in log file

    sprintf(tmp, "HWFWMM: %s", hwConfig.fwMismatch ? "MISMATCH" : "OK");
    printf("\n%s\n", tmp);                  // show on stdout
    Debug::out(LOG_INFO, "   %s", tmp);    // show in log file
}

void CCoreThread::handleRecoveryCommands(int recoveryLevel)
{
    Debug::out(LOG_DEBUG, "CCoreThread::handleRecoveryCommands() -- recoveryLevel is %d", recoveryLevel);

    switch(recoveryLevel) {
        case 1: // just insert config floppy image into slot 1
                // insertSpecialFloppyImage(SPECIAL_FDD_IMAGE_CE_CONF);
                break;

        //----------------------------------------------------
        case 2: // delete settings, set network to DHCP
                Debug::out(LOG_INFO, ">>> CCoreThread::handleRecoveryCommands -- LEVEL 2 - removing settings, restarting whole linux <<<\n");

                deleteSetting();         // delete all settings, set network to DHCP

                Debug::out(LOG_INFO, ">>> Terminating app and will reboot device, because app settings and network settings changed <<<\n");

                system("reboot");                           // reboot device
                sigintReceived = 1;                         // turn off app (probably not needed)
                break;

        //----------------------------------------------------
        case 3: // like 2, but also flash first firmware
                Debug::out(LOG_INFO, ">>> CCoreThread::handleRecoveryCommands -- LEVEL 3 - removing settings, flashing first FW <<<\n");

                deleteSetting();         // delete all settings, set network to DHCP

                Debug::out(LOG_INFO, ">>> Terminating app, because will do flashFirstFw as a part of handleRecoveryCommands() ! <<<\n");
                sigintReceived = 1;                         // turn off app
                break;
    }
}

void CCoreThread::deleteSetting(void)
{
    // delete settings
    system("rm -f /ce/settings/*");

    // sync to write stuff to card
    system("sync");
}
