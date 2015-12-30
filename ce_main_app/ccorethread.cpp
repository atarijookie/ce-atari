#include <unistd.h>
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
#include "native/scsi_defs.h"
#include "gpio.h"
#include "mounter.h"
#include "downloader.h"
#include "update.h"
#include "config/netsettings.h"
#include "ce_conf_on_rpi.h" 

#include "periodicthread.h"

#if defined(ONPC_HIGHLEVEL)
    #include "socks.h"
#endif

#define DEV_CHECK_TIME_MS	    3000
#define UPDATE_CHECK_TIME       1000
#define INET_IFACE_CHECK_TIME   1000
#define UPDATE_SCRIPTS_TIME     10000

extern THwConfig    hwConfig;
extern TFlags       flags;

extern DebugVars    dbgVars;

SharedObjects shared;
extern volatile bool floppyEncodingRunning;

volatile BYTE insertSpecialFloppyImageId;

CCoreThread::CCoreThread(ConfigService* configService, FloppyService *floppyService, ScreencastService* screencastService)
{
    NetworkSettings ns;
    ns.updateResolvConf();    

    Update::initialize();

    setEnabledIDbits        = false;
    setEnabledFloppyImgs    = false;
    setNewFloppyImageLed    = false;
    setFloppyConfig         = false;
    setDiskChanged          = false;
    diskChanged             = false;

    memset(&hansConfigWords, 0, sizeof(hansConfigWords));
    
    lastFloppyImageLed      = -1;
    newFloppyImageLedAfterEncode = -2;
    
	conSpi		= new CConSpi();
    retryMod    = new RetryModule();

    dataTrans   = new AcsiDataTrans();
    dataTrans->setCommunicationObject(conSpi);
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

	settingsReloadProxy.addSettingsUser((ISettingsUser *) shared.translated,    SETTINGSUSER_TRANSLATED);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) shared.translated,    SETTINGSUSER_SHARED);

    // give floppy setup everything it needs
    floppySetup.setAcsiDataTrans(dataTrans);
    floppySetup.setImageSilo(&floppyImageSilo);
    floppySetup.setTranslatedDisk(shared.translated);
    floppySetup.setSettingsReloadProxy(&settingsReloadProxy);

    // the floppy image silo might change settings (when images are changes), add settings reload proxy
    floppyImageSilo.setSettingsReloadProxy(&settingsReloadProxy);
    settingsReloadProxy.reloadSettings(SETTINGSUSER_FLOPPYIMGS);            // mark that floppy settings changed (when imageSilo loaded the settings)

    //Floppy Service needs access to floppysilo and this thread
    if(floppyService) {
        floppyService->setImageSilo(&floppyImageSilo);
        floppyService->setCoreThread(this);
    }

    // set up network adapter stuff
    netAdapter.setAcsiDataTrans(dataTrans);
}

CCoreThread::~CCoreThread()
{
    delete conSpi;
    delete dataTrans;
    delete retryMod;
    
    sharedObjects_destroy();
}

void CCoreThread::sharedObjects_create(ConfigService* configService, FloppyService *floppyService, ScreencastService* screencastService)
{
    shared.devFinder_detachAndLook  = false;
    shared.devFinder_look           = false;

    shared.scsi        = new Scsi();
    shared.scsi->setAcsiDataTrans(dataTrans);
    shared.scsi->attachToHostPath(TRANSLATEDBOOTMEDIA_FAKEPATH, SOURCETYPE_IMAGE_TRANSLATEDBOOT, SCSI_ACCESSTYPE_FULL);

    shared.translated = new TranslatedDisk(dataTrans, configService, screencastService);
	shared.translated->setSettingsReloadProxy(&settingsReloadProxy);

    //-----------
    // create config stream for ACSI interface
    shared.configStream.acsi = new ConfigStream();
    shared.configStream.acsi->setAcsiDataTrans(dataTrans);
    shared.configStream.acsi->setSettingsReloadProxy(&settingsReloadProxy);
    
    // create config stream for web interface
    shared.configStream.dataTransWeb    = new AcsiDataTrans();
    shared.configStream.web             = new ConfigStream();
    shared.configStream.web->setAcsiDataTrans(shared.configStream.dataTransWeb);
    shared.configStream.web->setSettingsReloadProxy(&settingsReloadProxy);

    // create config stream for linux terminal
    shared.configStream.dataTransTerm   = new AcsiDataTrans();
    shared.configStream.term            = new ConfigStream();
    shared.configStream.term->setAcsiDataTrans(shared.configStream.dataTransTerm);
    shared.configStream.term->setSettingsReloadProxy(&settingsReloadProxy);
}

