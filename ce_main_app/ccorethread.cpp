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

#if defined(ONPC_HIGHLEVEL)
    #include "socks.h"
#endif

#define DEV_CHECK_TIME_MS	    3000
#define UPDATE_CHECK_TIME       1000
#define INET_IFACE_CHECK_TIME   1000
#define UPDATE_SCRIPTS_TIME     10000

#define WEB_PARAMS_CHECK_TIME_MS    3000
#define WEB_PARAMS_FILE             "/tmp/ce_startupmode"

extern THwConfig    hwConfig;
extern TFlags       flags;

extern DebugVars    dbgVars;

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

    lastFloppyImageLed      = -1;

	conSpi		= new CConSpi();
    retryMod    = new RetryModule();

    dataTrans   = new AcsiDataTrans();
    dataTrans->setCommunicationObject(conSpi);
    dataTrans->setRetryObject(retryMod);
    
    scsi        = new Scsi();
    scsi->setAcsiDataTrans(dataTrans);
    scsi->attachToHostPath(TRANSLATEDBOOTMEDIA_FAKEPATH, SOURCETYPE_IMAGE_TRANSLATEDBOOT, SCSI_ACCESSTYPE_FULL);
//  scsi->attachToHostPath("TESTMEDIA", SOURCETYPE_TESTMEDIA, SCSI_ACCESSTYPE_FULL);

    translated = new TranslatedDisk(dataTrans, configService, screencastService);
	translated->setSettingsReloadProxy(&settingsReloadProxy);

    confStream = new ConfigStream();
    confStream->setAcsiDataTrans(dataTrans);
	confStream->setSettingsReloadProxy(&settingsReloadProxy);

    // now register all the objects which use some settings in the proxy
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_ACSI);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_FLOPPYIMGS);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_FLOPPYCONF);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_FLOPPY_SLOT);
	settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_TRANSLATED);
	settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_SCSI_IDS);
    
    settingsReloadProxy.addSettingsUser((ISettingsUser *) scsi,          SETTINGSUSER_ACSI);

	settingsReloadProxy.addSettingsUser((ISettingsUser *) translated,    SETTINGSUSER_TRANSLATED);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) translated,    SETTINGSUSER_SHARED);

	// register this class as receiver of dev attached / detached calls
	devFinder.setDevChangesHandler((DevChangesHandler *) this);

    // give floppy setup everything it needs
    floppySetup.setAcsiDataTrans(dataTrans);
    floppySetup.setImageSilo(&floppyImageSilo);
    floppySetup.setTranslatedDisk(translated);
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
    delete scsi;
    delete retryMod;

    delete translated;
    delete confStream;
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

	DWORD nextDevFindTime       = Utils::getCurrentMs();                    // create a time when the devices should be checked - and that time is now
    DWORD nextUpdateCheckTime   = Utils::getEndTime(5000);                  // create a time when update download status should be checked
    DWORD nextWebParsCheckTime  = Utils::getEndTime(WEB_PARAMS_CHECK_TIME_MS);  // create a time when we should check for new params from the web page
    DWORD nextInetIfaceCheckTime= Utils::getEndTime(1000);  			    // create a time when we should check for inet interfaces that got ready

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
    
	//up/running state of inet interfaces
	bool  state_eth0 	= false;
	bool  state_wlan0 	= false;

    Update::downloadUpdateList(NULL);                                   // download the list of components with the newest available versions

	bool res;

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
            if(Utils::getCurrentMs() >= hansFranzAliveCheckTime) {      // did enough time pass since the Hans and Franz reset?
                if(!flags.gotHansFwVersion || !flags.gotFranzFwVersion) {       // if don't have version from Hans or Franz, then they're not alive
                    Update::createFlashFirstFwScript();

                    Debug::out(LOG_INFO, "No answer from Hans or Franz, so first firmware flash script created, will do first firmware flashing.");
					sigintReceived = 1;
                } else {
                    Debug::out(LOG_DEBUG, "Got answers from both Hans and Franz, so not doing first firmware flashing.");
                }

                shouldCheckHansFranzAlive = false;                      // don't check this again
            }
        }

        // should we check if there are new params received from debug web page?
        if(Utils::getCurrentMs() >= nextWebParsCheckTime) {
            readWebStartupMode();
            nextWebParsCheckTime  = Utils::getEndTime(WEB_PARAMS_CHECK_TIME_MS);
        }

        // should we check for the new devices?
		if(Utils::getCurrentMs() >= nextDevFindTime) {
			devFinder.lookForDevChanges();				                // look for devices attached / detached

			nextDevFindTime = Utils::getEndTime(DEV_CHECK_TIME_MS);		// update the time when devices should be checked

            if(!Update::versions.updateListWasProcessed) {              // if didn't process update list yet
                Update::processUpdateList();

                if(Update::versions.updateListWasProcessed) {           // if we processed the list, update config stream
                    confStream->fillUpdateWithCurrentVersions();        // if the config screen is shown, then update info on it
                }
            }
		}

        // should check the update status?
        if(Utils::getCurrentMs() >= nextUpdateCheckTime) {
            nextUpdateCheckTime   = Utils::getEndTime(UPDATE_CHECK_TIME);   // update the time when we should check update status again

            int updateState = Update::state();                              // get the update state
            switch(updateState) {
                case UPDATE_STATE_DOWNLOADING:
                confStream->fillUpdateDownloadWithProgress();               // refresh config screen with download status
                break;

                //-----------
                case UPDATE_STATE_DOWNLOAD_FAIL:
                confStream->showUpdateDownloadFail();                       // show fail message on config screen
                Update::stateGoIdle();
				Debug::out(LOG_ERROR, "Update state - download failed");
                break;

                //-----------
                case UPDATE_STATE_DOWNLOAD_OK:

                if(!confStream->isUpdateDownloadPageShown()) {              // if user is NOT waiting on download page (cancel pressed), don't update
                    Update::stateGoIdle();
					Debug::out(LOG_DEBUG, "Update state - download OK, but user is not on download page - NOT doing update");
                } else {                                                    // if user is waiting on download page, aplly update
                    res = Update::createUpdateScript();

                    if(!res) {
                        confStream->showUpdateError();
						Debug::out(LOG_ERROR, "Update state - download OK, failed to create update script - NOT doing update");
                    } else {
						Debug::out(LOG_INFO, "Update state - download OK, update script created, will do update.");
						sigintReceived = 1;
                    }
                }

                break;
            }
        }

        // should we check for inet interfaces that might came up?
        if(Utils::getCurrentMs() >= nextInetIfaceCheckTime) {
		    nextInetIfaceCheckTime  = Utils::getEndTime(INET_IFACE_CHECK_TIME);   // update the time when we should interfaces again

            bool state_temp 		= 	inetIfaceReady("eth0"); 
			bool change_to_enabled	= 	(state_eth0!=state_temp)&&!state_eth0; 	//was this iface disabled and current state changed to enabled?
			state_eth0 				= 	state_temp; 

            state_temp 				= 	inetIfaceReady("wlan0"); 
			change_to_enabled 		|= 	(state_wlan0!=state_temp)&&!state_wlan0;
			state_wlan0 			= 	state_temp; 
			
			if( change_to_enabled ){ 
				Debug::out(LOG_DEBUG, "Internet interface comes up: reload network mount settings");
				translated->reloadSettings(SETTINGSUSER_TRANSLATED);
			}
		}

