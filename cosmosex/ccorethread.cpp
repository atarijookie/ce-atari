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
#include "settings.h"
#include "gpio.h"
#include "mounter.h"
#include "downloader.h"
#include "update.h"

#define DEV_CHECK_TIME_MS	3000
#define UPDATE_CHECK_TIME   1000

CCoreThread::CCoreThread()
{
    Update::initialize();

    setEnabledIDbits = false;

	gotDevTypeRaw			= false;
	gotDevTypeTranslated	= false;

	conSpi		= new CConSpi();

    dataTrans   = new AcsiDataTrans();
    dataTrans->setCommunicationObject(conSpi);

    scsi        = new Scsi();
    scsi->setAcsiDataTrans(dataTrans);
    scsi->attachToHostPath(TRANSLATEDBOOTMEDIA_FAKEPATH, SOURCETYPE_IMAGE_TRANSLATEDBOOT, SCSI_ACCESSTYPE_FULL);
//    scsi->attachToHostPath("TESTMEDIA", SOURCETYPE_TESTMEDIA, SCSI_ACCESSTYPE_FULL);
	scsi->attachToHostPath("sd_card_icdpro.img", SOURCETYPE_IMAGE, SCSI_ACCESSTYPE_FULL);

    translated = new TranslatedDisk();
    translated->setAcsiDataTrans(dataTrans);

    confStream = new ConfigStream();
    confStream->setAcsiDataTrans(dataTrans);

    // now register all the objects which use some settings in the proxy
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_ACSI);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_TRANSLATED);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this,          SETTINGSUSER_SHARED);
	
    settingsReloadProxy.addSettingsUser((ISettingsUser *) scsi,          SETTINGSUSER_ACSI);
	settingsReloadProxy.addSettingsUser((ISettingsUser *) translated,    SETTINGSUSER_TRANSLATED);
	
	// register this class as receiver of dev attached / detached calls
	devFinder.setDevChangesHandler((DevChangesHandler *) this);
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
//	Utils::resetHansAndFranz();
	
	DWORD nextDevFindTime       = Utils::getCurrentMs();    // create a time when the devices should be checked - and that time is now
    DWORD nextUpdateCheckTime   = Utils::getEndTime(5000);  // create a time when update download status should be checked

	mountAndAttachSharedDrive();					// if shared drive is enabled, try to mount it and attach it
	
    Update::downloadUpdateList();                   // download the list of components with the newest available versions

	bool res;
//---------------------------------------------
    image = imageFactory.getImage((char *) "blank.st");

    if(image) {
        if(image->isOpen()) {
            Debug::out("Encoding image...");
            encImage.encodeAndCacheImage(image, true);
            Debug::out("...done");
        } else {
            Debug::out("Image is not open!");
        }
    } else {
        Debug::out("Image file type not supported!");
    }
//---------------------------------------------
	
#ifdef ONPC
/*
    char atnSendFwVer[16] = {0xca, 0xfe, 0,1, 0, 8, 0, 8, 0xa0, 0x14, 0x02, 0x05, 0, 0, 0, 0};
    bcmSpiAddData(16, atnSendFwVer); 
*/

    char atnSendCmd[38] = {0xca, 0xfe, 0,2, 0, 12, 0, 16, 8, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xca, 0xfe, 0,3, 0,6, 1,0x04, 0,0, 0,0 };
    bcmSpiAddData(38, atnSendCmd); 