void CCoreThread::sharedObjects_destroy(void)
{
    delete shared.scsi;
    shared.scsi = NULL;
    
    delete shared.translated;
    shared.translated = NULL;
    
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

void CCoreThread::run(void)
{
    BYTE inBuff[8], outBuf[8];

	memset(outBuf, 0, 8);
    memset(inBuff, 0, 8);

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
        Utils::resetHansAndFranz();
    }

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

	bool res;
    
    DWORD nextFloppyEncodingCheck   = Utils::getEndTime(1000);
    bool prevFloppyEncodingRunning  = false;
    
    while(sigintReceived == 0) {
		bool gotAtn = false;						                    // no ATN received yet?
        
        // if should just get the HW version and HDD interface, but timeout passed, quit
        if(flags.getHwInfo && Utils::getCurrentMs() >= getHwInfoTimeout) {
            showHwVersion();                                            // show the default HW version
            sigintReceived = 1;                                         // quit
        }

        DWORD now = Utils::getCurrentMs();
        if(now >= lastFwInfoTime.nextDisplay) {
            lastFwInfoTime.nextDisplay  = Utils::getEndTime(1000);
            
            float hansTime  = ((float)(now - lastFwInfoTime.hans))  / 1000.0f;
            float franzTime = ((float)(now - lastFwInfoTime.franz)) / 1000.0f;
            
            hansTime    = (hansTime  < 15.0f) ? hansTime  : 15.0f;
            franzTime   = (franzTime < 15.0f) ? franzTime : 15.0f;
            
            bool hansAlive  = (hansTime < 3.0f);
            bool franzAlive = (franzTime < 3.0f);
            
            printf("\033[2K  [ %c ]  Hans: %.1f s (%s), Franz: %.1f s, (%s)\033[A\n", progChars[lastFwInfoTime.progress], hansTime, hansAlive ? "LIVE" : "DEAD", franzTime, franzAlive ? "LIVE" : "DEAD");
        
            lastFwInfoTime.progress = (lastFwInfoTime.progress + 1) % 4;
            
            if(!hansAlive && !flags.noReset && (now - lastFwInfoTime.hansResetTime) >= 3000) {
                printf("\033[2KHans not alive, resetting Hans.\n");
                lastFwInfoTime.hansResetTime = now;
                Utils::resetHans();
            }

            if(!franzAlive && !flags.noReset && (now - lastFwInfoTime.franzResetTime) >= 3000) {
                printf("\033[2KFranz not alive, resetting Franz.\n");
                lastFwInfoTime.franzResetTime = now;
                Utils::resetFranz();
            }
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
            
            if(prevFloppyEncodingRunning == true && floppyEncodingRunning == false) {   // if floppy encoding was running, but not it's not running
                if(newFloppyImageLedAfterEncode != -2) {                                // if we should set the new newFloppyImageLed after encoding is done
                    setEnabledFloppyImgs    = true;
                    setNewFloppyImageLed    = true;
                    newFloppyImageLed       = newFloppyImageLedAfterEncode;
                
                    newFloppyImageLedAfterEncode = -2;
                } 
            } 
            
            prevFloppyEncodingRunning = floppyEncodingRunning;
        }
        
#if !defined(ONPC_HIGHLEVEL)
        // check for any ATN code waiting from Hans
		res = conSpi->waitForATN(SPI_CS_HANS, (BYTE) ATN_ANY, 0, inBuff);

		if(res) {									    // HANS is signaling attention?
			gotAtn = true;							    // we've some ATN

			switch(inBuff[3]) {
			case ATN_FW_VERSION:
                lastFwInfoTime.hans = Utils::getCurrentMs();
    			handleFwVersion_hans();
				break;

			case ATN_ACSI_COMMAND:
                dbgVars.isInHandleAcsiCommand = 1;
				
                handleAcsiCommand();
                
                dbgVars.isInHandleAcsiCommand = 0;
				break;

			default:
				Debug::out(LOG_ERROR, (char *) "CCoreThread received weird ATN code %02x waitForATN()", inBuff[3]);
				break;
			}
		}
#endif

        if(insertSpecialFloppyImageId != 0) {                       // very stupid way of letting web IF to insert special image
            insertSpecialFloppyImage(insertSpecialFloppyImageId);
            insertSpecialFloppyImageId = 0;
        }

#if defined(ONPC_HIGHLEVEL)
        res = gotCmd();

        if(res) {
            handleAcsiCommand();
        }
#endif

#if !defined(ONPC_GPIO) && !defined(ONPC_HIGHLEVEL)
        // check for any ATN code waiting from Franz
        if(flags.noFranz) {                         // if running without Franz, don't communicate
            res = false;
        } else {                                    // running with Franz - check for any ATN
            res = conSpi->waitForATN(SPI_CS_FRANZ, (BYTE) ATN_ANY, 0, inBuff);
        }
        
		if(res) {									// FRANZ is signaling attention?
			gotAtn = true;							// we've some ATN

			switch(inBuff[3]) {
			case ATN_FW_VERSION:                    // device has sent FW version
                lastFwInfoTime.franz = Utils::getCurrentMs();
				handleFwVersion_franz();
				break;

            case ATN_SECTOR_WRITTEN:                // device has sent written sector data
                handleSectorWritten();
                break;

			case ATN_SEND_TRACK:                    // device requests data of a whole track
				handleSendTrack();
				break;

			default:
				Debug::out(LOG_ERROR, (char *) "CCoreThread received weird ATN code %02x waitForATN()", inBuff[3]);
				break;
			}
		}
#else
    flags.gotFranzFwVersion = true;
#endif

		if(!gotAtn) {								// no ATN was processed?
			Utils::sleepMs(1);						// wait 1 ms...
		}
    }
}

