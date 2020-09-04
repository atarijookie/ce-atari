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
#include "native/scsi.h"
#include "native/scsi_defs.h"
#include "mounter.h"
#include "downloader.h"
#include "update.h"
#include "config/netsettings.h"
#include "ce_conf_on_rpi.h"
#include "statusreport.h"
#include "display/displaythread.h"

#include "service/configservice.h"
#include "service/floppyservice.h"
#include "service/screencastservice.h"

#include "floppy/imagelist.h"
#include "floppy/imagesilo.h"
#include "floppy/imagestorage.h"
#include "floppy/floppyencoder.h"

#include "mediastreaming/mediastreaming.h"

#include "periodicthread.h"

#if defined(ONPC_HIGHLEVEL)
    #include "socks.h"
#endif

#define DEV_CHECK_TIME_MS       3000
#define UPDATE_CHECK_TIME       1000
#define INET_IFACE_CHECK_TIME   1000
#define UPDATE_SCRIPTS_TIME     10000

extern THwConfig    hwConfig;
extern TFlags       flags;
extern ChipInterface* chipInterface;

extern DebugVars    dbgVars;

extern SharedObjects shared;

CCoreThread::CCoreThread(ConfigService* configService, FloppyService *floppyService, ScreencastService* screencastService)
{
    NetworkSettings ns;
    ns.load();
    ns.updateResolvConf(false);     // update resolv.conf

    if(!ns.wlan0.isEnabled) {       // if wlan0 not enabled, send one wlan0 restart, which might bring the wlan0 down
        TMounterRequest tmr;
        tmr.action = MOUNTER_ACTION_RESTARTNETWORK_WLAN0;
        Mounter::add(tmr);
    }

    Update::initialize();

    setEnabledIDbits        = false;
    setEnabledFloppyImgs    = false;
    setNewFloppyImageLed    = false;
    setFloppyConfig         = false;
    setDiskChanged          = false;
    diskChanged             = false;

    lastFloppyImageLed = -1;
    newFloppyImageLedAfterEncode = -2;

    retryMod = new RetryModule();

    dataTrans = new AcsiDataTrans();
    dataTrans->setCommunicationObject(chipInterface);
    dataTrans->setRetryObject(retryMod);

    sharedObjects_create(configService, floppyService, screencastService);

    // now register all the objects which use some settings in the proxy
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_ACSI);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_FLOPPYIMGS);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_FLOPPYCONF);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_FLOPPY_SLOT);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_TRANSLATED);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_SCSI_IDS);

    settingsReloadProxy.addSettingsUser((ISettingsUser *) shared.scsi,          SETTINGSUSER_ACSI);

    settingsReloadProxy.addSettingsUser((ISettingsUser *) TranslatedDisk::getInstance(), SETTINGSUSER_TRANSLATED);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) TranslatedDisk::getInstance(), SETTINGSUSER_SHARED);

    // give floppy setup everything it needs
    floppySetup.setAcsiDataTrans(dataTrans);
    floppySetup.setSettingsReloadProxy(&settingsReloadProxy);

    // the floppy image silo might change settings (when images are changes), add settings reload proxy
    shared.imageSilo->setSettingsReloadProxy(&settingsReloadProxy);
    settingsReloadProxy.reloadSettings(SETTINGSUSER_FLOPPYIMGS);            // mark that floppy settings changed (when imageSilo loaded the settings)

    //Floppy Service needs access to floppysilo and this thread
    if(floppyService) {
        floppyService->setImageSilo(shared.imageSilo);
        floppyService->setCoreThread(this);
    }

    // set up network adapter stuff
    netAdapter.setAcsiDataTrans(dataTrans);

    // set up mediastreaming service
}

CCoreThread::~CCoreThread()
{
    delete dataTrans;
    delete retryMod;

    MediaStreaming::deleteInstance();
    sharedObjects_destroy();
}