#endif

    while(sigintReceived == 0) {
		bool gotAtn = false;						                    // no ATN received yet?
		
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

                // TODO: refresh config screen with download status

                break;

                //-----------
                case UPDATE_STATE_DOWNLOAD_FAIL:

                // TODO: show fail message on config screen

                Update::stateGoIdle();
                break;

                //-----------
                case UPDATE_STATE_DOWNLOAD_OK:

                 // TODO: quit the app and do the update

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
				Debug::out((char *) "CCoreThread received weird ATN code %02x waitForATN()", inBuff[3]);
				break;
			}
		}
		
        // check for any ATN code waiting from Franz
		res = conSpi->waitForATN(SPI_CS_FRANZ, (BYTE) ATN_ANY, 0, inBuff);		
		if(res) {									// FRANZ is signaling attention?
			gotAtn = true;							// we've some ATN

			switch(inBuff[3]) {
			case ATN_FW_VERSION:
				handleFwVersion(SPI_CS_FRANZ);
				break;

            case ATN_SECTOR_WRITTEN:

                break;

			case ATN_SEND_TRACK:
				handleSendTrack();
				break;

			default:
				Debug::out((char *) "CCoreThread received weird ATN code %02x waitForATN()", inBuff[3]);
				break;
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

    BYTE bufOut[CMD_SIZE], bufIn[CMD_SIZE];
    memset(bufOut, 0, CMD_SIZE);

    conSpi->txRx(SPI_CS_HANS, 14, bufOut, bufIn);        // get 14 cmd bytes
    Debug::out("handleAcsiCommand: %02x %02x %02x %02x %02x %02x", bufIn[0], bufIn[1], bufIn[2], bufIn[3], bufIn[4], bufIn[5]);

    BYTE justCmd = bufIn[0] & 0x1f;
    BYTE wasHandled = false;

    BYTE acsiId = bufIn[0] >> 5;                        // get just ACSI ID

    if(acsiIDevType[acsiId] == DEVTYPE_OFF) {          	// if this ACSI ID is off, reply with error and quit
        dataTrans->setStatus(SCSI_ST_CHECK_CONDITION);
        dataTrans->sendDataAndStatus();
        return;
    }

    // ok, so the ID is right, let's see what we can do
    if(justCmd == 0) {                              // if the command is 0 (TEST UNIT READY)
        if(bufIn[1] == 'C' && bufIn[2] == 'E') {    // and this is CosmosEx specific command

            switch(bufIn[3]) {
            case HOSTMOD_CONFIG:                    // config console command?
                wasHandled = true;
                confStream->processCommand(bufIn);
                break;

            case HOSTMOD_TRANSLATED_DISK:
                wasHandled = true;
                translated->processCommand(bufIn);
                break;
            }
        }
    } else if(justCmd == 0x1f) {                    // if the command is ICD mark
        BYTE justCmd2 = bufIn[1] & 0x1f;

        if(justCmd2 == 0 && bufIn[2] == 'C' && bufIn[3] == 'E') {    // the command is 0 (TEST UNIT READY), and this is CosmosEx specific command

            switch(bufIn[4]) {
            case HOSTMOD_TRANSLATED_DISK:
                wasHandled = true;
                translated->processCommand(bufIn + 1);
                break;
            }
        }
    }

    if(wasHandled != true) {                        // if the command was not previously handled, it's probably just some SCSI command
        scsi->processCommand(bufIn);                // process the command
    }
}

void CCoreThread::reloadSettings(void)
{
	// first dettach all the devices
	scsi->detachAll();
    translated->detachAll();

	// then load the new settings
    loadSettings();

	// and now try to attach everything back
	devFinder.clearMap();									// make all the devices appear as new
	devFinder.lookForDevChanges();							// and now find all the devices
	
	mountAndAttachSharedDrive();							// and also attach shared drive
}

void CCoreThread::loadSettings(void)
{
    Debug::out("CCoreThread::loadSettings");

    Settings s;
    enabledIDbits = 0;									// no bits / IDs enabled yet

	gotDevTypeRaw			= false;					// no raw and translated types found yet
	gotDevTypeTranslated	= false;
	
    char key[32];
    for(int id=0; id<8; id++) {							// read the list of device types from settings
        sprintf(key, "ACSI_DEVTYPE_%d", id);			// create settings KEY, e.g. ACSI_DEVTYPE_0
        
		int devType = s.getInt(key, DEVTYPE_OFF);

        if(devType < 0) {
            devType = DEVTYPE_OFF;
        }

        acsiIDevType[id] = devType;

        if(devType != DEVTYPE_OFF) {                    // if ON
            enabledIDbits |= (1 << id);                 // set the bit to 1
        }
		
		if(devType == DEVTYPE_RAW) {					// found at least one RAW device?
			gotDevTypeRaw = true;
		}
		
		if(devType == DEVTYPE_TRANSLATED) {				// found at least one TRANSLATED device?
			gotDevTypeTranslated = true;
		}
    }

	// no ACSI ID was enabled? enable ACSI ID 0
	if(!gotDevTypeRaw && !gotDevTypeTranslated) {
		Debug::out("CCoreThread::loadSettings -- no ACSI ID was enabled, so enabling ACSI ID 0");
			
		acsiIDevType[0]	= DEVTYPE_TRANSLATED;
		enabledIDbits	= 1;
		
		gotDevTypeTranslated = true;
	}
	
    setEnabledIDbits = true;
}

void CCoreThread::handleFwVersion(int whichSpiCs)
{
    BYTE fwVer[10], oBuf[10];

    memset(oBuf, 0, 10);                        // first clear the output buffer

    if(whichSpiCs == SPI_CS_HANS) {             // it's Hans?
        if(setEnabledIDbits) {                  // if we should send ACSI ID configuration
            oBuf[3] = CMD_ACSI_CONFIG;          // CMD: send acsi config
            oBuf[4] = enabledIDbits;            // store ACSI enabled IDs
            setEnabledIDbits = false;           // and don't sent this anymore (until needed)
        }
    } else {                                    // it's Franz?

    }

    conSpi->txRx(whichSpiCs, 8, oBuf, fwVer);

    int year = bcdToInt(fwVer[1]) + 2000;
    if(fwVer[0] == 0xf0) {
        Update::versions.current.franz.fromInts(year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));              // store found FW version of Franz

        Debug::out("FW: Franz, %d-%02d-%02d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));
    } else {
        Update::versions.current.hans.fromInts(year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));               // store found FW version of Hans

        int currentLed = fwVer[4];
        Debug::out("FW: Hans,  %d-%02d-%02d, LED is: %d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]), currentLed);
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
	Debug::out("CCoreThread::onDevAttached: devName %s", (char *) devName.c_str());

	// TODO: add logic to detach the device, if it was attached as other type (raw / tran)

	// if have RAW enabled, but not TRANSLATED - attach all drives (atari and non-atari) as RAW
	if(gotDevTypeRaw && !gotDevTypeTranslated) {
		scsi->attachToHostPath(devName, SOURCETYPE_DEVICE, SCSI_ACCESSTYPE_FULL);
	}

	// if have TRANSLATED enabled, but not RAW - can't attach atari drives, but do attach non-atari drives as TRANSLATED
	if(!gotDevTypeRaw && gotDevTypeTranslated) {
		if(!isAtariDrive) {			// attach non-atari drive as TRANSLATED	
			attachDevAsTranslated(devName);
		} else {					// can't attach atari drive
			Debug::out("Can't attach device %s, because it's an Atari drive and no RAW device is enabled.", (char *) devName.c_str());
		}
	}

	// if both TRANSLATED and RAW are enabled - attach non-atari as TRANSLATED, and atari as RAW
	if(gotDevTypeRaw && gotDevTypeTranslated) {
		if(isAtariDrive) {			// attach atari drive as RAW
			scsi->attachToHostPath(devName, SOURCETYPE_DEVICE, SCSI_ACCESSTYPE_FULL);
		} else {					// attach non-atari drive as TRANSLATED
			attachDevAsTranslated(devName);
		}
	}
	
	// if no device type is enabled
	if(!gotDevTypeRaw && !gotDevTypeTranslated) {
		Debug::out("Can't attach device %s, because no device type (RAW or TRANSLATED) is enabled on ACSI bus!", (char *) devName.c_str());
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
	
	if(!gotDevTypeTranslated) {													// don't have any translated device on acsi bus, don't attach
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
			Debug::out("attachDevAsTranslated: failed to attach %s", (char *) mountPath.c_str());
			continue;
		}
		
		mapDeviceToHostPaths.insert(std::pair<std::string, std::string>(devName, mountPath) );	// store it to multimap
	}
}

void CCoreThread::mountAndAttachSharedDrive(void)
{
	std::string mountPath = "/mnt/shared";

    Settings s;
    std::string addr, path;
	bool sharedEnabled;
	bool nfsNotSamba;

    addr			= s.getString((char *) "SHARED_ADDRESS",  (char *) "");
    path			= s.getString((char *) "SHARED_PATH",     (char *) "");
	
	sharedEnabled	= s.getBool((char *) "SHARED_ENABLED", false);
	nfsNotSamba		= s.getBool((char *) "SHARED_NFS_NOT_SAMBA", false);
	
	if(!sharedEnabled) {
		Debug::out("mountAndAttachSharedDrive: shared drive not enabled, not mounting and not attaching...");
		return;
	}
	
	if(addr.empty() || path.empty()) {
		Debug::out("mountAndAttachSharedDrive: address or path is empty, this won't work!");
		return;
	}
	
	TMounterRequest tmr;													// fill this struct to mount something somewhere
	tmr.action			= MOUNTER_ACTION_MOUNT;								// action: mount
	tmr.deviceNotShared		= false;
	tmr.shared.host			= addr;
	tmr.shared.hostDir		= path;
	tmr.shared.nfsNotSamba	= nfsNotSamba;
	tmr.mountDir			= mountPath;
	mountAdd(tmr);

	bool res = translated->attachToHostPath(mountPath, TRANSLATEDTYPE_SHAREDDRIVE);	// try to attach

	if(!res) {																// if didn't attach, skip the rest
		Debug::out("mountAndAttachSharedDrive: failed to attach shared drive %s", (char *) mountPath.c_str());
	}
}

void CCoreThread::handleSendTrack(void)
{
    BYTE oBuf[2], iBuf[15000];

    memset(oBuf, 0, 2);
    conSpi->txRx(SPI_CS_FRANZ, 2, oBuf, iBuf);

    int side    = iBuf[0];               // now read the current floppy position
    int track   = iBuf[1];

    Debug::out("ATN_SEND_TRACK -- track %d, side %d", track, side);

    int tr, si, spt;
    image->getParams(tr, si, spt);      // read the floppy image params

    if(side < 0 || side > 1 || track < 0 || track >= tr) {
        Debug::out("Side / Track out of range!");
        return;
    }

    BYTE *encodedTrack;
    int countInTrack;

    encodedTrack = encImage.getEncodedTrack(track, side, countInTrack);

    int remaining   = 15000 - (4*2) - 2;		// this much bytes remain to send after the received ATN

    conSpi->txRx(SPI_CS_FRANZ, remaining, encodedTrack, iBuf);
}