void CCoreThread::handleAcsiCommand(void)
{
    Debug::out(LOG_DEBUG, "\n");
    
    BYTE bufOut[ACSI_CMD_SIZE];
    BYTE bufIn[ACSI_CMD_SIZE];

    memset(bufOut,  0, ACSI_CMD_SIZE);
    memset(bufIn,   0, ACSI_CMD_SIZE);

    dbgVars.prevAcsiCmdTime = dbgVars.thisAcsiCmdTime;
    dbgVars.thisAcsiCmdTime = Utils::getCurrentMs();
    
#if defined(ONPC_HIGHLEVEL)
    memcpy(bufIn, header, 14);                          // get the cmd from received header
#else
    conSpi->txRx(SPI_CS_HANS, 14, bufOut, bufIn);       // get 14 cmd bytes
#endif

    BYTE justCmd, tag1, tag2, module;
    BYTE *pCmd;
    BYTE isIcd = false;
    BYTE wasHandled = false;

    BYTE acsiId = bufIn[0] >> 5;                        	// get just ACSI ID
    if(acsiIdInfo.acsiIDdevType[acsiId] == DEVTYPE_OFF) {	// if this ACSI ID is off, reply with error and quit
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
        Debug::out(LOG_DEBUG, "handleAcsiCommand: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", bufIn[0], bufIn[1], bufIn[2], bufIn[3], bufIn[4], bufIn[5], bufIn[6], bufIn[7], bufIn[8], bufIn[9], bufIn[10], bufIn[11], bufIn[12], bufIn[13]);
	    Debug::out(LOG_DEBUG, "handleAcsiCommand isIcd");
	} else {
        Debug::out(LOG_DEBUG, "handleAcsiCommand: %02x %02x %02x %02x %02x %02x", bufIn[0], bufIn[1], bufIn[2], bufIn[3], bufIn[4], bufIn[5]);
    }

    // if it's the retry module (highest bit in HOSTMOD is set), let it go before the big switch for modules, 
    // because it will change the command and let it possibly run trough the correct module
    if(module & 0x80) {                         // if it's a RETRY attempt...
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
            
            pthread_mutex_lock(&shared.mtxTranslated);
            shared.translated->processCommand(pCmd);
            pthread_mutex_unlock(&shared.mtxTranslated);
            break;

        case HOSTMOD_FDD_SETUP:                         // floppy setup command?
            wasHandled = true;
            floppySetup.processCommand(pCmd);
            break;

        case HOSTMOD_NETWORK_ADAPTER:
            wasHandled = true;
            netAdapter.processCommand(pCmd);
            break;
        }
    }

    if(wasHandled != true) {                            // if the command was not previously handled, it's probably just some SCSI command
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
        bool newMountRawNotTrans = s.getBool((char *) "MOUNT_RAW_NOT_TRANS", 0);
        
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
        setFloppyImageLed(floppyImageSilo.getCurrentSlot());
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

void CCoreThread::loadSettings(void)
{
    Debug::out(LOG_DEBUG, "CCoreThread::loadSettings");

    Settings s;
	s.loadAcsiIDs(&acsiIdInfo);
    s.loadFloppyConfig(&floppyConfig);

    shared.mountRawNotTrans = s.getBool((char *) "MOUNT_RAW_NOT_TRANS", 0);
    
    setEnabledIDbits    = true;
    setFloppyConfig     = true;
}

#define MAKEWORD(A, B)  ( (((WORD)A)<<8) | ((WORD)B) )

void CCoreThread::handleFwVersion_hans(void)
{
    BYTE fwVer[14], oBuf[14];
	int cmdLength;

    memset(oBuf,    0, 14);                                         // first clear the output buffer
    memset(fwVer,   0, 14);

    // WORD sent (bytes shown): 01 23 45 67

    cmdLength = 12;
    responseStart(cmdLength);                                       // init the response struct

    //--------------
    // send the ACSI + SD config + FDD enabled images, when they changed from current values to something new
    BYTE enabledIDbits, sdCardAcsiId;
    getIdBits(enabledIDbits, sdCardAcsiId);                                 // get the enabled IDs 
    
    hansConfigWords.next.acsi   = MAKEWORD(enabledIDbits,                   sdCardAcsiId);
    hansConfigWords.next.fdd    = MAKEWORD(floppyImageSilo.getSlotBitmap(), 0);
    
    if( (hansConfigWords.next.acsi  != hansConfigWords.current.acsi) || 
        (hansConfigWords.next.fdd   != hansConfigWords.current.fdd )) {
        
        // hansConfigWords.skipNextSet - it's a flag used for skipping one config sending, because we send the new config now, but receive it processed in the next (not this) fw version packet
        
        if(!hansConfigWords.skipNextSet) {                      
            responseAddWord(oBuf, CMD_ACSI_CONFIG);             // CMD: send acsi config
            responseAddWord(oBuf, hansConfigWords.next.acsi);   // store ACSI enabled IDs and which ACSI ID is used for SD card
            responseAddWord(oBuf, hansConfigWords.next.fdd);    // store which floppy images are enabled
            
            hansConfigWords.skipNextSet = true;                 // we have just sent the config, skip the next sending, so we won't send it twice in a row
        } else {                                                // if we should skip sending config this time, then don't skip it next time (if needed)
            hansConfigWords.skipNextSet = false;
        }
    }
    //--------------

    if(setNewFloppyImageLed) {
        responseAddWord(oBuf, CMD_FLOPPY_SWITCH);               // CMD: set new image LED (bytes 8 & 9)
        responseAddWord(oBuf, MAKEWORD(floppyImageSilo.getSlotBitmap(), newFloppyImageLed));  // store which floppy images LED should be on
        setNewFloppyImageLed = false;                           // and don't sent this anymore (until needed)
    }

    conSpi->txRx(SPI_CS_HANS, cmdLength, oBuf, fwVer);

    int year = bcdToInt(fwVer[1]) + 2000;

    Update::versions.current.hans.fromInts(year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));       // store found FW version of Hans
    flags.gotHansFwVersion = true;

    int  currentLed = fwVer[4];
    BYTE xilinxInfo = fwVer[5];

    hansConfigWords.current.acsi    = MAKEWORD(fwVer[6], fwVer[7]);
    hansConfigWords.current.fdd     = MAKEWORD(fwVer[8],        0);
    
    char recoveryLevel = fwVer[9];
    if(recoveryLevel != 0) {                                                        // if the recovery level is not empty
        if(recoveryLevel == 'R' || recoveryLevel == 'S' || recoveryLevel == 'T') {  // and it's a valid recovery level 
            handleRecoveryCommands(recoveryLevel - 'Q');                            // handle recovery action
        }
    }
    
    convertXilinxInfo(xilinxInfo);
    
    Debug::out(LOG_DEBUG, "FW: Hans,  %d-%02d-%02d, LED is: %d, XI: 0x%02x", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]), currentLed, xilinxInfo);

    if(floppyImageSilo.currentSlotHasNewContent()) {    // the content of current slot changed? 
        Debug::out(LOG_DEBUG, "Content of current floppy image slot changed, forcing disk change", currentLed);

        diskChanged     = true;                         // tell Franz that floppy changed
        setDiskChanged  = true;
    }
    
    if(lastFloppyImageLed != currentLed) {              // did the floppy image LED change since last time?
        lastFloppyImageLed = currentLed;

        Debug::out(LOG_DEBUG, "Floppy image changed to %d, forcing disk change", currentLed);

        floppyImageSilo.setCurrentSlot(currentLed);     // switch the floppy image

        diskChanged     = true;                         // also tell Franz that floppy changed
        setDiskChanged  = true;
    }

    // if should get the HW info and should quit
    if(flags.getHwInfo) {
        showHwVersion();                                // show what HW version we have found

        Debug::out(LOG_INFO, ">>> Terminating app, because it was used as HW INFO tool <<<\n");
        sigintReceived = 1;
        return;
    }

    // if Xilinx HW vs FW mismatching, flash Xilinx again to fix the situation
    if(hwConfig.fwMismatch) {
        Update::createUpdateXilinxScript();
    
        Debug::out(LOG_ERROR, ">>> Terminating app, because there's Xilinx HW vs FW mismatch! <<<\n");
        sigintReceived = 1;
        return;
    }
}