#if !defined(ONPC_HIGHLEVEL)
        // check for any ATN code waiting from Hans
		res = conSpi->waitForATN(SPI_CS_HANS, (BYTE) ATN_ANY, 0, inBuff);

		if(res) {									    // HANS is signaling attention?
			gotAtn = true;							    // we've some ATN

			switch(inBuff[3]) {
			case ATN_FW_VERSION:
                lastFwInfoTime.hans = Utils::getCurrentMs();
    			handleFwVersion(SPI_CS_HANS);
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
				handleFwVersion(SPI_CS_FRANZ);
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
        
        int bytesAvailable;
        if(ce_conf_fd1 > 0 && ce_conf_fd2 > 0) {                        // if we got the ce_conf FIFO handles
            int ires = ioctl(ce_conf_fd1, FIONREAD, &bytesAvailable);   // how many bytes we can read?

            if(ires != -1 && bytesAvailable >= 3) {                     // if there are at least 3 bytes waiting
                BYTE cmd[6] = {0, 'C', 'E', 0, 0, 0};
                ires = read(ce_conf_fd1, cmd + 3, 3);                   // read the byte triplet
                
                Debug::out(LOG_DEBUG, "confStream - through FIFO: %02x %02x %02x (ires = %d)", cmd[3], cmd[4], cmd[5], ires);
                
                confStream->processCommand(cmd, ce_conf_fd2);
            }
        }

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
            confStream->processCommand(pCmd);
            break;

        case HOSTMOD_TRANSLATED_DISK:                   // translated disk command?
            wasHandled = true;
            translated->processCommand(pCmd);
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
        scsi->processCommand(bufIn);                    // process the command
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
        
        if(mountRawNotTrans != newMountRawNotTrans) {       // mount strategy changed?
            mountRawNotTrans = newMountRawNotTrans;

            Debug::out(LOG_DEBUG, "CCoreThread::reloadSettings -- USB media mount strategy changed, remounting");
        
            translated->detachAllUsbMedia();                // detach all translated USB media
            scsi->detachAllUsbMedia();                      // detach all RAW USB media

            // and now try to attach everything back
            devFinder.clearMap();						    // make all the devices appear as new
            devFinder.lookForDevChanges();					// and now find all the devices
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
	scsi->detachAll();

	// then load the new settings
    loadSettings();

	// and now try to attach everything back
	devFinder.clearMap();									// make all the devices appear as new
	devFinder.lookForDevChanges();							// and now find all the devices
}

void CCoreThread::loadSettings(void)
{
    Debug::out(LOG_DEBUG, "CCoreThread::loadSettings");

    Settings s;
	s.loadAcsiIDs(&acsiIdInfo);
    s.loadFloppyConfig(&floppyConfig);

    mountRawNotTrans = s.getBool((char *) "MOUNT_RAW_NOT_TRANS", 0);
    
    setEnabledIDbits    = true;
    setFloppyConfig     = true;
}

#define MAKEWORD(A, B)  ( (((WORD)A)<<8) | ((WORD)B) )

void CCoreThread::handleFwVersion(int whichSpiCs)
{
    BYTE fwVer[14], oBuf[14];
	int cmdLength;

    memset(oBuf,    0, 14);                                         // first clear the output buffer
    memset(fwVer,   0, 14);

    // WORD sent (bytes shown): 01 23 45 67

    if(whichSpiCs == SPI_CS_HANS) {                                 // it's Hans?
		cmdLength = 12;
        responseStart(cmdLength);                                   // init the response struct

        static bool hansHandledOnce = false;

        if(hansHandledOnce) {                                       // don't send commands until we did receive status at least a couple of times
            if(setEnabledIDbits) {                                  // if we should send ACSI ID configuration
                BYTE enabledIDbits, sdCardAcsiId;
                getIdBits(enabledIDbits, sdCardAcsiId);             // get the enabled IDs 
                
                responseAddWord(oBuf, CMD_ACSI_CONFIG);             // CMD: send acsi config
                responseAddWord(oBuf, MAKEWORD(enabledIDbits, sdCardAcsiId)); // store ACSI enabled IDs and which ACSI ID is used for SD card
                setEnabledIDbits = false;                           // and don't sent this anymore (until needed)
            }

            if(setEnabledFloppyImgs) {
                responseAddWord(oBuf, CMD_FLOPPY_CONFIG);                               // CMD: send which floppy images are enabled (bytes 4 & 5)
                responseAddWord(oBuf, MAKEWORD(floppyImageSilo.getSlotBitmap(), 0));    // store which floppy images are enabled
                setEnabledFloppyImgs = false;                                           // and don't sent this anymore (until needed)
            }

            if(setNewFloppyImageLed) {
                responseAddWord(oBuf, CMD_FLOPPY_SWITCH);               // CMD: set new image LED (bytes 8 & 9)
                responseAddWord(oBuf, MAKEWORD(newFloppyImageLed, 0));  // store which floppy images LED should be on
                setNewFloppyImageLed = false;                           // and don't sent this anymore (until needed)
            }
        }

        hansHandledOnce = true;
    } else {                                    		            // it's Franz?
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
    }

    conSpi->txRx(whichSpiCs, cmdLength, oBuf, fwVer);

    int year = bcdToInt(fwVer[1]) + 2000;
    if(fwVer[0] == 0xf0) {
        Update::versions.current.franz.fromInts(year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));              // store found FW version of Franz
        flags.gotFranzFwVersion = true;

        Debug::out(LOG_DEBUG, "FW: Franz, %d-%02d-%02d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));
    } else {
        Update::versions.current.hans.fromInts(year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));               // store found FW version of Hans
        flags.gotHansFwVersion = true;

        int  currentLed = fwVer[4];
        BYTE xilinxInfo = fwVer[5];

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
            Update::createFlashFirstFwScript();
        
            Debug::out(LOG_ERROR, ">>> Terminating app, because there's Xilinx HW vs FW mismatch! <<<\n");
            sigintReceived = 1;
            return;
        }
    }
}

