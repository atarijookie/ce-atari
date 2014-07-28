#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
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

#define DEV_CHECK_TIME_MS	3000
#define UPDATE_CHECK_TIME   1000

extern bool g_noReset;                      // don't reset Hans and Franz on start - used with STM32 ST-Link JTAG

bool    g_gotHansFwVersion;
bool    g_gotFranzFwVersion;

CCoreThread::CCoreThread()
{
    Update::initialize();

    setEnabledIDbits        = false;
    setEnabledFloppyImgs    = false;

    lastFloppyImageLed      = -1;

	conSpi		= new CConSpi();

    dataTrans   = new AcsiDataTrans();
    dataTrans->setCommunicationObject(conSpi);

    scsi        = new Scsi();
    scsi->setAcsiDataTrans(dataTrans);
    scsi->attachToHostPath(TRANSLATEDBOOTMEDIA_FAKEPATH, SOURCETYPE_IMAGE_TRANSLATEDBOOT, SCSI_ACCESSTYPE_FULL);
//  scsi->attachToHostPath("TESTMEDIA", SOURCETYPE_TESTMEDIA, SCSI_ACCESSTYPE_FULL);

    translated = new TranslatedDisk();
    translated->setAcsiDataTrans(dataTrans);

    confStream = new ConfigStream();
    confStream->setAcsiDataTrans(dataTrans);
	confStream->setSettingsReloadProxy(&settingsReloadProxy);

    // now register all the objects which use some settings in the proxy
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_ACSI);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_FLOPPYIMGS);
	
    settingsReloadProxy.addSettingsUser((ISettingsUser *) scsi,          SETTINGSUSER_ACSI);

	settingsReloadProxy.addSettingsUser((ISettingsUser *) translated,    SETTINGSUSER_TRANSLATED);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) translated,    SETTINGSUSER_SHARED);
	
	// register this class as receiver of dev attached / detached calls
	devFinder.setDevChangesHandler((DevChangesHandler *) this);

    // give floppy setup everything it needs
    floppySetup.setAcsiDataTrans(dataTrans);
    floppySetup.setImageSilo(&floppyImageSilo);
    floppySetup.setTranslatedDisk(translated);

    // the floppy image silo might change settings (when images are changes), add settings reload proxy
    floppyImageSilo.setSettingsReloadProxy(&settingsReloadProxy);
    settingsReloadProxy.reloadSettings(SETTINGSUSER_FLOPPYIMGS);            // mark that floppy settings changed (when imageSilo loaded the settings)
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
    
    Debug::out(LOG_DEBUG, "Will check for Hans and Franz alive: %s", (shouldCheckHansFranzAlive ? "yes" : "no") );
    //------------------------------
	
	DWORD nextDevFindTime       = Utils::getCurrentMs();                // create a time when the devices should be checked - and that time is now
    DWORD nextUpdateCheckTime   = Utils::getEndTime(5000);              // create a time when update download status should be checked

    Update::downloadUpdateList();                                       // download the list of components with the newest available versions

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

#ifndef ONPC		
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
		if(!gotAtn) {								// no ATN was processed?
			Utils::sleepMs(1);						// wait 1 ms...
		}		
    }
}

void CCoreThread::handleAcsiCommand(void)
{
    #define CMD_SIZE    14

    BYTE bufOut[CMD_SIZE];
    memset(bufOut, 0, CMD_SIZE);

    BYTE bufIn[CMD_SIZE];

    conSpi->txRx(SPI_CS_HANS, 14, bufOut, bufIn);        // get 14 cmd bytes

    Debug::out(LOG_DEBUG, "handleAcsiCommand: %02x %02x %02x %02x %02x %02x", bufIn[0], bufIn[1], bufIn[2], bufIn[3], bufIn[4], bufIn[5]);

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

    // ok, so the ID is right, let's see what we can do
    if(justCmd == 0 && tag1 == 'C' && tag2 == 'E') {    // if the command is 0 (TEST UNIT READY) and there's this CE tag
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
	
    setEnabledIDbits = true;
}

void CCoreThread::handleFwVersion(int whichSpiCs)
{
    BYTE fwVer[14], oBuf[14];
	int cmdLength;

    memset(oBuf, 0, 14);                                // first clear the output buffer

    // WORD sent (bytes shown): 01 23 45 67 

    if(whichSpiCs == SPI_CS_HANS) {                     // it's Hans?
		cmdLength = 12;
	
        if(setEnabledIDbits) {                          // if we should send ACSI ID configuration
            oBuf[1] = CMD_ACSI_CONFIG;                  // CMD: send acsi config  (bytes 0 & 1)
            oBuf[2] = acsiIdInfo.enabledIDbits;         // store ACSI enabled IDs 
            oBuf[3] = acsiIdInfo.sdCardAcsiId;          // store which ACSI ID is used for SD card
            setEnabledIDbits = false;                   // and don't sent this anymore (until needed)
        }

        if(setEnabledFloppyImgs) {
            oBuf[5] = CMD_FLOPPY_CONFIG;                // CMD: send which floppy images are enabled (bytes 4 & 5)
            oBuf[6] = floppyImageSilo.getSlotBitmap();  // store which floppy images are enabled
            setEnabledFloppyImgs = false;               // and don't sent this anymore (until needed)
        }
    } else {                                    		// it's Franz?
		cmdLength = 8;
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
        }
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

	// TODO: add logic to detach the device, if it was attached as other type (raw / tran)

	// if have RAW enabled, but not TRANSLATED - attach all drives (atari and non-atari) as RAW
	if(acsiIdInfo.gotDevTypeRaw && !acsiIdInfo.gotDevTypeTranslated) {
		scsi->attachToHostPath(devName, SOURCETYPE_DEVICE, SCSI_ACCESSTYPE_FULL);
	}

	// if have TRANSLATED enabled, but not RAW - can't attach atari drives, but do attach non-atari drives as TRANSLATED
	if(!acsiIdInfo.gotDevTypeRaw && acsiIdInfo.gotDevTypeTranslated) {
		if(!isAtariDrive) {			// attach non-atari drive as TRANSLATED	
			attachDevAsTranslated(devName);
		} else {					// can't attach atari drive
			Debug::out(LOG_INFO, "Can't attach device %s, because it's an Atari drive and no RAW device is enabled.", (char *) devName.c_str());
		}
	}

	// if both TRANSLATED and RAW are enabled - attach non-atari as TRANSLATED, and atari as RAW
	if(acsiIdInfo.gotDevTypeRaw && acsiIdInfo.gotDevTypeTranslated) {
		if(isAtariDrive) {			// attach atari drive as RAW
			scsi->attachToHostPath(devName, SOURCETYPE_DEVICE, SCSI_ACCESSTYPE_FULL);
		} else {					// attach non-atari drive as TRANSLATED
			attachDevAsTranslated(devName);
		}
	}
	
	// if no device type is enabled
	if(!acsiIdInfo.gotDevTypeRaw && !acsiIdInfo.gotDevTypeTranslated) {
		Debug::out(LOG_INFO, "Can't attach device %s, because no device type (RAW or TRANSLATED) is enabled on ACSI bus!", (char *) devName.c_str());
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
	
	if(!acsiIdInfo.gotDevTypeTranslated) {										// don't have any translated device on acsi bus, don't attach
		return;
	}
	
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