void CCoreThread::handleFwVersion_franz(void)
{
    BYTE fwVer[14], oBuf[14];
	int cmdLength;

    memset(oBuf,    0, 14);                                         // first clear the output buffer
    memset(fwVer,   0, 14);

    // WORD sent (bytes shown): 01 23 45 67

    cmdLength = 8;
    responseStart(cmdLength);                                   // init the response struct

    static bool franzHandledOnce = false;

    if(franzHandledOnce) {                                      // don't send the commands until we did receive at least one firmware message
        if(setFloppyConfig) {                                   // should set floppy config?
            responseAddByte(oBuf, ( floppyConfig.enabled        ? CMD_DRIVE_ENABLED     : CMD_DRIVE_DISABLED) );
            responseAddByte(oBuf, ((floppyConfig.id == 0)       ? CMD_SET_DRIVE_ID_0    : CMD_SET_DRIVE_ID_1) );
            responseAddByte(oBuf, ( floppyConfig.writeProtected ? CMD_WRITE_PROTECT_ON  : CMD_WRITE_PROTECT_OFF) );
            setFloppyConfig = false;
        }

        if(setDiskChanged) {
            responseAddByte(oBuf, ( diskChanged                 ? CMD_DISK_CHANGE_ON    : CMD_DISK_CHANGE_OFF) );

            if(diskChanged) {                                   // if the disk changed, change it to not-changed and let it send a command again in a second
                diskChanged = false;
            } else {                                            // if we're in the not-changed state, don't send it again
                setDiskChanged = false;
            }
        }
    }

    franzHandledOnce = true;

    conSpi->txRx(SPI_CS_FRANZ, cmdLength, oBuf, fwVer);

    int year = bcdToInt(fwVer[1]) + 2000;
    Update::versions.current.franz.fromInts(year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));              // store found FW version of Franz
    flags.gotFranzFwVersion = true;

    Debug::out(LOG_DEBUG, "FW: Franz, %d-%02d-%02d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));
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

void CCoreThread::convertXilinxInfo(BYTE xilinxInfo)
{
    int prevHwHddIface = hwConfig.hddIface; 

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
        case 0x11:  // use this for v.1 
        default:    // and also for all other cases
                    hwConfig.version        = 1;
                    hwConfig.hddIface       = HDD_IF_ACSI;
                    hwConfig.fwMismatch     = false;
                    break;
    }
    
    // if the HD IF changed (received the 1st HW info) and we're on SCSI bus, we need to send the new (limited) SCSI IDs to Hans, so he won't answer on Initiator SCSI ID
    if((prevHwHddIface != hwConfig.hddIface) && hwConfig.hddIface == HDD_IF_SCSI) {
        Debug::out(LOG_DEBUG, "Found out that we're running on SCSI bus - will resend the ID bits configuration to Hans");
        setEnabledIDbits = true;
    }
    
    saveHwConfig();
}