void CCoreThread::sharedObjects_create(ConfigService* configService, FloppyService *floppyService, ScreencastService* screencastService)
{
    shared.devFinder_detachAndLook = false;
    shared.devFinder_look = false;

    shared.scsi = new Scsi();
    shared.scsi->setAcsiDataTrans(dataTrans);

    TranslatedDisk * translated = TranslatedDisk::createInstance(dataTrans, configService, screencastService);
    translated->setSettingsReloadProxy(&settingsReloadProxy);

    shared.imageList = new ImageList();
    shared.imageSilo = new ImageSilo();
    shared.imageStorage = new ImageStorage();

    //-----------
    // create config stream for ACSI interface
    shared.configStream.acsi = new ConfigStream(CONFIGSTREAM_ON_ATARI);
    shared.configStream.acsi->setAcsiDataTrans(dataTrans);
    shared.configStream.acsi->setSettingsReloadProxy(&settingsReloadProxy);

    // create config stream for web interface
    shared.configStream.dataTransWeb    = new AcsiDataTrans();
    shared.configStream.web             = new ConfigStream(CONFIGSTREAM_THROUGH_WEB);
    shared.configStream.web->setAcsiDataTrans(shared.configStream.dataTransWeb);
    shared.configStream.web->setSettingsReloadProxy(&settingsReloadProxy);

    // create config stream for linux terminal
    shared.configStream.dataTransTerm   = new AcsiDataTrans();
    shared.configStream.term            = new ConfigStream(CONFIGSTREAM_IN_LINUX_CONSOLE);
    shared.configStream.term->setAcsiDataTrans(shared.configStream.dataTransTerm);
    shared.configStream.term->setSettingsReloadProxy(&settingsReloadProxy);
}

void CCoreThread::sharedObjects_destroy(void)
{
    delete shared.scsi;
    shared.scsi = NULL;

    TranslatedDisk::deleteInstance();

    delete shared.imageList;
    shared.imageList = NULL;

    delete shared.imageSilo;
    shared.imageSilo = NULL;

    delete shared.imageStorage;
    shared.imageStorage = NULL;

    delete shared.configStream.acsi;
    shared.configStream.acsi = NULL;

    delete shared.configStream.web;
    shared.configStream.web = NULL;

    delete shared.configStream.dataTransWeb;
    shared.configStream.dataTransWeb = NULL;

    delete shared.configStream.term;
    shared.configStream.term = NULL;

    delete shared.configStream.dataTransTerm;
    shared.configStream.dataTransTerm = NULL;
}

#define INBUF_SIZE  (WRITTENMFMSECTOR_SIZE + 8)

