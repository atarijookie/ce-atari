#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>

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

#define DEV_CHECK_TIME_MS	3000
#define UPDATE_CHECK_TIME   1000

#define WEB_PARAMS_CHECK_TIME_MS    3000
#define WEB_PARAMS_FILE             "/tmp/ce_startupmode"

extern bool g_noReset;                      // don't reset Hans and Franz on start - used with STM32 ST-Link JTAG
extern BYTE g_logLevel;                     // current log level

bool    g_gotHansFwVersion;
bool    g_gotFranzFwVersion;

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

    dataTrans   = new AcsiDataTrans();
    dataTrans->setCommunicationObject(conSpi);

    scsi        = new Scsi();
    scsi->setAcsiDataTrans(dataTrans);
    scsi->attachToHostPath(TRANSLATEDBOOTMEDIA_FAKEPATH, SOURCETYPE_IMAGE_TRANSLATEDBOOT, SCSI_ACCESSTYPE_FULL);
//  scsi->attachToHostPath("TESTMEDIA", SOURCETYPE_TESTMEDIA, SCSI_ACCESSTYPE_FULL);

    translated = new TranslatedDisk(dataTrans,configService,screencastService);

    confStream = new ConfigStream();
    confStream->setAcsiDataTrans(dataTrans);
	confStream->setSettingsReloadProxy(&settingsReloadProxy);

    // now register all the objects which use some settings in the proxy
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_ACSI);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_FLOPPYIMGS);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_FLOPPYCONF);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_FLOPPY_SLOT);
	settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_TRANSLATED);
	
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
    floppyService->setImageSilo(&floppyImageSilo);
    floppyService->setCoreThread(this);
}