void CCoreThread::saveHwConfig(void)
{
    Settings s;
    
    int ver, hddIf, scsiMch;
    
    // get current values for these configs
    ver     = s.getInt((char *) "HW_VERSION",       1);
    hddIf   = s.getInt((char *) "HW_HDD_IFACE",     HDD_IF_ACSI);
    scsiMch = s.getInt((char *) "HW_SCSI_MACHINE",  SCSI_MACHINE_UNKNOWN);

    // store value only if it has changed
    if(ver != hwConfig.version) {
        s.setInt((char *) "HW_VERSION", ver);
    }
    
    if(hddIf != hwConfig.hddIface) {
        s.setInt((char *) "HW_HDD_IFACE", hddIf);
    }
    
    if(scsiMch != hwConfig.scsiMachine) {
        s.setInt((char *) "HW_SCSI_MACHINE", scsiMch);
    }
}

void CCoreThread::showHwVersion(void)
{
    char tmp[256];

    Debug::out(LOG_INFO, "Reporting this as HW INFO...");     // show in log file

    // HW version is 1 or 2, and in other cases defaults to 1
    sprintf(tmp, "HW_VER: %d", (hwConfig.version == 1 || hwConfig.version == 2) ? hwConfig.version : 1);
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

void CCoreThread::responseStart(int bufferLengthInBytes)        // use this to start creating response (commands) to Hans or Franz
{
    response.bfrLengthInBytes   = bufferLengthInBytes;
    response.currentLength      = 0;
}

void CCoreThread::responseAddWord(BYTE *bfr, WORD value)        // add a WORD to the response (command) to Hans or Franz
{
    if(response.currentLength >= response.bfrLengthInBytes) {
        return;
    }

    bfr[response.currentLength + 0] = (BYTE) (value >> 8);
    bfr[response.currentLength + 1] = (BYTE) (value & 0xff);
    response.currentLength += 2;
}

void CCoreThread::responseAddByte(BYTE *bfr, BYTE value)        // add a BYTE to the response (command) to Hans or Franz
{
    if(response.currentLength >= response.bfrLengthInBytes) {
        return;
    }

    bfr[response.currentLength] = value;
    response.currentLength++;
}

void CCoreThread::setFloppyImageLed(int ledNo)
{
    if(ledNo >=0 && ledNo < 3) {                    // if the LED # is within expected range
        BYTE enabledImgs    = floppyImageSilo.getSlotBitmap();

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

int CCoreThread::bcdToInt(int bcd)
{
    int a,b;

    a = bcd >> 4;       // upper nibble
    b = bcd &  0x0f;    // lower nibble

    return ((a * 10) + b);
}

void CCoreThread::handleSendTrack(void)
{
    BYTE oBuf[2], iBuf[15000];

    memset(oBuf, 0, 2);
    conSpi->txRx(SPI_CS_FRANZ, 2, oBuf, iBuf);

    int side    = iBuf[0];                      // now read the current floppy position
    int track   = iBuf[1];

    int remaining   = 15000 - (4*2) - 2;		// this much bytes remain to send after the received ATN

    Debug::out(LOG_DEBUG, "ATN_SEND_TRACK -- track %d, side %d", track, side);

    int tr, si, spt;
    floppyImageSilo.getParams(tr, si, spt);      // read the floppy image params

    BYTE *encodedTrack;
    int countInTrack;

    if(side < 0 || side > 1 || track < 0 || track >= tr) {      // side / track out of range? use empty track
        Debug::out(LOG_ERROR, "Side / Track out of range, returning empty track");
        
        encodedTrack = floppyImageSilo.getEmptyTrack();
    } else {                                                    // side + track within range? use encoded track
        encodedTrack = floppyImageSilo.getEncodedTrack(track, side, countInTrack);
    }

    conSpi->txRx(SPI_CS_FRANZ, remaining, encodedTrack, iBuf);
}

void CCoreThread::handleSectorWritten(void)
{
    #define BUFFSIZE    2048
    BYTE oBuf[BUFFSIZE], iBuf[BUFFSIZE];

    memset(oBuf, 0, BUFFSIZE);

    WORD remainingSize = conSpi->getRemainingLength();              // get how many data we still have
    conSpi->txRx(SPI_CS_FRANZ, remainingSize, oBuf, iBuf);          // get all the remaining data

    // get the written sector, side, track number
    int sector  = iBuf[0];
    int track   = iBuf[1] & 0x7f;
    int side    = (iBuf[1] & 0x80) ? 1 : 0;

    Debug::out(LOG_DEBUG, "handleSectorWritten -- track %d, side %d, sector %d -- TODO!!!", track, side, sector);

    // TODO:
    // do the written sector processing
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
    std::string ceConfFilename;
    std::string ceConfFullPath;
    std::string dummy;

    if(specialImageId == SPECIAL_FDD_IMAGE_CE_CONF) {               // CE CONF image
        ceConfFilename = CE_CONF_FDD_IMAGE_JUST_FILENAME;
        ceConfFullPath = CE_CONF_FDD_IMAGE_PATH_AND_FILENAME;
        
        Debug::out(LOG_INFO, "Will insert special FDD image: CE_CONF image");
    } else if(specialImageId == SPECIAL_FDD_IMAGE_FDD_TEST) {       // FDD TEST image
        ceConfFilename = FDD_TEST_IMAGE_JUST_FILENAME;
        ceConfFullPath = FDD_TEST_IMAGE_PATH_AND_FILENAME;

        Debug::out(LOG_INFO, "Will insert special FDD image: FDD TEST image");
    } else {
        Debug::out(LOG_INFO, "Unknown special image: %d, doing nothing.", specialImageId);
        return;
    }
    
    // encode MSA config image to MFM stream - in slot #0
    floppyImageSilo.add(0, ceConfFilename, ceConfFullPath, dummy, dummy, false);
    
    // set the current to slot #0
    floppyImageSilo.setCurrentSlot(0);

    // when encoding stops, set this FDD image LED
    newFloppyImageLedAfterEncode = 0;
}
                
void CCoreThread::deleteSettingAndSetNetworkToDhcp(void)
{
    // delete settings
    system("rm -f /ce/settings/*");
    
    // get the network settings
	NetworkSettings ns;
	ns.load();						// load the current values     
    ns.eth0.dhcpNotStatic   = true; // force DHCP on eth0
    ns.wlan0.dhcpNotStatic  = true; // force DHCP on wlan0
    ns.save();                      // save those settings

    // sync to write stuff to card
    system("sync");
}