void CCoreThread::run(void)
{
    // inBuff might contain whole written floppy sector + header size
    BYTE inBuff[INBUF_SIZE], outBuf[INBUF_SIZE];

    memset(outBuf, 0, INBUF_SIZE);
    memset(inBuff, 0, INBUF_SIZE);

    loadSettings();

    //------------------------------
    // stuff related to checking of Franz and Hans being alive and then possibly flashing them
    bool    shouldCheckHansFranzAlive   = true;                         // when true and the 15 second timeout since start passed, check for Hans and Franz being alive
    DWORD   hansFranzAliveCheckTime     = Utils::getEndTime(15000);     // get the time when we should check if Hans and Franz are alive

    flags.gotHansFwVersion  = false;
    flags.gotFranzFwVersion = false;

    if(flags.noFranz) {                                                     // if running without Franz, pretend we got his FW version
        flags.gotFranzFwVersion = true;
    }

    if(flags.noReset) {                                                     // if we're debugging Hans or Franz (noReset is set to true), don't do this alive check
        shouldCheckHansFranzAlive = false;
    } else {                                                            // if we should reset Hans and Franz on start, do it (and we're probably not debugging Hans or Franz)
        chipInterface->resetHDDandFDD();
    }

#ifdef ONPC_NOTHING
    shouldCheckHansFranzAlive   = false;
    flags.noReset               = true;
#endif

#if defined(ONPC_HIGHLEVEL)
    shouldCheckHansFranzAlive = false;                                  // when running ONPC with HIGHLEVEL of emulation, don't check this

    serverSocket_setParams(1111);
#endif

    Debug::out(LOG_DEBUG, "Will check for Hans and Franz alive: %s", (shouldCheckHansFranzAlive ? "yes" : "no") );
    //------------------------------

    DWORD getHwInfoTimeout      = Utils::getEndTime(3000);                  // create a time when we already should have info about HW, and if we don't have that by that time, then fail

    struct {
        DWORD hans;
        DWORD franz;
        DWORD nextDisplay;

        DWORD hansResetTime;
        DWORD franzResetTime;

        int     progress;
    } lastFwInfoTime;
    char progChars[4] = {'|', '/', '-', '\\'};

    lastFwInfoTime.hans         = 0;
    lastFwInfoTime.franz        = 0;
    lastFwInfoTime.nextDisplay  = Utils::getEndTime(1000);
    lastFwInfoTime.progress     = 0;

    lastFwInfoTime.hansResetTime    = Utils::getCurrentMs();
    lastFwInfoTime.franzResetTime   = Utils::getCurrentMs();

#if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)
    bool needsAction;
    bool hardNotFloppy;
#endif

    DWORD nextFloppyEncodingCheck   = Utils::getEndTime(1000);
    //bool prevFloppyEncodingRunning  = false;

    LoadTracker load;

    while(sigintReceived == 0) {
        bool gotAtn = false;                                            // no ATN received yet?

        // if should just get the HW version and HDD interface, but timeout passed, quit
        if(flags.getHwInfo && Utils::getCurrentMs() >= getHwInfoTimeout) {
            showHwVersion();                                            // show the default HW version
            sigintReceived = 1;                                         // quit
        }

        DWORD now = Utils::getCurrentMs();
        if(now >= lastFwInfoTime.nextDisplay) {
            lastFwInfoTime.nextDisplay  = Utils::getEndTime(1000);

            //-------------
            // calculate load, show message if needed
            load.calculate();                                           // calculate load

            if(load.suspicious) {                                       // load is suspiciously high?
                Debug::out(LOG_DEBUG, ">>> Suspicious core cycle load -- cycle time: %4d ms, load: %3d %%", load.cycle.total, load.loadPercents);
                printf(">>> Suspicious core cycle load -- cycle time: %4d ms, load: %3d %%\n", load.cycle.total, load.loadPercents);

                lastFwInfoTime.hansResetTime    = now;
                lastFwInfoTime.franzResetTime   = now;
                }
            //-------------

            float hansTime  = ((float)(now - lastFwInfoTime.hans))  / 1000.0f;
            float franzTime = ((float)(now - lastFwInfoTime.franz)) / 1000.0f;

            hansTime    = (hansTime  < 15.0f) ? hansTime  : 15.0f;
            franzTime   = (franzTime < 15.0f) ? franzTime : 15.0f;

            bool hansAlive  = (hansTime < 3.0f);
            bool franzAlive = (franzTime < 3.0f);

            int chipIfType = chipInterface->chipInterfaceType();
            if(chipIfType == CHIP_IF_V1_V2) {       // v1 v2 - with Hans and Franz
                printf("\033[2K  [ %c ]  Hans: %s, Franz: %s\033[A\n", progChars[lastFwInfoTime.progress], hansAlive ? "LIVE" : "DEAD", franzAlive ? "LIVE" : "DEAD");
            } else if(chipIfType == CHIP_IF_V3) {   // v3 - with FPGA
                printf("\033[2K  [ %c ]  FPGA: %s\033[A\n", progChars[lastFwInfoTime.progress], hansAlive ? "LIVE" : "DEAD");
            }

            lastFwInfoTime.progress = (lastFwInfoTime.progress + 1) % 4;

            if(!hansAlive && !flags.noReset && (now - lastFwInfoTime.hansResetTime) >= 3000) {
                printf("\033[2KHans not alive, resetting Hans.\n");
                Debug::out(LOG_INFO, "Hans not alive, resetting Hans.");
                lastFwInfoTime.hansResetTime = now;
                chipInterface->resetHDD();
            }

            if(!franzAlive && !flags.noReset && (now - lastFwInfoTime.franzResetTime) >= 3000) {
                printf("\033[2KFranz not alive, resetting Franz.\n");
                Debug::out(LOG_INFO, "Franz not alive, resetting Franz.");
                lastFwInfoTime.franzResetTime = now;
                chipInterface->resetFDD();
            }

            load.clear();                       // clear load counter
        }

        // should we check if Hans and Franz are alive?
        if(shouldCheckHansFranzAlive) {
            if(Utils::getCurrentMs() >= hansFranzAliveCheckTime) {          // did enough time pass since the Hans and Franz reset?
                if(!flags.gotHansFwVersion || !flags.gotFranzFwVersion) {   // if don't have version from Hans or Franz, then they're not alive
                    // Removed flashing first FW when the chips don't reply -- something this detection goes bad,
                    // and this resulted in writing FW to chips even if it was not needed. Now this possible action will be left for
                    // user manual launch (to avoid automatic writing FW over and over again if it won't help).

                    Debug::out(LOG_INFO, "No answer from Hans or Franz, will quit app, hopefully app restart will solve this.");
                    Debug::out(LOG_INFO, "If not, and this will happen in a loop, consider writing chips firmware again.");
                    sigintReceived = 1;
                } else {
                    Debug::out(LOG_DEBUG, "Got answers from both Hans and Franz :)");
                }

                shouldCheckHansFranzAlive = false;                      // don't check this again
            }
        }

        if(now >= nextFloppyEncodingCheck) {
            nextFloppyEncodingCheck = Utils::getEndTime(1000);

            //if(prevFloppyEncodingRunning && !ImageSilo::getFloppyEncodingRunning()) {   // if floppy encoding was running, but not it's not running
                if(newFloppyImageLedAfterEncode != -2) {                                // if we should set the new newFloppyImageLed after encoding is done
                    setEnabledFloppyImgs    = true;
                    setNewFloppyImageLed    = true;
                    newFloppyImageLed       = newFloppyImageLedAfterEncode;

                    newFloppyImageLedAfterEncode = -2;
                }
            //}
            //prevFloppyEncodingRunning = ImageSilo::getFloppyEncodingRunning();
        }

        load.busy.markStart();                          // mark the start of the busy part of the code

#if !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)
        needsAction = chipInterface->actionNeeded(hardNotFloppy, inBuff);

        if(needsAction && hardNotFloppy) {    // hard drive needs action?
            gotAtn = true;  // we've some ATN

            switch(inBuff[3]) {
                case ATN_FW_VERSION:
                    statuses.hans.aliveTime = now;
                    statuses.hans.aliveSign = ALIVE_FWINFO;

                    lastFwInfoTime.hans = Utils::getCurrentMs();
                    handleFwVersion_hans();
                    break;

                case ATN_ACSI_COMMAND:
                    dbgVars.isInHandleAcsiCommand = 1;

                    statuses.hdd.aliveTime  = now;
                    statuses.hdd.aliveSign  = ALIVE_RW;

                    statuses.hans.aliveTime = now;
                    statuses.hans.aliveSign = ALIVE_CMD;

                    handleAcsiCommand(inBuff + 8);

                    dbgVars.isInHandleAcsiCommand = 0;
                break;

            default:
                Debug::out(LOG_ERROR, "CCoreThread received weird ATN code %02x waitForATN()", inBuff[3]);
                break;
            }
        }
#endif

        if(events.insertSpecialFloppyImageId != 0) {                       // very stupid way of letting web IF to insert special image
            insertSpecialFloppyImage(events.insertSpecialFloppyImageId);
            events.insertSpecialFloppyImageId = 0;
        }

#if defined(ONPC_HIGHLEVEL)
        bool res = gotCmd();

        if(res) {
            handleAcsiCommand(inBuf);
        }
#endif

#if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL) && !defined(ONPC_NOTHING)
        // check for any ATN code waiting from Franz
        if(flags.noFranz) {                         // if running without Franz, don't communicate
            needsAction = false;
        }

        if(needsAction && !hardNotFloppy) {         // floppy drive needs action?
            gotAtn = true;                          // we've some ATN

            switch(inBuff[3]) {
            case ATN_FW_VERSION:                    // device has sent FW version
                statuses.franz.aliveTime = now;
                statuses.franz.aliveSign = ALIVE_FWINFO;

                lastFwInfoTime.franz = Utils::getCurrentMs();
                handleFwVersion_franz();
                break;

            case ATN_SECTOR_WRITTEN:                // device has sent written sector data
                statuses.fdd.aliveTime   = now;
                statuses.fdd.aliveSign   = ALIVE_WRITE;

                statuses.franz.aliveTime = now;
                statuses.franz.aliveSign = ALIVE_WRITE;

                handleSectorWritten();
                break;

            case ATN_SEND_TRACK:                    // device requests data of a whole track
                statuses.franz.aliveTime = now;
                statuses.franz.aliveSign = ALIVE_READ;

                statuses.fdd.aliveTime   = now;
                statuses.fdd.aliveSign   = ALIVE_READ;

                handleSendTrack(inBuff + 8);
                break;

            default:
                Debug::out(LOG_ERROR, "CCoreThread received weird ATN code %02x waitForATN()", inBuff[3]);
                break;
            }
        }
#else
    flags.gotFranzFwVersion = true;
#endif
        load.busy.markEnd();                        // mark the end of the busy part of the code

        if(!gotAtn) {                                // no ATN was processed?
            Utils::sleepMs(1);                        // wait 1 ms...
        }
    }
}