CCoreThread::~CCoreThread()
{
    delete conSpi;
    delete dataTrans;
    delete scsi;

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

    g_gotHansFwVersion  = false;
    g_gotFranzFwVersion = false;

    if(g_noReset) {                                                     // if we're debugging Hans or Franz (noReset is set to true), don't do this alive check
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

	DWORD nextDevFindTime       = Utils::getCurrentMs();                // create a time when the devices should be checked - and that time is now
    DWORD nextUpdateCheckTime   = Utils::getEndTime(5000);              // create a time when update download status should be checked
    DWORD nextWebParsCheckTime  = Utils::getEndTime(WEB_PARAMS_CHECK_TIME_MS);  // create a time when we should check for new params from the web page

    Update::downloadUpdateList(NULL);                                   // download the list of components with the newest available versions

	bool res;

    while(sigintReceived == 0) {
		bool gotAtn = false;						                    // no ATN received yet?

        // should we check if Hans and Franz are alive?
        if(shouldCheckHansFranzAlive) {
            if(Utils::getCurrentMs() >= hansFranzAliveCheckTime) {      // did enough time pass since the Hans and Franz reset?
                if(!g_gotHansFwVersion || !g_gotFranzFwVersion) {       // if don't have version from Hans or Franz, then they're not alive
                    Update::createFlashFirstFwScript();

                    Debug::out(LOG_INFO, "No answer from Hans or Franz, so first firmware flash script created, will do first firmware flashing.");
					sigintReceived = 1;
                } else {
                    Debug::out(LOG_INFO, "Got answers from both Hans and Franz, so not doing first firmware flashing.");
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
					Debug::out(LOG_INFO, "Update state - download OK, but user is not on download page - NOT doing update");
                } else {                                                    // if user is waiting on download page, aplly update
                    res = Update::createUpdateScript();

                    if(!res) {
                        confStream->showUpdateError();
						Debug::out(LOG_INFO, "Update state - download OK, failed to create update script - NOT doing update");
                    } else {
						Debug::out(LOG_INFO, "Update state - download OK, update script created, will do update.");
						sigintReceived = 1;
                    }
                }

                break;
            }
        }

#if !defined(ONPC_HIGHLEVEL)
        // check for any ATN code waiting from Hans
		res = conSpi->waitForATN(SPI_CS_HANS, (BYTE) ATN_ANY, 0, inBuff);

		if(res) {									    // HANS is signaling attention?
			gotAtn = true;							    // we've some ATN

			switch(inBuff[3]) {
			case ATN_FW_VERSION:
				handleFwVersion(SPI_CS_HANS);
				break;

			case ATN_ACSI_COMMAND:
				handleAcsiCommand();
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
		res = conSpi->waitForATN(SPI_CS_FRANZ, (BYTE) ATN_ANY, 0, inBuff);
		if(res) {									// FRANZ is signaling attention?
			gotAtn = true;							// we've some ATN

			switch(inBuff[3]) {
			case ATN_FW_VERSION:                    // device has sent FW version
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
    g_gotFranzFwVersion = true;
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
    #define CMD_SIZE    14

    BYTE bufOut[CMD_SIZE];
    BYTE bufIn[CMD_SIZE];

    memset(bufOut,  0, CMD_SIZE);
    memset(bufIn,   0, CMD_SIZE);

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
        }
    }

    if(wasHandled != true) {                            // if the command was not previously handled, it's probably just some SCSI command
        scsi->processCommand(bufIn);                    // process the command
    }
}

void CCoreThread::reloadSettings(int type)
{
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
                responseAddWord(oBuf, CMD_ACSI_CONFIG);             // CMD: send acsi config
                responseAddWord(oBuf, MAKEWORD(acsiIdInfo.enabledIDbits, acsiIdInfo.sdCardAcsiId)); // store ACSI enabled IDs and which ACSI ID is used for SD card
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
        g_gotFranzFwVersion = true;

        Debug::out(LOG_DEBUG, "FW: Franz, %d-%02d-%02d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));
    } else {
        Update::versions.current.hans.fromInts(year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));               // store found FW version of Hans
        g_gotHansFwVersion = true;

        int currentLed = fwVer[4];

        Debug::out(LOG_DEBUG, "FW: Hans,  %d-%02d-%02d, LED is: %d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]), currentLed);

        if(lastFloppyImageLed != currentLed) {              // did the floppy image LED change since last time?
            lastFloppyImageLed = currentLed;

            Debug::out(LOG_INFO, "Floppy image changed to %d", currentLed);

            floppyImageSilo.setCurrentSlot(currentLed);     // switch the floppy image

            diskChanged     = true;                         // also tell Franz that floppy changed
            setDiskChanged  = true;
        }
    }
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
	Debug::out(LOG_INFO, "CCoreThread::onDevAttached: devName %s", (char *) devName.c_str());

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

    int side    = iBuf[0];               // now read the current floppy position
    int track   = iBuf[1];

    Debug::out(LOG_DEBUG, "ATN_SEND_TRACK -- track %d, side %d", track, side);

    int tr, si, spt;
    floppyImageSilo.getParams(tr, si, spt);      // read the floppy image params

    if(side < 0 || side > 1 || track < 0 || track >= tr) {
        Debug::out(LOG_ERROR, "Side / Track out of range!");
        return;
    }

    BYTE *encodedTrack;
    int countInTrack;

	encodedTrack = floppyImageSilo.getEncodedTrack(track, side, countInTrack);

    int remaining   = 15000 - (4*2) - 2;		// this much bytes remain to send after the received ATN

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
        if(g_logLevel == logLev) {                  // but logLevel won't change?
            return;                                 // nothing to do
        }

        g_logLevel = logLev;                        // set new log level
        Debug::out(LOG_ERROR, "Log level changed from file %s to level %d", WEB_PARAMS_FILE, logLev);
    } else {                                        // on set wrong log level - switch to lowest log level
        g_logLevel = LOG_ERROR;
        Debug::out(LOG_ERROR, "Log level change invalid, switching to LOG_ERROR");
    }
}