void CCoreThread::getIdBits(BYTE &enabledIDbits, BYTE &sdCardAcsiId)
{
    // get the bits from struct
    enabledIDbits  = acsiIdInfo.enabledIDbits;
    sdCardAcsiId   = acsiIdInfo.sdCardAcsiId;
    
    if(hwConfig.hddIface != HDD_IF_SCSI) {          // not SCSI? Don't change anything
        Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on ACSI");
        return;
    }
    
    // if we're on SCSI bus, remove ID bits if they are used for SCSI Initiator on that machine (ID 7 on TT, ID 0 on Falcon)
    switch(hwConfig.scsiMachine) {
        case SCSI_MACHINE_TT:                       // TT? remove bit 7 
            Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on TT, will remove ID 7 from enabled ID bits");
        
            enabledIDbits = enabledIDbits & 0x7F;
            if(sdCardAcsiId == 7) {
                sdCardAcsiId = 0xff;
            }
            break;

        //------------
        case SCSI_MACHINE_FALCON:                   // Falcon? remove bit 0
            Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on Falcon, will remove ID 0 from enabled ID bits");

            enabledIDbits = enabledIDbits & 0xFE;
            if(sdCardAcsiId == 0) {
                sdCardAcsiId = 0xff;
            }
            break;

        //------------
        default:
        case SCSI_MACHINE_UNKNOWN:                  // unknown machine? remove both bits 7 and 0
            Debug::out(LOG_DEBUG, "CCoreThread::getIdBits() -- we're running on unknown machine, will remove ID 7 and ID 0 from enabled ID bits");

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

void CCoreThread::onDevAttached(std::string devName, bool isAtariDrive)
{
	Debug::out(LOG_DEBUG, "CCoreThread::onDevAttached: devName %s", (char *) devName.c_str());

    if(mountRawNotTrans) {                  // attach as raw?
        Debug::out(LOG_DEBUG, "CCoreThread::onDevAttached -- should mount USB media as raw, attaching as RAW");
		scsi->attachToHostPath(devName, SOURCETYPE_DEVICE, SCSI_ACCESSTYPE_FULL);
    } else {                                // attach as translated?
        Debug::out(LOG_DEBUG, "CCoreThread::onDevAttached -- should mount USB media as translated, attaching as TRANSLATED");
        attachDevAsTranslated(devName);
    }
}

void CCoreThread::onDevDetached(std::string devName)
{
	// try to detach the device - works if was attached as RAW, does nothing otherwise
	scsi->dettachFromHostPath(devName);

	// and also try to detach the device from translated disk
	std::pair <std::multimap<std::string, std::string>::iterator, std::multimap<std::string, std::string>::iterator> ret;
	std::multimap<std::string, std::string>::iterator it;

	ret = mapDeviceToHostPaths.equal_range(devName);				// find a range of host paths which are mapped to partitions found on this device

	for (it = ret.first; it != ret.second; ++it) {					// now go through the list of device - host_path pairs and unmount them
		std::string hostPath = it->second;							// retrieve just the host path

		translated->detachFromHostPath(hostPath);					// now try to detach this from translated drives
	}

	mapDeviceToHostPaths.erase(ret.first, ret.second);				// and delete the whole device items from this multimap
}

void CCoreThread::attachDevAsTranslated(std::string devName)
{
	bool res;
	std::list<std::string>				partitions;
	std::list<std::string>::iterator	it;

	devFinder.getDevPartitions(devName, partitions);							// get list of partitions for that device (sda -> sda1, sda2)

	for (it = partitions.begin(); it != partitions.end(); ++it) {				// go through those partitions
		std::string partitionDevice;
		std::string mountPath;
		std::string devPath, justDevName;

		partitionDevice = *it;													// get the current device, which represents single partition (e.g. sda1)

		Utils::splitFilenameFromPath(partitionDevice, devPath, justDevName);	// split path to path and device name (e.g. /dev/sda1 -> /dev + sda1)
		mountPath = "/mnt/" + justDevName;										// create host path (e.g. /mnt/sda1)

		TMounterRequest tmr;
		tmr.action			= MOUNTER_ACTION_MOUNT;								// action: mount
		tmr.deviceNotShared	= true;												// mount as device
		tmr.devicePath		= partitionDevice;									// e.g. /dev/sda2
		tmr.mountDir		= mountPath;										// e.g. /mnt/sda2
		mountAdd(tmr);

		res = translated->attachToHostPath(mountPath, TRANSLATEDTYPE_NORMAL);	// try to attach

		if(!res) {																// if didn't attach, skip the rest
			Debug::out(LOG_ERROR, "attachDevAsTranslated: failed to attach %s", (char *) mountPath.c_str());
			continue;
		}

		mapDeviceToHostPaths.insert(std::pair<std::string, std::string>(devName, mountPath) );	// store it to multimap
	}
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

void CCoreThread::readWebStartupMode(void)
{
    FILE *f = fopen(WEB_PARAMS_FILE, "rt");

    if(!f) {                                            // couldn't open file? nothing to do
        return;
    }

    char tmp[64];
    memset(tmp, 0, 64);

    fgets(tmp, 64, f);

    int ires, logLev;
    ires = sscanf(tmp, "ll%d", &logLev);            // read the param
    fclose(f);

    unlink(WEB_PARAMS_FILE);                        // remove the file so we won't read it again

    if(ires != 1) {                                 // failed to read the new log level? quit
        return;
    }

    if(logLev >= LOG_OFF && logLev <= LOG_DEBUG) {  // param is valid
        if(flags.logLevel == logLev) {              // but logLevel won't change?
            return;                                 // nothing to do
        }

        flags.logLevel = logLev;                    // set new log level
        Debug::out(LOG_INFO, "Log level changed from file %s to level %d", WEB_PARAMS_FILE, logLev);
    } else {                                        // on set wrong log level - switch to lowest log level
        flags.logLevel = LOG_ERROR;
        Debug::out(LOG_INFO, "Log level change invalid, switching to LOG_ERROR");
    }
}

/*
	Is a particular internet interface up, running and has an IP address?
*/
bool CCoreThread::inetIfaceReady(const char*ifrname){
	struct ifreq ifr;
	bool up_and_running;

	//temporary socket
	int socketfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (socketfd == -1){
        return false;
	}
			
	memset( &ifr, 0, sizeof(ifr) );
	strcpy( ifr.ifr_name, ifrname );
	
	if( ioctl( socketfd, SIOCGIFFLAGS, &ifr ) != -1 ) {
	    up_and_running = (ifr.ifr_flags & ( IFF_UP | IFF_RUNNING )) == ( IFF_UP | IFF_RUNNING );
	    
		//it's only ready and usable if it has an IP address
        if( up_and_running && ioctl(socketfd, SIOCGIFADDR, &ifr) != -1 ){
	        if( ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr.s_addr==0 ){
			    up_and_running = false;
			} 
		} else {
		    up_and_running = false;
		}
	    //Debug::out(LOG_DEBUG, "inetIfaceReady ip: %s %s", ifrname, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	} else {
	    up_and_running = false;
	}

	int result=0;
	do {
        result = close(socketfd);
    } while (result == -1 && errno == EINTR);	

	return up_and_running; 
}