void CCoreThread::handleAcsiCommand(BYTE *bufIn)
{
    Debug::out(LOG_DEBUG, "\n");

    dbgVars.prevAcsiCmdTime = dbgVars.thisAcsiCmdTime;
    dbgVars.thisAcsiCmdTime = Utils::getCurrentMs();

#if defined(ONPC_HIGHLEVEL)
    memcpy(bufIn, header, 14);                          // get the cmd from received header
#endif

    BYTE justCmd, tag1, tag2, module;
    BYTE *pCmd;
    BYTE isIcd = false;
    BYTE wasHandled = false;

    BYTE acsiId = bufIn[0] >> 5;                            // get just ACSI ID
    if(acsiIdInfo.acsiIDdevType[acsiId] == DEVTYPE_OFF) {    // if this ACSI ID is off, reply with error and quit
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
            wasHandled = true;

            pthread_mutex_lock(&shared.mtxConfigStreams);
            shared.configStream.acsi->processCommand(pCmd);
            pthread_mutex_unlock(&shared.mtxConfigStreams);
            break;

        case HOSTMOD_TRANSLATED_DISK:                   // translated disk command?
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
            wasHandled = true;
            pthread_mutex_lock(&shared.mtxImages);      // lock floppy images shared objects -- now used by floppy setup object
            floppySetup.processCommand(pCmd);
            pthread_mutex_unlock(&shared.mtxImages);    // unlock floppy images shared objects
            break;

        case HOSTMOD_NETWORK_ADAPTER:
            wasHandled = true;
            netAdapter.processCommand(pCmd);
            break;

        case HOSTMOD_MEDIA_STREAMING:
            wasHandled = true;
            MediaStreaming::getInstance()->processCommand(pCmd, dataTrans);
            break;
        }
    }

    if(!wasHandled) {           // if the command was not previously handled, it's probably just some SCSI command
        pthread_mutex_lock(&shared.mtxScsi);
        shared.scsi->processCommand(bufIn);                    // process the command
        pthread_mutex_unlock(&shared.mtxScsi);
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

    // if just floppy image configuration changed, set that we should send new floppy images config and quit
    if(type == SETTINGSUSER_FLOPPYIMGS) {
        setEnabledFloppyImgs = true;
        return;
    }

    if(type == SETTINGSUSER_TRANSLATED) {
        Settings s;
        bool newMountRawNotTrans = s.getBool("MOUNT_RAW_NOT_TRANS", 0);

        if(shared.mountRawNotTrans != newMountRawNotTrans) {       // mount strategy changed?
            shared.mountRawNotTrans = newMountRawNotTrans;

            Debug::out(LOG_DEBUG, "CCoreThread::reloadSettings -- USB media mount strategy changed, remounting");

            shared.devFinder_detachAndLook = true;
        }

        return;
    }

    if(type == SETTINGSUSER_FLOPPYCONF) {
        Settings s;
        s.loadFloppyConfig(&floppyConfig);

        setFloppyConfig = true;
        return;
    }

    if(type == SETTINGSUSER_FLOPPY_SLOT) {
        setFloppyImageLed(shared.imageSilo->getCurrentSlot());
        return;
    }

    // first dettach all the devices
    pthread_mutex_lock(&shared.mtxScsi);
    shared.scsi->detachAll();
    pthread_mutex_unlock(&shared.mtxScsi);

    // then load the new settings
    loadSettings();

    // and now try to attach everything back
    shared.devFinder_look = true;
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
    display_setLine(DISP_LINE_HDD_IDS, tmpLine1);
    display_setLine(DISP_LINE_HDD_TYPES, tmpLine2);
}

void CCoreThread::loadSettings(void)
{
    Debug::out(LOG_DEBUG, "CCoreThread::loadSettings");

    Settings s;
    s.loadAcsiIDs(&acsiIdInfo);
    s.loadFloppyConfig(&floppyConfig);

    fillDisplayLines();     // fill lines for front display

    shared.mountRawNotTrans = s.getBool("MOUNT_RAW_NOT_TRANS", 0);

    setEnabledIDbits    = true;
    setFloppyConfig     = true;
}

void CCoreThread::handleFwVersion_hans(void)
{
    BYTE fwVer[16];
    memset(fwVer, 0, 16);

    BYTE enabledIDbits, sdCardAcsiId;
    getIdBits(enabledIDbits, sdCardAcsiId);     // get the enabled IDs

    chipInterface->setHDDconfig(enabledIDbits, sdCardAcsiId, shared.imageSilo->getSlotBitmap(), setNewFloppyImageLed, newFloppyImageLed);
    chipInterface->getFWversion(true, fwVer);

    int chipIfType = chipInterface->chipInterfaceType();

    if(setNewFloppyImageLed) {                  // if was setting floppy LED
        setNewFloppyImageLed = false;           // don't sent this anymore (until needed)
    }

    flags.gotHansFwVersion = true;

    //----------------------------------
    // if HW info changed
    if(hwConfig.changed) {
        hwConfig.changed = false;

        setEnabledIDbits = true;                    // resend config 

        pthread_mutex_lock(&shared.mtxScsi);
        shared.scsi->updateTranslatedBootMedia();   // also update CE_DD bootsector with proper SCSI ID
        pthread_mutex_unlock(&shared.mtxScsi);

        fillDisplayLines();                         // fill lines for front display
        saveHwConfig();                             // save the new config
    }

    //----------------------------------
    // do the following only for chip interface v1 v2
    if(chipIfType == CHIP_IF_V1_V2) {
        int currentLed = fwVer[4];

        char recoveryLevel = fwVer[9];
        if(recoveryLevel != 0) {                                                        // if the recovery level is not empty
            if(recoveryLevel == 'R' || recoveryLevel == 'S' || recoveryLevel == 'T') {  // and it's a valid recovery level
                handleRecoveryCommands(recoveryLevel - 'Q');                            // handle recovery action
            }
        }

        if(lastFloppyImageLed != currentLed) {              // did the floppy image LED change since last time?
            lastFloppyImageLed = currentLed;

            Debug::out(LOG_DEBUG, "Floppy image changed to %d, forcing disk change", currentLed);

            shared.imageSilo->setCurrentSlot(currentLed);     // switch the floppy image

            diskChanged     = true;                         // also tell Franz that floppy changed
            setDiskChanged  = true;
        }

        Debug::out(LOG_DEBUG, "FW: Hans,  %d-%02d-%02d, LED is: %d", Update::versions.current.hans.getYear(), Update::versions.current.hans.getMonth(), Update::versions.current.hans.getDay(), currentLed);
    } else {
        Debug::out(LOG_DEBUG, "FW: FPGA %d-%02d-%02d", Update::versions.current.fpga.getYear(), Update::versions.current.fpga.getMonth(), Update::versions.current.fpga.getDay());
    }

    //----------------------------------

    if(shared.imageSilo->currentSlotHasNewContent()) {    // the content of current slot changed?
        Debug::out(LOG_DEBUG, "Content of current floppy image slot changed, forcing disk change");

        diskChanged     = true;                         // tell Franz that floppy changed
        setDiskChanged  = true;
    }

    // if should get the HW info and should quit
    if(flags.getHwInfo) {
        showHwVersion();                                // show what HW version we have found

        Debug::out(LOG_INFO, ">>> Terminating app, because it was used as HW INFO tool <<<\n");
        sigintReceived = 1;
        return;
    }

    //----------------------------------
    // do the following only for chip interface v1 v2
    if(chipIfType == CHIP_IF_V1_V2) {
        // if Xilinx HW vs FW mismatching, flash Xilinx again to fix the situation
        if(hwConfig.fwMismatch) {
            Update::createUpdateXilinxScript();

            Debug::out(LOG_ERROR, ">>> Terminating app, because there's Xilinx HW vs FW mismatch! <<<\n");
            sigintReceived = 1;
            return;
        }
    }
}

void CCoreThread::handleFwVersion_franz(void)
{
    BYTE fwVer[14];
    memset(fwVer,   0, 14);

    chipInterface->setFDDconfig(setFloppyConfig, floppyConfig.enabled, floppyConfig.id, floppyConfig.writeProtected, setDiskChanged, diskChanged);
    chipInterface->getFWversion(false, fwVer);

    if(setFloppyConfig) {                                       // did set floppy config? don't set again
        setFloppyConfig = false;
    }
    
    static bool franzHandledOnce = false;

    if(franzHandledOnce) {                                      // don't send the commands until we did receive at least one firmware message
        if(setDiskChanged) {
            if(diskChanged) {                                   // if the disk changed, change it to not-changed and let it send a command again in a second
                diskChanged = false;
            } else {                                            // if we're in the not-changed state, don't send it again
                setDiskChanged = false;
            }
        }
    }

    franzHandledOnce = true;
    flags.gotFranzFwVersion = true;

    int chipIfType = chipInterface->chipInterfaceType();

    if(chipIfType == CHIP_IF_V1_V2) {                   // only on IF v1 v2 show info about Franz
        Debug::out(LOG_DEBUG, "FW: Franz, %d-%02d-%02d", Update::versions.current.franz.getYear(), Update::versions.current.franz.getMonth(), Update::versions.current.franz.getDay());
    }
}

void CCoreThread::getIdBits(BYTE &enabledIDbits, BYTE &sdCardAcsiId)
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

void CCoreThread::setFloppyImageLed(int ledNo)
{
    if(ledNo >=0 && ledNo < 3) {                    // if the LED # is within expected range
        BYTE enabledImgs = shared.imageSilo->getSlotBitmap();

        if(enabledImgs & (1 << ledNo)) {            // if the required LED # is enabled, set it
            Debug::out(LOG_DEBUG, "Setting new floppy image LED to %d", ledNo);
            newFloppyImageLed       = ledNo;
            setNewFloppyImageLed    = true;
        }
    }

    if(ledNo == 0xff) {                             // if this is a request to turn the LEDs off, do it
        Debug::out(LOG_DEBUG, "Setting new floppy image LED to 0xff (no LED)");
        newFloppyImageLed       = ledNo;
        setNewFloppyImageLed    = true;
    }
}

void CCoreThread::handleSendTrack(BYTE *inBuf)
{
    static int prevTrack = 0;

    int side    = inBuf[0];                      // now read the current floppy position
    int track   = inBuf[1];

    int tr, si, spt;
    shared.imageSilo->getParams(tr, si, spt);      // read the floppy image params

    BYTE *encodedTrack;
    int countInTrack;

    if(side < 0 || side > 1 || track < 0 || track >= tr) {      // side / track out of range? use empty track
        Debug::out(LOG_ERROR, "handleSendTrack() -- side / track out of range, returning empty track. Franz wants: [track %d, side %d], but current image has %d tracks, %d sides, %d sectors/track", track, side, tr, si, spt);

        encodedTrack = shared.imageSilo->getEmptyTrack();
    } else {                                                    // side + track within range? use encoded track
        Debug::out(LOG_DEBUG, "handleSendTrack() -- Franz wants: [track %d, side %d]", track, side);

        encodedTrack = shared.imageSilo->getEncodedTrack(track, side, countInTrack);
    }

    int remaining = MFM_STREAM_SIZE - (4*2) - 2;    // this much bytes remain to send after the received ATN
    chipInterface->fdd_sendTrackToChip(remaining, encodedTrack);

    // now we should do some buzzing because of floppy seek
    if(prevTrack != track) {                        // track changed?
        int trackDiff = abs(prevTrack - track);     // get how many tracks we've moved
        beeper_floppySeek(trackDiff);               // do the beep
        prevTrack = track;
    }
}

void CCoreThread::handleSectorWritten(void)
{
    int side, track, sector, byteCount;
    BYTE *writtenSector = chipInterface->fdd_sectorWritten(side, track, sector, byteCount); // get side + track + sector number, byte count, and pointer to buffer where the written data is

    if(!floppyConfig.writeProtected) {  // not write protected? write
        Debug::out(LOG_DEBUG, "handleSectorWritten -- track %d, side %d, sector %d", track, side, sector);
        floppyEncoder_decodeMfmWrittenSector(track, side, sector, writtenSector, byteCount); // let floppy encoder handle decoding, reencoding, saving
    } else {                            // is write protected? don't write
        Debug::out(LOG_DEBUG, "handleSectorWritten -- floppy is write protected, not writing");
    }
}

void CCoreThread::handleRecoveryCommands(int recoveryLevel)
{
    Debug::out(LOG_DEBUG, "CCoreThread::handleRecoveryCommands() -- recoveryLevel is %d", recoveryLevel);

    switch(recoveryLevel) {
        case 1: // just insert config floppy image into slot 1
                insertSpecialFloppyImage(SPECIAL_FDD_IMAGE_CE_CONF);
                break;

        //----------------------------------------------------
        case 2: // delete settings, set network to DHCP
                Debug::out(LOG_INFO, ">>> CCoreThread::handleRecoveryCommands -- LEVEL 2 - removing settings, restarting whole linux <<<\n");

                deleteSettingAndSetNetworkToDhcp();         // delete all settings, set network to DHCP

                Debug::out(LOG_INFO, ">>> Terminating app and will reboot device, because app settings and network settings changed <<<\n");

                system("reboot");                           // reboot device
                sigintReceived = 1;                         // turn off app (probably not needed)
                break;

        //----------------------------------------------------
        case 3: // like 2, but also flash first firmware
                Debug::out(LOG_INFO, ">>> CCoreThread::handleRecoveryCommands -- LEVEL 3 - removing settings, flashing first FW <<<\n");

                deleteSettingAndSetNetworkToDhcp();         // delete all settings, set network to DHCP

                Update::createFlashFirstFwScript(true);     // create flash first fw script -- with linux reboot

                Debug::out(LOG_INFO, ">>> Terminating app, because will do flashFirstFw as a part of handleRecoveryCommands() ! <<<\n");
                sigintReceived = 1;                         // turn off app
                break;
    }
}

void CCoreThread::insertSpecialFloppyImage(int specialImageId)
{
    std::string imgFilename;
    std::string imgSrcPath;
    std::string imgFullPath;
    std::string dummy;

    if(specialImageId == SPECIAL_FDD_IMAGE_CE_CONF) {           // CE CONF image
        imgFilename = CE_CONF_FDD_IMAGE_JUST_FILENAME;
        imgSrcPath  = CE_CONF_FDD_IMAGE_PATH_AND_FILENAME;      // image in /ce/app dir
        imgFullPath = CE_CONF_FDD_IMAGE_PATH_AND_FILENAME_TMP;  // image in /tmp dir

        Utils::copyFile(imgSrcPath, imgFullPath);               // copy from /ce/app to /tmp to allow writing, but don't preserve changes

        Debug::out(LOG_INFO, "Will insert special FDD image: CE_CONF image");
    } else if(specialImageId == SPECIAL_FDD_IMAGE_FDD_TEST) {   // FDD TEST image
        imgFilename = FDD_TEST_IMAGE_JUST_FILENAME;
        imgSrcPath  = FDD_TEST_IMAGE_PATH_AND_FILENAME;         // image in /ce/app dir
        imgFullPath = FDD_TEST_IMAGE_PATH_AND_FILENAME_TMP;     // image in /tmp dir

        Utils::copyFile(imgSrcPath, imgFullPath);               // copy from /ce/app to /tmp to allow writing, but don't preserve changes

        Debug::out(LOG_INFO, "Will insert special FDD image: FDD TEST image");
    } else {
        Debug::out(LOG_INFO, "Unknown special image: %d, doing nothing.", specialImageId);
        return;
    }

    // encode MSA config image to MFM stream - in slot #0
    shared.imageSilo->add(0, imgFilename, imgFullPath, dummy, false);

    // set the current to slot #0
    shared.imageSilo->setCurrentSlot(0);

    // when encoding stops, set this FDD image LED
    newFloppyImageLedAfterEncode = 0;
}

void CCoreThread::deleteSettingAndSetNetworkToDhcp(void)
{
    // delete settings
    system("rm -f /ce/settings/*");

    // get the network settings
    NetworkSettings ns;
    ns.load();                      // load the current values
    ns.eth0.dhcpNotStatic   = true; // force DHCP on eth0
    ns.wlan0.dhcpNotStatic  = true; // force DHCP on wlan0
    ns.save();                      // save those settings

    // sync to write stuff to card
    system("sync");
}
