#include <algorithm>
#include <string>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include "../global.h"
#include "../debug.h"
#include "../settings.h"
#include "../utils.h"
#include "../mounter.h"
#include "acsidatatrans.h"
#include "acsicommand/screencastacsicommand.h"
#include "acsicommand/dateacsicommand.h"
#include "settingsreloadproxy.h"
#include "translateddisk.h"
#include "translatedhelper.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "desktopcreator.h"

extern THwConfig hwConfig;
extern InterProcessEvents events;

TranslatedDisk::TranslatedDisk(AcsiDataTrans *dt, ConfigService *cs, ScreencastService *scs)
{
    dataTrans = dt;
    configService = cs;

    reloadProxy = NULL;
    
    dataBuffer  = new BYTE[ACSI_BUFFER_SIZE];
    dataBuffer2 = new BYTE[ACSI_BUFFER_SIZE];

    pexecImage  = new BYTE[PEXEC_DRIVE_SIZE_BYTES];
    memset(pexecImage, 0, PEXEC_DRIVE_SIZE_BYTES);
    
    pexecImageReadFlags = new BYTE[PEXEC_DRIVE_SIZE_SECTORS];
    memset(pexecImageReadFlags, 0, PEXEC_DRIVE_SIZE_SECTORS);

    prgSectorStart  = 0;
    prgSectorEnd    = PEXEC_DRIVE_SIZE_SECTORS;
    pexecDriveIndex = -1;
    
    detachAll();

    for(int i=0; i<MAX_FILES; i++) {        // initialize host file structures
        files[i].hostHandle     = NULL;
        files[i].atariHandle    = EIHNDL;
        files[i].hostPath       = "";
    }

    initFindStorages();

    loadSettings();

    mountAndAttachSharedDrive();					                    // if shared drive is enabled, try to mount it and attach it
    attachConfigDrive();                                                // if config drive is enabled, attach it

    //ACSI command "date"
    dateAcsiCommand         = new DateAcsiCommand(dataTrans,configService);

    //ACSI commands "screencast"
	screencastAcsiCommand   = new ScreencastAcsiCommand(dataTrans,scs);

    initAsciiTranslationTable();
 
    for(int i=0; i<MAX_ZIP_DIRS; i++) {                 // init the ZIP dirs
        zipDirs[i] = new ZipDirEntry(i);
    }
}

TranslatedDisk::~TranslatedDisk()
{
    delete dateAcsiCommand;
    delete screencastAcsiCommand;

    delete []dataBuffer;
    delete []dataBuffer2;

    delete []pexecImage;
    delete []pexecImageReadFlags;
    
    destroyFindStorages();
    
    for(int i=0; i<MAX_ZIP_DIRS; i++) {                 // init the ZIP dirs
        delete zipDirs[i];
    }
}

void TranslatedDisk::setSettingsReloadProxy(SettingsReloadProxy *rp)
{
    reloadProxy = rp;
}

void TranslatedDisk::loadSettings(void)
{
    Debug::out(LOG_DEBUG, "TranslatedDisk::loadSettings");

    Settings s;
    char drive1, drive2, drive3;

    drive1 = s.getChar("DRIVELETTER_FIRST",      -1);
    drive2 = s.getChar("DRIVELETTER_SHARED",     -1);
    drive3 = s.getChar("DRIVELETTER_CONFDRIVE",  'O');

    driveLetters.firstTranslated    = drive1 - 'A';
    driveLetters.shared             = drive2 - 'A';
    driveLetters.confDrive          = drive3 - 'A';

    // now set the read only drive flags
    driveLetters.readOnly = 0;

    if(driveLetters.confDrive >= 0 && driveLetters.confDrive <=15) {        // if got a valid drive letter for config drive
        driveLetters.readOnly = (1 << driveLetters.confDrive);              // make config drive read only
    }
    
    useZipdirNotFile = s.getBool("USE_ZIP_DIR", 1);
}

void TranslatedDisk::reloadSettings(int type)
{
    Debug::out(LOG_DEBUG, "TranslatedDisk::reloadSettings");

    // first load the settings
    loadSettings();

    // now move the attached drives around to match the new configuration

    TranslatedConfTemp tmpConf[MAX_DRIVES];
    for(int i=2; i<MAX_DRIVES; i++) {           // first make the copy of the current state
        tmpConf[i].enabled          = conf[i].enabled;
        tmpConf[i].hostRootPath     = conf[i].hostRootPath;
        tmpConf[i].translatedType   = conf[i].translatedType;
        tmpConf[i].devicePath       = conf[i].devicePath;
        tmpConf[i].label            = conf[i].label;
    }

    detachAll();                                // then deinit the conf structures

    int good = 0, bad = 0;
    bool res;

    for(int i=2; i<MAX_DRIVES; i++) {           // and now find new places
        if(!tmpConf[i].enabled) {       // skip the not used positions
            continue;
        }

        res = attachToHostPath(tmpConf[i].hostRootPath, tmpConf[i].translatedType, tmpConf[i].devicePath);   // now attach back

        if(res) {
            good++;
        } else {
            bad++;
        }
    }

    // attach shared and config disk if they weren't attached before and now should be
    mountAndAttachSharedDrive();                            // if shared drive is enabled, try to mount it and attach it
    attachConfigDrive();                                    // if config drive is enabled, attach it

    // todo: attach remainig DOS drives when they couldn't be attached before (not enough letters before)

    Debug::out(LOG_DEBUG, "TranslatedDisk::configChanged_reload -- attached again, good %d, bad %d", good, bad);
}

void TranslatedDisk::mountAndAttachSharedDrive(void)
{
	std::string mountPath = SHARED_DRIVE_PATH;

    Settings s;
    std::string addr, path, username, password;
	bool sharedEnabled;
	bool nfsNotSamba;

    addr			= s.getString("SHARED_ADDRESS",  "");
    path			= s.getString("SHARED_PATH",     "");

    username		= s.getString("SHARED_USERNAME", "");
    password		= s.getString("SHARED_PASSWORD", "");

	sharedEnabled	= s.getBool("SHARED_ENABLED", false);
	nfsNotSamba		= s.getBool("SHARED_NFS_NOT_SAMBA", false);

	if(!sharedEnabled) {
		Debug::out(LOG_DEBUG, "mountAndAttachSharedDrive: shared drive not enabled, not mounting and not attaching...");
		return;
	}

	if(addr.empty() || path.empty()) {
		Debug::out(LOG_ERROR, "mountAndAttachSharedDrive: address or path is empty, this won't work!");
		return;
	}

	TMounterRequest tmr;													// fill this struct to mount something somewhere
	tmr.action              = MOUNTER_ACTION_MOUNT;							// action: mount
	tmr.deviceNotShared		= false;
	tmr.shared.host			= addr;
	tmr.shared.hostDir		= path;
	tmr.shared.nfsNotSamba	= nfsNotSamba;
    tmr.shared.username     = username;
    tmr.shared.password     = password;
	tmr.mountDir			= mountPath;
	Mounter::add(tmr);

    std::string devicePath;
    if(nfsNotSamba) {
        devicePath = std::string("NFS: ") + addr + std::string(":");

        if(path.length() > 0 && (path[0] != '/' && path[0] != '\\')) {      // if the path doesn't end with slash, add it
            devicePath += "/";
        }
        
        devicePath += path;                                                 // now add the path
        std::replace( devicePath.begin(), devicePath.end(), '\\', '/');
    } else {
        devicePath = std::string("samba: \\\\") + addr;

        if(path.length() > 0 && (path[0] != '/' && path[0] != '\\')) {      // if the path doesn't end with slash, add it
            devicePath += "\\";
        }

        devicePath += path;
        std::replace(devicePath.begin(), devicePath.end(), '/', '\\');
    }
    
	bool res = attachToHostPath(mountPath, TRANSLATEDTYPE_SHAREDDRIVE, devicePath);	// try to attach

	if(!res) {																// if didn't attach, skip the rest
		Debug::out(LOG_ERROR, "mountAndAttachSharedDrive: failed to attach shared drive %s", mountPath.c_str());
	}
}

void TranslatedDisk::attachConfigDrive(void)
{
    std::string configDrivePath = CONFIG_DRIVE_PATH;
	bool res = attachToHostPath(configDrivePath, TRANSLATEDTYPE_CONFIGDRIVE, configDrivePath);   // try to attach

	if(!res) {																                // if didn't attach, skip the rest
		Debug::out(LOG_ERROR, "attachConfigDrive: failed to attach config drive %s", configDrivePath.c_str());
	}
}

bool TranslatedDisk::attachToHostPath(std::string hostRootPath, int translatedType, std::string devicePath)
{
    int index = -1;

    if(isAlreadyAttached(hostRootPath)) {                   // if already attached, return success
        Debug::out(LOG_DEBUG, "TranslatedDisk::attachToHostPath - already attached");
        return true;
    }

    // are we attaching shared drive?
    if(translatedType == TRANSLATEDTYPE_SHAREDDRIVE) {
        if(driveLetters.shared > 0) {                       // we have shared drive letter defined
            attachToHostPathByIndex(driveLetters.shared, hostRootPath, translatedType, devicePath);

            return true;
        } else {
            return false;
        }
    }

    // are we attaching config drive?
    if(translatedType == TRANSLATEDTYPE_CONFIGDRIVE) {
        if(driveLetters.confDrive > 0) {              // we have config drive letter defined
            attachToHostPathByIndex(driveLetters.confDrive, hostRootPath, translatedType, devicePath);

            return true;
        } else {
            return false;
        }
    }

    // ok, so we're attaching normal drive
    int start = driveLetters.firstTranslated;

    if(start < 0) {                                     // no first normal drive defined? fail
        return false;
    }

    for(int i=start; i<MAX_DRIVES; i++) {               // find the empty slot for the new drive
        // if this letter is reserved to shared drive
        if(i == driveLetters.shared) {
            continue;
        }

        // if this letter is reserved to config drive
        if(i == driveLetters.confDrive) {
            continue;
        }

        if(!conf[i].enabled) {              // not used yet?
            index = i;                      // found one!
            break;
        }
    }

    if(index == -1) {                       // no empty slot?
        return false;
    }

    attachToHostPathByIndex(index, hostRootPath, translatedType, devicePath);
    return true;
}

void TranslatedDisk::attachToHostPathByIndex(int index, std::string hostRootPath, int translatedType, std::string devicePath)
{
    if(index < 0 || index > MAX_DRIVES) {
        return;
    }

	conf[index].dirTranslator.clear();
    conf[index].enabled             = true;
    conf[index].devicePath          = devicePath;
    conf[index].hostRootPath        = hostRootPath;
    conf[index].currentAtariPath    = HOSTPATH_SEPAR_STRING;
    conf[index].translatedType      = translatedType;
    conf[index].mediaChanged        = true;
    conf[index].label = Utils::getDeviceLabel(devicePath);

    Debug::out(LOG_DEBUG, "TranslatedDisk::attachToHostPath - path %s attached to index %d (letter %c)", hostRootPath.c_str(), index, 'A' + index);
}

bool TranslatedDisk::isAlreadyAttached(std::string hostRootPath)
{
    for(int i=0; i<MAX_DRIVES; i++) {                   // see if the specified path is already attached
        if(!conf[i].enabled) {                          // not used yet? skip
            continue;
        }

        if(conf[i].hostRootPath == hostRootPath) {      // found the matching path?
            return true;
        }
    }

    return false;
}

void TranslatedDisk::detachByIndex(int index)
{
    if(index < 0 || index > MAX_DRIVES) {
        return;
    }

    conf[index].enabled             = false;
    conf[index].stDriveLetter       = 'A' + index;
    conf[index].currentAtariPath    = HOSTPATH_SEPAR_STRING;
    conf[index].translatedType      = TRANSLATEDTYPE_NORMAL;
    conf[index].mediaChanged        = true;
	conf[index].label.clear();
	conf[index].dirTranslator.clear();
}

void TranslatedDisk::detachAll(void)
{
    for(int i=0; i<MAX_DRIVES; i++) {               // initialize the config structs
        detachByIndex(i);
    }

    currentDriveLetter  = 'C';
    currentDriveIndex   = 0;
}

void TranslatedDisk::detachAllUsbMedia(void)
{
    for(int i=0; i<MAX_DRIVES; i++) {               // go through the drives
        // if it's shared drive, or it's a config drive, don't detach
        if(conf[i].translatedType == TRANSLATEDTYPE_SHAREDDRIVE || conf[i].translatedType == TRANSLATEDTYPE_CONFIGDRIVE) {
            continue;
        }
        
        detachByIndex(i);       // it's normal drive, detach
    }

    currentDriveLetter  = 'C';
    currentDriveIndex   = 0;
}

void TranslatedDisk::detachFromHostPath(std::string hostRootPath)
{
    int index = -1;

    for(int i=2; i<MAX_DRIVES; i++) {                   // find where the storage the existing empty slot for the new drive
        if(!conf[i].enabled) {                          // skip disabled drives
            continue;
        }

        if(conf[i].hostRootPath == hostRootPath) {      // the host root path matches?
            index = i;
            break;
        }
    }

    if(index == -1) {                                   // no empty slot?
        return;
    }

	detachByIndex(index);

    // close all files which might be open on this host path
    for(int i=0; i<MAX_FILES; i++) {
        if(startsWith(files[i].hostPath, hostRootPath)) {       // the host path starts with this detached path
            closeFileByIndex(i);
        }
    }
}

void TranslatedDisk::processCommand(BYTE *cmd)
{
    if(dataTrans == 0 ) {
        Debug::out(LOG_ERROR, "processCommand was called without valid dataTrans!");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    if(cmd[1] != 'C' || cmd[2] != 'E' || cmd[3] != HOSTMOD_TRANSLATED_DISK) {   // not for us?
        return;
    }

    const char *functionName = functionCodeToName(cmd[4]);
    Debug::out(LOG_DEBUG, "TranslatedDisk function - %s (%02x)", functionName, cmd[4]);
	//>dataTrans->dumpDataOnce();

    // now do all the command handling
    switch(cmd[4]) {
        // special CosmosEx commands for this module
        case TRAN_CMD_IDENTIFY:
        dataTrans->addDataBfr("CosmosEx translated disk", 24, true);       // add identity string with padding
        dataTrans->setStatus(E_OK);
        break;

        case TRAN_CMD_GETDATETIME:
        dateAcsiCommand->processCommand(cmd);
        break;

		case TRAN_CMD_SCREENCASTPALETTE:
        case TRAN_CMD_SENDSCREENCAST:
        	screencastAcsiCommand->processCommand(cmd);
        	break;

        case TRAN_CMD_SCREENSHOT_CONFIG:    getScreenShotConfig(cmd);   break;
            
        // path functions
        case GEMDOS_Dsetdrv:        onDsetdrv(cmd);     break;
        case GEMDOS_Dgetdrv:        onDgetdrv(cmd);     break;
        case GEMDOS_Dsetpath:       onDsetpath(cmd);    break;
        case GEMDOS_Dgetpath:       onDgetpath(cmd);    break;

        // directory & file search
//      case GEMDOS_Fsetdta:        onFsetdta(cmd);     break;        // this function needs to be handled on ST only
//      case GEMDOS_Fgetdta:        onFgetdta(cmd);     break;        // this function needs to be handled on ST only
        case GEMDOS_Fsfirst:        onFsfirst(cmd);     break;
        case GEMDOS_Fsnext:         onFsnext(cmd);      break;

        // file and directory manipulation
        case GEMDOS_Dfree:          onDfree(cmd);       break;
        case GEMDOS_Dcreate:        onDcreate(cmd);     break;
        case GEMDOS_Ddelete:        onDdelete(cmd);     break;
        case GEMDOS_Frename:        onFrename(cmd);     break;
        case GEMDOS_Fdatime:        onFdatime(cmd);     break;
        case GEMDOS_Fdelete:        onFdelete(cmd);     break;
        case GEMDOS_Fattrib:        onFattrib(cmd);     break;

        // file content functions
        case GEMDOS_Fcreate:        onFcreate(cmd);     break;
        case GEMDOS_Fopen:          onFopen(cmd);       break;
        case GEMDOS_Fclose:         onFclose(cmd);      break;
        case GEMDOS_Fread:          onFread(cmd);       break;
        case GEMDOS_Fwrite:         onFwrite(cmd);      break;
        case GEMDOS_Fseek:          onFseek(cmd);       break;

        // Pexec() related stuff
        case GEMDOS_Pexec:          onPexec(cmd);       break;

        // custom functions, which are not translated gemdos functions, but needed to do some other work
        case GD_CUSTOM_initialize:      onInitialize();                 break;
        case GD_CUSTOM_getConfig:       onGetConfig(cmd);               break;
        case GD_CUSTOM_ftell:           onFtell(cmd);                   break;
        case GD_CUSTOM_getRWdataCnt:    onRWDataCount(cmd);             break;
        case GD_CUSTOM_Fsnext_last:     onFsnext_last(cmd);             break;
        case GD_CUSTOM_getBytesToEOF:   getByteCountToEndOfFile(cmd);   break;

        // BIOS functions we need to support
        case BIOS_Drvmap:               onDrvMap(cmd);                  break;
        case BIOS_Mediach:              onMediach(cmd);                 break;
        case BIOS_Getbpb:               onGetbpb(cmd);                  break;

		// other functions
		case ACC_GET_MOUNTS:			onGetMounts(cmd);	            break;
        case ACC_UNMOUNT_DRIVE:         onUnmountDrive(cmd);            break;
        case ST_LOG_TEXT:               onStLog(cmd);                   break;
        case TEST_READ:                 onTestRead(cmd);                break;
        case TEST_WRITE:                onTestWrite(cmd);               break;
        case TEST_GET_ACSI_IDS:         onTestGetACSIids(cmd);          break;

        // in other cases
        default:                                // in other cases
        dataTrans->setStatus(EINVFN);           // invalid function
        break;
    }

    dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
}

void TranslatedDisk::onUnmountDrive(BYTE *cmd)
{
    int drive = cmd[5];

    if(drive < 2 || drive > 15) {               // index out of range?
        dataTrans->setStatus(EDRVNR);
        return;
    }

    // if shared drive or config drive, don't do anything
    if(conf[drive].translatedType == TRANSLATEDTYPE_SHAREDDRIVE || conf[drive].translatedType == TRANSLATEDTYPE_CONFIGDRIVE) {
        dataTrans->setStatus(E_OK);
        return;
    }

    Debug::out(LOG_DEBUG, "onUnmountDrive -- drive: %d, hostRootPath: %s", drive, conf[drive].hostRootPath.c_str());

    // send umount request
	TMounterRequest tmr;
    tmr.action			= MOUNTER_ACTION_UMOUNT;							// action: umount
	tmr.mountDir		= conf[drive].hostRootPath;							// e.g. /mnt/sda2
	Mounter::add(tmr);

    detachByIndex(drive);                                                   // detach drive from translated disk module

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onGetMounts(BYTE *cmd)
{
	char tmp[256];
	std::string mounts;
	int index;

	static const char *trTypeStr[4] = {"", "USB drive", "shared drive", "config drive"};
	const char *mountStr;

    for(int i=2; i<MAX_DRIVES; i++) {       // create enabled drive bits
        if(conf[i].enabled) {
			index = conf[i].translatedType + 1;
		} else {
			index = 0;
		}

		mountStr = trTypeStr[index];
		if(conf[i].label.empty()) {
			sprintf(tmp, "%c: %s\n", ('A' + i), mountStr);
		} else {
			sprintf(tmp, "%c: %s (%s)\n", ('A' + i), conf[i].label.c_str(), mountStr);
		}

		mounts += tmp;
    }

	dataTrans->addDataCString(mounts.c_str(), true);
	dataTrans->setStatus(E_OK);
}

WORD TranslatedDisk::getDrivesBitmap(void)
{
    WORD drives = 0;

    for(int i=0; i<MAX_DRIVES; i++) {       // create enabled drive bits
        if(i == 0 || i == 1) {              // A and B enabled by default
            drives |= (1 << i);
        }

        if(conf[i].enabled) {
            drives |= (1 << i);             // set the bit
        }
    }

    return drives;
}

void TranslatedDisk::closeAllFiles(void)
{
    for(int i=0; i<MAX_FILES; i++) {        // close all open files
        if(files[i].hostHandle == NULL) {   // if file is not open, skip it
            continue;
        }

        closeFileByIndex(i);
    }
}

void TranslatedDisk::closeFileByIndex(int index)
{
    if(index < 0 || index > MAX_FILES) {
        return;
    }

    if(files[index].hostHandle != NULL) {   // if file is open, close it
        fclose(files[index].hostHandle);
    }

    // now init the vars
    files[index].hostHandle     = NULL;
    files[index].atariHandle    = EIHNDL;
    files[index].hostPath       = "";
}

void TranslatedDisk::onInitialize(void)     // this method is called on the startup of CosmosEx translated disk driver
{
    closeAllFiles();

    tempFindStorage.clear();
    clearFindStorages();
    
    DWORD res;
    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "TranslatedDisk::onInitialize - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }

    // get the current machine info and generate DESKTOP.INF file
    WORD tosVersion, curRes, drives;
    
    tosVersion  = Utils::getWord(dataBuffer + 0);
    curRes      = Utils::getWord(dataBuffer + 2);
    drives      = Utils::getWord(dataBuffer + 4);
    
    //------------------------------------
    // depending on TOS major version determine the machine, on which this SCSI device is used, and limit the available SCSI IDs depending on that
    BYTE tosVersionMajor = tosVersion >> 8;
    
    int oldHwScsiMachine = hwConfig.scsiMachine;                // store the old value
    
    if(tosVersionMajor == 3) {                                  // TOS 3 == TT
        hwConfig.scsiMachine = SCSI_MACHINE_TT;
    } else if(tosVersionMajor == 4) {                           // TOS 4 == Falcon
        hwConfig.scsiMachine = SCSI_MACHINE_FALCON;
    } else {                                                    // other TOSes - probably not SCSI
        hwConfig.scsiMachine = SCSI_MACHINE_UNKNOWN;
    }
    
    if(oldHwScsiMachine != hwConfig.scsiMachine) {              // SCSI machine changed? resend SCSI IDs to Hans
        if(reloadProxy) {                                       // if got settings reload proxy, invoke reload
            Debug::out(LOG_DEBUG, "TranslatedDisk::onInitialize() - SCSI machine changed, will resend new SCSI IDs");
            reloadProxy->reloadSettings(SETTINGSUSER_SCSI_IDS);
        }
    }
    //------------------------------------
    
    WORD translatedDrives = getDrivesBitmap();          // get bitmap of all translated drives we got
    
    DesktopConfig dc;
    dc.tosVersion        = tosVersion;                   // TOS version as reported in TOS
    dc.currentResolution = curRes;                       // screen resolution as reported by Getrez()
    dc.drivesAll         = drives | translatedDrives;    // all drives = drives reported by Drvmap() + all translated drives
    dc.translatedDrives  = translatedDrives;             // just translated drives
    dc.configDrive       = driveLetters.confDrive;       // index of config drive
    dc.sharedDrive       = driveLetters.shared;          // index of shared drive
    
    Settings s;
    dc.settingsResolution   = s.getInt("SCREEN_RESOLUTION", 1);
    for(int i = 2; i < MAX_DRIVES; i++) {
        dc.label[i] = conf[i].label;
    }

    //system("rm -f /tmp/configdrive/*.inf");             // remove any *.inf file
    //system("rm -f /tmp/configdrive/*.INF");             // remove any *.INF file, too

    DesktopCreator::createToFile(&dc);                  // create the DESKTOP.INF / NEWDESK.INF file

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onGetConfig(BYTE *cmd)
{
    // 1st WORD (bytes 0, 1) - bitmap of CosmosEx translated drives
    WORD drives = getDrivesBitmap();
    dataTrans->addDataWord(drives);                             // drive bits first

    // bytes 2,3,4 -- drive letters assignment
    dataTrans->addDataByte(driveLetters.firstTranslated);       // first translated drive
    dataTrans->addDataByte(driveLetters.shared);                // shared drive
    dataTrans->addDataByte(driveLetters.confDrive);             // config drive

    //------------------
    // this can be used to set the right date and time on ST
    Settings s;
    bool    setDateTime;
    float   utcOffset;
    setDateTime = s.getBool ("TIME_SET",        true);
    utcOffset   = s.getFloat("TIME_UTC_OFFSET", 0);

    int iUtcOffset = (int) (utcOffset * 10.0);

    time_t timenow      = time(NULL);                           // get time -- the UTC offset / timezone will be applied in localtime()
    struct tm loctime   = *localtime(&timenow);

    if((loctime.tm_year + 1900) < 2014) {                       // if the retrieved date/time seems to be wrong (too old), clear it and don't set it!
        memset(&loctime, 0, sizeof(tm));
        setDateTime = false;
    }

    dataTrans->addDataByte(setDateTime);                        // byte 5 - if should set date/time on ST or not
    dataTrans->addDataByte(iUtcOffset);                         // byte 6 - UTC offset: +- hours * 10 (+3.5 will be +35, -3.5 will be -35)

    dataTrans->addDataWord(loctime.tm_year + 1900);             // bytes 7, 8 - year
    dataTrans->addDataByte(loctime.tm_mon + 1);                 // byte 9     - month
    dataTrans->addDataByte(loctime.tm_mday);                    // byte 10    - day

    dataTrans->addDataByte(loctime.tm_hour);                    // byte 11 - hours
    dataTrans->addDataByte(loctime.tm_min);                     // byte 12 - minutes
    dataTrans->addDataByte(loctime.tm_sec);                     // byte 13 - seconds

    Debug::out(LOG_DEBUG, "onGetConfig - setDateTime %d, utcOffset %d, %04d-%02d-%02d %02d:%02d:%02d", setDateTime, utcOffset, loctime.tm_year + 1900, loctime.tm_mon + 1, loctime.tm_mday, loctime.tm_hour, loctime.tm_min, loctime.tm_sec);
    //------------------
    // now get and send the IP addresses of eth0 and wlan0 (at +0 is eth0, at +5 is wlan)
    BYTE tmp[10];
    Utils::getIpAdds(tmp);

    dataTrans->addDataBfr(tmp, 10, false);                      // store it to buffer - bytes 14 to 23 (byte 14 is eth0_enabled, byte 19 is wlan0_enabled)
    //------------------

	//after sending screencast skip frameSkip frames
    int frameSkip = s.getInt ("SCREENCAST_FRAMESKIP",        20);
    dataTrans->addDataByte(frameSkip);                     		// byte 24 - frame skip for screencast

    //-----------------
    dataTrans->addDataWord(TRANSLATEDDISK_VERSION);             // byte 25 & 26 - version of translated disk interface / protocol -- driver will check this, and will refuse to work in cases of mismatch
    
    //-----------------
    dataTrans->addDataByte(events.screenShotVblEnabled);        // byte 27: enable screenshot VBL?
    dataTrans->addDataByte(events.doScreenShot);                // byte 28: take screenshot?
    events.doScreenShot = false;                                // unset this flag so we will send only one screenshot
    //-----------------

    dataTrans->padDataToMul16();                                // pad to multiple of 16

    dataTrans->setStatus(E_OK);
}

bool TranslatedDisk::hostPathExists(std::string hostPath)
{
    // now check if it exists
    int res = access(hostPath.c_str(), F_OK);

    if(res != -1) {             // if it's not this error, then the file exists
        Debug::out(LOG_DEBUG, "TranslatedDisk::hostPathExists( %s ) == TRUE (file / dir exists)", hostPath.c_str());
        return true;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::hostPathExists( %s ) == FALSE (file / dir does not exist)", hostPath.c_str());
    return false;
}

bool TranslatedDisk::createFullAtariPathAndFullHostPath(const std::string &inPartialAtariPath, std::string &outFullAtariPath, int &outAtariDriveIndex, std::string &outFullHostPath, bool &waitingForMount, int &zipDirNestingLevel)
{
    bool res;

    // initialize variables - in case that anything following fails
    outFullAtariPath    = "";
    outAtariDriveIndex  = -1;
    outFullHostPath     = "";
    waitingForMount     = false;
    zipDirNestingLevel  = 0;

    // convert partial atari path to full atari path
    res = createFullAtariPath(inPartialAtariPath, outFullAtariPath, outAtariDriveIndex);        
    
    if(!res) {                  // if createFullAtariPath() failed, don't do the rest
        return false;
    }
    
    // got full atari path, now try to create full host path
    createFullHostPath(outFullAtariPath, outAtariDriveIndex, outFullHostPath, waitingForMount, zipDirNestingLevel);
    return true;
}

bool TranslatedDisk::createFullAtariPath(std::string inPartialAtariPath, std::string &outFullAtariPath, int &outAtariDriveIndex)
{
    outAtariDriveIndex = -1;

    //Debug::out(LOG_DEBUG, "TranslatedDisk::createFullAtariPath - inPartialAtariPath: %s", inPartialAtariPath.c_str());

    pathSeparatorAtariToHost(inPartialAtariPath);
    
    //---------------------
    // if it's full path including drive letter, outAtariDriveIndex is in the path
    if(inPartialAtariPath[1] == ':') {
        outAtariDriveIndex = driveLetterToDriveIndex(inPartialAtariPath[0]);   // check if we have this drive or not

        if(outAtariDriveIndex == -1) {
            Debug::out(LOG_DEBUG, "TranslatedDisk::createFullAtariPath - invalid drive letter '%c' -- %s", inPartialAtariPath[0], inPartialAtariPath.c_str());
            return false;
        }

        outFullAtariPath = inPartialAtariPath.substr(2);               // required atari path is absolute path, without drive letter and ':'
        
        removeDoubleDots(outFullAtariPath);                            // search for '..' and simplify the path
        Debug::out(LOG_DEBUG, "TranslatedDisk::createFullAtariPath - with    drive letter, absolute path : %s, drive index: %d => %s", inPartialAtariPath.c_str(), outAtariDriveIndex, outFullAtariPath.c_str());
        return true;
    }

    //---------------------
    // if it's a path without drive letter, it's for current drive
    outAtariDriveIndex = currentDriveIndex;

    if(!conf[currentDriveIndex].enabled) {                          // we're trying this on disabled drive?
        Debug::out(LOG_DEBUG, "TranslatedDisk::createFullAtariPath - the current drive %d is not enabled (not translated drive)", currentDriveIndex);
        return false;
    }
    
    if(startsWith(inPartialAtariPath, HOSTPATH_SEPAR_STRING)) {         // starts with backslash? absolute path on current drive
        outFullAtariPath = inPartialAtariPath.substr(1);
        removeDoubleDots(outFullAtariPath);                             // search for '..' and simplify the path
        
        Debug::out(LOG_DEBUG, "TranslatedDisk::createFullAtariPath - without drive letter, absolute path : %s => %s ", inPartialAtariPath.c_str(), outFullAtariPath.c_str());
        return true;
    } else {                                                        // starts without backslash? relative path on current drive
        outFullAtariPath = conf[currentDriveIndex].currentAtariPath;
        Utils::mergeHostPaths(outFullAtariPath, inPartialAtariPath);
        removeDoubleDots(outFullAtariPath);                             // search for '..' and simplify the path
        
        Debug::out(LOG_DEBUG, "TranslatedDisk::createFullAtariPath - without drive letter, relative path : %s => %s", inPartialAtariPath.c_str(), outFullAtariPath.c_str());
        return true;
    }
}

void TranslatedDisk::createFullHostPath(const std::string &inFullAtariPath, int inAtariDriveIndex, std::string &outFullHostPath, bool &waitingForMount, int &zipDirNestingLevel)
{
    waitingForMount     = false;
    zipDirNestingLevel  = 0;                                        // no ZIP DIR nesting atm

    std::string root = conf[inAtariDriveIndex].hostRootPath;        // get root path
    
    std::string partialLongHostPath;
    conf[inAtariDriveIndex].dirTranslator.shortToLongPath(root, inFullAtariPath, partialLongHostPath);	// now convert short to long path

    Debug::out(LOG_DEBUG, "TranslatedDisk::createFullHostPath - dirTranslator.shortToLongPath -- root: %s, inFullAtariPath: %s -> partialLongHostPath: %s", root.c_str(), inFullAtariPath.c_str(), partialLongHostPath.c_str());
    
    outFullHostPath = root;
    Utils::mergeHostPaths(outFullHostPath, partialLongHostPath);    // merge 
    
    if(useZipdirNotFile) {                                          // if ZIP DIRs are enabled
        replaceHostPathWithZipDirPath(inAtariDriveIndex, outFullHostPath, waitingForMount, zipDirNestingLevel);
        Debug::out(LOG_DEBUG, "TranslatedDisk::createFullHostPath - replaceHostPathWithZipDirPath -- new outFullHostPath: %s , waitingForMount: %d, zipDirNestingLevel: %d", outFullHostPath.c_str(), (int) waitingForMount, zipDirNestingLevel);
    }
}

int TranslatedDisk::driveLetterToDriveIndex(char pathDriveLetter)
{
    int driveIndex = 0;

    if(!isValidDriveLetter(pathDriveLetter)) {          // not a valid drive letter?
        Debug::out(LOG_DEBUG, "TranslatedDisk::createHostPath - invalid drive letter");
        return -1;
    }

    pathDriveLetter = toUpperCase(pathDriveLetter);     // make sure it's upper case
    driveIndex = pathDriveLetter - 'A';                 // calculate drive index

    if(driveIndex < 2) {                                // drive A and B not handled
        Debug::out(LOG_DEBUG, "TranslatedDisk::createHostPath - drive A & B not handled");
        return -1;
    }

    if(!conf[driveIndex].enabled) {                     // that drive is not enabled?
        Debug::out(LOG_DEBUG, "TranslatedDisk::createHostPath - drive not enabled");
        return -1;
    }

    return driveIndex;
}

bool TranslatedDisk::isDriveIndexReadOnly(int driveIndex)
{
    if(driveIndex < 0 || driveIndex > 15) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::isDriveIndexReadOnly -- drive index: %d -> out of index, not READ ONLY ", driveIndex);
        return false;
    }

    WORD mask = (1 << driveIndex);

    if((driveLetters.readOnly & mask) != 0) {               // if the bit representing the drive is set, it's read only
        Debug::out(LOG_DEBUG, "TranslatedDisk::isDriveIndexReadOnly -- drive index: %d -> is READ ONLY ", driveIndex);
        return true;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::isDriveIndexReadOnly -- drive index: %d -> not READ ONLY ", driveIndex);
    return false;
}

void TranslatedDisk::removeDoubleDots(std::string &path)
{
    #define MAX_DIR_NESTING     64

//    Debug::out(LOG_DEBUG, "removeDoubleDots before: %s", path.c_str());

    std::string strings[MAX_DIR_NESTING];
    int found = 0, start = 0, pos;

    // first split the string by separator
    while(1) {
        pos = path.find(HOSTPATH_SEPAR_CHAR, start);

        if(pos == -1) {                             // not found?
            strings[found] = path.substr(start);    // copy in the rest
            found++;
            break;
        }

        strings[found] = path.substr(start, (pos - start));
        found++;

        start = pos + 1;

        if(found >= MAX_DIR_NESTING) {              // sanitize possible overflow
            Debug::out(LOG_ERROR, "removeDoubleDots has reached maximum dir nesting level, not removing double dost!");
            break;
        }
    }

    // now remove the double dots and single dots
    for(int i=0; i<found; i++) {            // go forward to find double dot
        if(strings[i] == ".") {             // single dot found?
            strings[i] = "";                // remove it
            continue;
        }
    
        if(strings[i] == "..") {            // double dot found?
            strings[i] = "";                // remove it

            for(int j=(i-1); j>=0; j--) {       // now go backward to find something what is not empty
                if(strings[j].length() > 0) {   // found something non-empty? remove it
                   strings[j] = "";
                   break;
                }
            }
        }
    }

    // and finally - put the string back together
    std::string final = "";

    for(int i=0; i<found; i++) {
        if(strings[i].length() != 0) {      // not empty string?
            final = final + HOSTPATH_SEPAR_STRING + strings[i];
        }
    }

//    Debug::out(LOG_DEBUG, "removeDoubleDots after: %s", final.c_str());
    path = final;
}

bool TranslatedDisk::isLetter(char a)
{
    if((a >= 'a' && a <= 'z') || (a >= 'A' && a <= 'Z')) {
        return true;
    }

    return false;
}

char TranslatedDisk::toUpperCase(char a)
{
    if(a >= 'a' && a <= 'z') {      // if is lower case
        return (a - 32);
    }

    return a;
}

bool TranslatedDisk::isValidDriveLetter(char a)
{
    if(a >= 'A' && a <= 'P') {
        return true;
    }

    if(a >= 'a' && a <= 'p') {
        return true;
    }

    return false;
}

int  TranslatedDisk::getZipDirByMountPoint(std::string &searchedMountPoint)
{
    for(int i=0; i<MAX_ZIP_DIRS; i++) {                         // find the mount point
        if(zipDirs[i]->mountPoint == searchedMountPoint) {      // mount point found? return index
            return i;
        }
    }
    
    return -1;                                                  // mount point not found
}

bool TranslatedDisk::startsWith(std::string what, std::string subStr)
{
    if(what.find(subStr) == 0) {
        return true;
    }

    return false;
}

bool TranslatedDisk::endsWith(std::string what, std::string subStr)
{
    if(what.rfind(subStr) == (what.length() - subStr.length())) {
        return true;
    }

    return false;
}

void TranslatedDisk::pathSeparatorAtariToHost(std::string &path)
{
    int len, i;
    len = path.length();

    for(i = 0; i<len; i++) {
        if(path[i] == ATARIPATH_SEPAR_CHAR) {   // if atari separator
            path[i] = HOSTPATH_SEPAR_CHAR;      // change to host separator
        }
    }
}

void TranslatedDisk::pathSeparatorHostToAtari(std::string &path)
{
    int len, i;
    len = path.length();

    for(i = 0; i<len; i++) {
        if(path[i] == HOSTPATH_SEPAR_CHAR) {    // if host separator
            path[i] = ATARIPATH_SEPAR_CHAR;      // change to atari separator
        }
    }
}

int TranslatedDisk::deleteDirectoryPlain(const char *path)
{
    // if the dir is empty, it will delete it, otherwise it will fail
    int res = rmdir(path);
    
    if(res == 0) {              // on success return success
        return E_OK;
    }
    Debug::out(LOG_ERROR, "TranslatedDisk::deleteDirectoryPlain rmdir(%s) : %s", path, strerror(errno));
    return EACCDN;
}

bool TranslatedDisk::isRootDir(std::string hostPath)
{
	// remove trailing '/' if needed
	if(hostPath.size() > 0 && hostPath[hostPath.size() - 1] == HOSTPATH_SEPAR_CHAR) {
		hostPath.erase(hostPath.size() - 1, 1);
	}

    for(int i=2; i<MAX_DRIVES; i++) {                   // go through all translated drives
        if(!conf[i].enabled) {                          // skip disabled drives
            continue;
        }
        
        //Debug::out(LOG_DEBUG, "TranslatedDisk::isRootDir - %s == %s ?", conf[i].hostRootPath.c_str(), hostPath.c_str());
        if(conf[i].hostRootPath == hostPath) {          // ok, this is root dir!
            Debug::out(LOG_DEBUG, "TranslatedDisk::isRootDir - hostPath: %s -- yes, it's a root dir", hostPath.c_str());
            return true;
        }
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::isRootDir - hostPath: %s -- no, it's NOT a root dir", hostPath.c_str());
    return false;                                       // this wasn't found as root dir
}

void TranslatedDisk::initFindStorages(void)             // use once to init it on start
{
    for(int i=0; i<MAX_FIND_STORAGES; i++) {
        findStorages[i] = NULL;
    }
}

void TranslatedDisk::clearFindStorages(void)            // use it on ST reset to clear useless find storages, but don't free memory
{
    for(int i=0; i<MAX_FIND_STORAGES; i++) {
        if(findStorages[i] != NULL) {
            findStorages[i]->clear();
        }
    }
}

void TranslatedDisk::destroyFindStorages(void)          // use it at the end release all the memory
{
    for(int i=0; i<MAX_FIND_STORAGES; i++) {
        if(findStorages[i] != NULL) {
            delete findStorages[i];
            findStorages[i] = NULL;
        }
    }
}

int TranslatedDisk::getEmptyFindStorageIndex(void)
{
    // search allocated findStorages, see if some of them is allocated but not used
    for(int i=0; i<MAX_FIND_STORAGES; i++) {                    
        if(findStorages[i] == NULL) {                           // skip not allocated findStorages
            continue;
        }

        if(findStorages[i]->dta == 0) {                         // found one findStorage, which is not used?
            return i;                                           // return index
        }
    }

    // search for first non-allocated findStorage
    for(int i=0; i<MAX_FIND_STORAGES; i++) {                    
        if(findStorages[i] != NULL) {                           // skip the allocated findStorages
            continue;
        }

        findStorages[i] = new TFindStorage();                   // allocated    
        return i;                                               // return index
    }

    return -1;                                                  // not found, return -1
}

int TranslatedDisk::getFindStorageIndexByDta(DWORD dta)         // find the findStorage with the specified DTA
{
    for(int i=0; i<MAX_FIND_STORAGES; i++) {
        if(findStorages[i] == NULL) {                           // skip not allocated findStorages
            continue;
        }

        if(findStorages[i]->dta == dta) {
            return i;                                           // found, return index
        }
    }

    return -1;                                                  // not found, return -1
}

void TranslatedDisk::onStLog(BYTE *cmd)
{
    DWORD res;
    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "TranslatedDisk::onStLog - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }

    Debug::out(LOG_DEBUG, "ST log: %s", dataBuffer);
    dataTrans->setStatus(E_OK);
}

const char *TranslatedDisk::functionCodeToName(int code)
{
    switch(code) {
        case TRAN_CMD_IDENTIFY:      	    return "TRAN_CMD_IDENTIFY";
        case TRAN_CMD_GETDATETIME:          return "TRAN_CMD_GETDATETIME";
        case TRAN_CMD_SENDSCREENCAST:       return "TRAN_CMD_SENDSCREENCAST";
        case TRAN_CMD_SCREENCASTPALETTE:    return "TRAN_CMD_SCREENCASTPALETTE";
        case TRAN_CMD_SCREENSHOT_CONFIG:    return "TRAN_CMD_SCREENSHOT_CONFIG";
        case ST_LOG_TEXT:                   return "ST_LOG_TEXT";
        
        case GEMDOS_Dsetdrv:            return "GEMDOS_Dsetdrv";
        case GEMDOS_Dgetdrv:            return "GEMDOS_Dgetdrv";
        case GEMDOS_Dsetpath:           return "GEMDOS_Dsetpath";
        case GEMDOS_Dgetpath:           return "GEMDOS_Dgetpath";
        case GEMDOS_Fsetdta:            return "GEMDOS_Fsetdta";
        case GEMDOS_Fgetdta:            return "GEMDOS_Fgetdta";
        case GEMDOS_Fsfirst:            return "GEMDOS_Fsfirst";
        case GEMDOS_Fsnext:             return "GEMDOS_Fsnext";
        case GEMDOS_Dfree:              return "GEMDOS_Dfree";
        case GEMDOS_Dcreate:            return "GEMDOS_Dcreate";
        case GEMDOS_Ddelete:            return "GEMDOS_Ddelete";
        case GEMDOS_Frename:            return "GEMDOS_Frename";
        case GEMDOS_Fdatime:            return "GEMDOS_Fdatime";
        case GEMDOS_Fdelete:            return "GEMDOS_Fdelete";
        case GEMDOS_Fattrib:            return "GEMDOS_Fattrib";
        case GEMDOS_Fcreate:            return "GEMDOS_Fcreate";
        case GEMDOS_Fopen:              return "GEMDOS_Fopen";
        case GEMDOS_Fclose:             return "GEMDOS_Fclose";
        case GEMDOS_Fread:              return "GEMDOS_Fread";
        case GEMDOS_Fwrite:             return "GEMDOS_Fwrite";
        case GEMDOS_Fseek:              return "GEMDOS_Fseek";
        case GEMDOS_Tgetdate:           return "GEMDOS_Tgetdate";
        case GEMDOS_Tsetdate:           return "GEMDOS_Tsetdate";
        case GEMDOS_Tgettime:           return "GEMDOS_Tgettime";
        case GEMDOS_Tsettime:           return "GEMDOS_Tsettime";
        
        case GEMDOS_Pexec:              return "GEMDOS_Pexec & sub commands";
        
        case GD_CUSTOM_initialize:      return "GD_CUSTOM_initialize";
        case GD_CUSTOM_getConfig:       return "GD_CUSTOM_getConfig";
        case GD_CUSTOM_ftell:           return "GD_CUSTOM_ftell";
        case GD_CUSTOM_getRWdataCnt:    return "GD_CUSTOM_getRWdataCnt";
        case GD_CUSTOM_Fsnext_last:     return "GD_CUSTOM_Fsnext_last";
        case GD_CUSTOM_getBytesToEOF:   return "GD_CUSTOM_getBytesToEOF";
        case BIOS_Drvmap:               return "BIOS_Drvmap";
        case BIOS_Mediach:              return "BIOS_Mediach";
        case BIOS_Getbpb:               return "BIOS_Getbpb";
        
        case TEST_READ:                 return "TEST_READ";
        case TEST_WRITE:                return "TEST_WRITE";
        default:                        return "unknown";
    }
}

void TranslatedDisk::initAsciiTranslationTable(void)
{
    memset(asciiAtariToPc, 0, 256);

    asciiAtariToPc[128] = 'C';
    asciiAtariToPc[129] = 'u';
    asciiAtariToPc[130] = 'e';
    asciiAtariToPc[131] = 'a';
    asciiAtariToPc[132] = 'a';
    asciiAtariToPc[133] = 'a';
    asciiAtariToPc[134] = 'a';
    asciiAtariToPc[135] = 'c';
    asciiAtariToPc[136] = 'e';
    asciiAtariToPc[137] = 'e';
    asciiAtariToPc[138] = 'e';
    asciiAtariToPc[139] = 'i';
    asciiAtariToPc[140] = 'i';
    asciiAtariToPc[141] = 'i';
    asciiAtariToPc[142] = 'A';
    asciiAtariToPc[143] = 'A';
    asciiAtariToPc[144] = 'E';
    asciiAtariToPc[145] = 'a';
    asciiAtariToPc[146] = 'A';
    asciiAtariToPc[147] = 'o';
    asciiAtariToPc[148] = 'o';
    asciiAtariToPc[149] = 'o';
    asciiAtariToPc[150] = 'u';
    asciiAtariToPc[151] = 'u';
    asciiAtariToPc[152] = 'y';
    asciiAtariToPc[153] = 'o';
    asciiAtariToPc[154] = 'U';
    asciiAtariToPc[155] = 'c';
    asciiAtariToPc[156] = 'p';
    asciiAtariToPc[157] = 'Y';
    asciiAtariToPc[158] = 's';
    asciiAtariToPc[159] = 'f';
    asciiAtariToPc[160] = 'a';
    asciiAtariToPc[161] = 'i';
    asciiAtariToPc[162] = 'o';
    asciiAtariToPc[163] = 'u';
    asciiAtariToPc[164] = 'n';
    asciiAtariToPc[165] = 'N';
    asciiAtariToPc[166] = 'a';
    asciiAtariToPc[167] = 'o';
    asciiAtariToPc[173] = 'i';
    asciiAtariToPc[176] = 'a';
    asciiAtariToPc[177] = 'o';
    asciiAtariToPc[178] = 'O';
    asciiAtariToPc[179] = 'o';
    asciiAtariToPc[180] = 'e';
    asciiAtariToPc[181] = 'E';
    asciiAtariToPc[182] = 'A';
    asciiAtariToPc[183] = 'A';
    asciiAtariToPc[184] = 'O';
}

bool TranslatedDisk::pathContainsWildCards(const char *path)
{
    for(int i=0; i<2048; i++) {         // go through the string. Limit length to 2048
        char in = path[i];

        if(in == 0) {                   // if it's string terminator, quit
            return false;
        }

        if(in == '?' || in == '*') {    // it's a wild card? return true
            return true;
        }
    }
    
    return false;                       // no wild card found
}

void TranslatedDisk::convertAtariASCIItoPc(char *path)
{
    int i, len;

    const char *allowed = "!#$%&'()~^@-_{}";
    #define ALLOWED_COUNT   15
    
    len = strlen(path);

    if(len >= 2048) {                                                   // probably no terminating char? go only this far
        len = 2048;
    }

    for(i=0; i<len; i++) {                                              // go through the string
        char in = path[i];

        if(in == 0) {                                                   // if it's string terminator, quit
            return;
        }

        //------------------------
        // check if it's one of the directly usable chars
        if(in >= 33 && in <= 58) {                                      // printable chars, numbers? OK
            continue;
        }

        if(in >= 64 && in <= 122) {                                     // letters, other printable chars? ok
            continue;
        }

        if(in == '?' || in == '*') {                                    // wild cards? ok
            continue;
        }

        //------------------------
        bool isAllowed = false;
        for(int j=0; j<ALLOWED_COUNT; j++) {    // try to find this char in allowed characters array
            if(in == allowed[j]) {
                isAllowed = true;
                break;
            }
        }

        if(isAllowed) {                         // it's allowed char, let it be
            continue;
        }        

        //------------------------
        // if it's not from the supported chars, try to convert it
        unsigned int uin    = (unsigned char) in;
        char conv           = asciiAtariToPc[uin];

        if(conv == 0) {                                                 // we don't have a good replacement? replace with '_'
            path[i] = '_';
        } else {                                                        // we got a replacement, use it!
            path[i] = conv;
        }
    }
}

void TranslatedDisk::getScreenShotConfig(BYTE *cmd)
{
    dataTrans->addDataByte(events.screenShotVblEnabled);

    dataTrans->addDataByte(events.doScreenShot);
    events.doScreenShot = false;                                        // unset this flag so we will send only one screenshot
    
    dataTrans->padDataToMul16();
    dataTrans->setStatus(E_OK);
}

bool TranslatedDisk::zipDirAlreadyMounted(const char *zipFile, int &zipDirIndex)
{
    DWORD minAccessTime     = 0xffffffff;
    int   minAccessIndex    = 0;

    for(int i=0; i<MAX_ZIP_DIRS; i++) {                         // find the mount point
        const char *mountedFile = zipDirs[i]->realHostPath.c_str();
        if(strcmp(mountedFile, zipFile) == 0) {                 // found the mount point? return true, zipDirIndex contains index to info
            zipDirIndex = i;
            return true;
        }
        
        if(minAccessTime > zipDirs[i]->lastAccessTime) {        // if found something that is younger than the currently youngest access time, store it
            minAccessTime   = zipDirs[i]->lastAccessTime;
            minAccessIndex  = i;
        }
    }
    
    zipDirIndex = minAccessIndex;                               // mount point not found, return false and zipDirIndex contains place where mount info should be stored
    return false;
}

void TranslatedDisk::replaceHostPathWithZipDirPath(int inAtariDriveIndex, std::string &hostPath, bool &waitingForMount, int &zipDirNestingLevel)
{
    waitingForMount = false;

    int i;
    
    for(i=0; i<MAX_ZIPDIR_NESTING; i++) {
        bool containsZip = false;                                   // flag marking if the path still contains a ZIP file and thus needs another iteration
    
        replaceHostPathWithZipDirPath_internal(hostPath, waitingForMount, containsZip);
        
        if(containsZip) {                                           // if the last call of replaceHostPathWithZipDirPath_internal() contained ZIP file, mark that the whole path contained at least one zip
            zipDirNestingLevel++;                                   // nesting level increased
        }
        
        if(waitingForMount || !containsZip) {                       // if we're waiting for mount now, or it doesn't contain ZIP, quit
            return;
        }

        //------------------
        // if we got here, part of the path now has been replaced and the rest needs to be translated from short to long
        bool hasZipDirSubPath = (hostPath.length() > 13);           // if it's not just path to ZIP DIR (e.g. not only '/tmp/zipdir3' ), but it has some sub path (e.g. '/tmp/zipdir3/LONGFI~1.TXT' )
        
        if(hasZipDirSubPath) {                                      // if the path contains at least one ZIP DIR, we need to do translation from short to long path again, as this is now a new path
            // hostPath is now something like /tmp/zipdir3/LONGFI~1.TXT , so we should break it into root ( '/tmp/zipdir3') and the rest
            std::string zipDirRoot      = hostPath.substr(0, 12);   // contains something like '/tmp/zipdir3'
            std::string zipDirSubPath   = hostPath.substr(13);      // contains the rest of the string, like 'LONGFI~1.TXT'
            
            std::string partialLongHostZipDirPath;
            conf[inAtariDriveIndex].dirTranslator.shortToLongPath(zipDirRoot, zipDirSubPath, partialLongHostZipDirPath);    // now convert short to long path
            
            hostPath = zipDirRoot;
            Utils::mergeHostPaths(hostPath, partialLongHostZipDirPath);     // merge and thus create /tmp/zipdir3/LongFileName.txt
            
            Debug::out(LOG_DEBUG, "TranslatedDisk::replaceHostPathWithZipDirPath - after ZIP DIR translation: dirTranslator.shortToLongPath -- zipDirRoot: %s, zipDirSubPath: %s -> partialLongHostZipDirPath: %s", zipDirRoot.c_str(), zipDirSubPath.c_str(), partialLongHostZipDirPath.c_str());
        }        
    }
}

void TranslatedDisk::replaceHostPathWithZipDirPath_internal(std::string &hostPath, bool &waitingForMount, bool &containsZip)
{
    const char *pHostPath = hostPath.c_str();
    waitingForMount = false;
    containsZip     = false;
 
    //----------
    // first check if the host path contains '*.ZIP' part
    const char *pZip = strcasestr(pHostPath, ".ZIP");
    
    if(pZip == NULL) {                                  // didn't find .ZIP string? just a normal path
        Debug::out(LOG_DEBUG, "TranslatedDisk::replaceHostPathWithZipDirPath_internal -- no ZIP file in hostPath: %s", pHostPath);
        containsZip = false;
        return;
    }

    int zipFilePathLen = (pZip - pHostPath) + 4;        // calculate the length of the full path to ZIP file
    
    char zipFilePath[1024];
    strncpy(zipFilePath, pHostPath, zipFilePathLen);    // copy the path to ZIP file
    zipFilePath[zipFilePathLen] = 0;                    // terminate the string
    
    Debug::out(LOG_DEBUG, "TranslatedDisk::replaceHostPathWithZipDirPath_internal -- hostPath: %s -> ZIP file: %s", pHostPath, zipFilePath);
    
    //----------
    // check if there's a real .ZIP file at the place of where the pretended ZIP DIR is
    if(!isOkToMountThisAsZipDir(zipFilePath)) {
        containsZip = false;
        return;
    }    
    //----------
    // check if the ZIP file is already mounted
    bool isMounted;
    int  zipDirIndex;
    isMounted = zipDirAlreadyMounted(zipFilePath, zipDirIndex);     // see if the file is already mounted

    if(!isMounted || !zipDirs[zipDirIndex]->isMounted) {
        doZipDirMountOrStateCheck(isMounted, zipFilePath, zipDirIndex, waitingForMount);
        
        if(waitingForMount == true) {                               // return that we're waiting for mount to finish
            containsZip = true;
            return;
        }
    }
    
    //----------
    // now replace the part of the host path with the alternative ZIP DIR path
    std::string pathInsideZipFile = hostPath.substr(zipFilePathLen);
    hostPath = zipDirs[zipDirIndex]->mountPoint + pathInsideZipFile;                // create new host path inside zip mount point

    zipDirs[zipDirIndex]->lastAccessTime = Utils::getCurrentMs();                   // mark the current time as the time of last access
    
    Debug::out(LOG_DEBUG, "TranslatedDisk::replaceHostPathWithZipDirPath_internal -- new path now is %s", hostPath.c_str());
    containsZip = true;
}

bool TranslatedDisk::isOkToMountThisAsZipDir(const char *zipFilePath)
{
    struct stat attr;
    int res = stat(zipFilePath, &attr);                 // get the status of the possible zip file
	
	if(res != 0) {
		Debug::out(LOG_ERROR, "TranslatedDisk::isOkToMountThisAsZipDir() -- stat() failed, file / dir doesn't exist?");
		return false;		
	}
	
	bool isDir  = (S_ISDIR(attr.st_mode) != 0);         // check if it's a directory
    bool isFile = (S_ISREG(attr.st_mode) != 0);         // check if it's a file

    if(isDir) {                                         // if it's a dir, quit
        Debug::out(LOG_DEBUG, "TranslatedDisk::isOkToMountThisAsZipDir -- %s is a normal dir, path not replaced", zipFilePath);
        return false;
    }
    
    if(!isFile) {                                       // if it's not a file, quit
        Debug::out(LOG_DEBUG, "TranslatedDisk::isOkToMountThisAsZipDir -- %s is a NOT a file, WTF, path not replaced", zipFilePath);
        return false;
    }
    
    if(attr.st_size > MAX_ZIPDIR_ZIPFILE_SIZE) {        // file too big? quit
        Debug::out(LOG_DEBUG, "TranslatedDisk::isOkToMountThisAsZipDir -- file %s is too big, only files smaller than 5 MB are mounted, path not replaced", zipFilePath);
        return false;
    }
    
    return true;
}

void TranslatedDisk::doZipDirMountOrStateCheck(bool isMounted, char *zipFilePath, int zipDirIndex, bool &waitingForMount)
{
    if(!isMounted) {                                    // if ZIP file not mounted yet, mount it
        Debug::out(LOG_DEBUG, "TranslatedDisk::doZipDirMountOrStateCheck -- mounting file %s to dir %s", zipFilePath, zipDirs[zipDirIndex]->mountPoint.c_str());
        
        //----------
        // issue mount request
        TMounterRequest tmr;
        tmr.action      = MOUNTER_ACTION_MOUNT_ZIP;         // action: mount ZIP file
        tmr.devicePath  = zipFilePath;                      // e.g. /mnt/shared/normal/archive.zip
        tmr.mountDir    = zipDirs[zipDirIndex]->mountPoint; // e.g. /tmp/zipdir2
        int masId       = Mounter::add(tmr);                // add mounter action, get mount action state id
        
        zipDirs[zipDirIndex]->mountActionStateId    = masId;    // store the mounter action state id, for future mount state query
        zipDirs[zipDirIndex]->isMounted             = false;    // not mounted yet
        //----------
        // mark this ZIP file as mounted
        zipDirs[zipDirIndex]->realHostPath = zipFilePath;   // store path to zip file - this will be marker that this zip file is mounted
        
        waitingForMount = true;                             // return that we're waiting for mount to finish
        return;
    } else {
        Debug::out(LOG_DEBUG, "TranslatedDisk::doZipDirMountOrStateCheck -- file %s already mounted to %s, reusing and not mounting", zipFilePath, zipDirs[zipDirIndex]->mountPoint.c_str());
        
        if(!zipDirs[zipDirIndex]->isMounted) {                                      // if not mounted yet
            int state = Mounter::mas_getState(zipDirs[zipDirIndex]->mountActionStateId);     // get state of this mount action
            
            if(state == MOUNTACTION_STATE_DONE) {                                   // if state is DONE, mark that it's mounted and continue
                zipDirs[zipDirIndex]->isMounted = true;
            } else {                                                                // state is NOT DONE yet
                waitingForMount = true;                                             // return that we're waiting for mount to finish
                return;
            }
        }
        
        // if mounted, it continues here
    }
}

bool TranslatedDisk::driveIsEnabled(int driveIndex)
{
    if(driveIndex < 0 || driveIndex >= MAX_DRIVES) {        // out of bounds? fail
        return false;
    }

    return conf[driveIndex].enabled;
}

void TranslatedDisk::driveGetReport(int driveIndex, std::string &reportString)
{
    reportString = "";

    if(driveIndex < 0 || driveIndex >= MAX_DRIVES) {        // out of bounds? fail
        return;
    }

    if(!conf[driveIndex].enabled) {                         // not enabled? fail
        return;
    }

  	int typeIndex  = conf[driveIndex].translatedType;

    switch(typeIndex) {
        case TRANSLATEDTYPE_NORMAL:
            if(conf[driveIndex].label.empty())
                reportString = "USB drive";
            else {
                reportString = conf[driveIndex].label;
                reportString += " (USB drive)";
            }
            reportString += " - device: ";
            reportString += conf[driveIndex].devicePath;
            break;
        case TRANSLATEDTYPE_SHAREDDRIVE:
            reportString = "shared drive, ";
            reportString += conf[driveIndex].devicePath;
            break;
        case TRANSLATEDTYPE_CONFIGDRIVE:
            reportString = "config drive (located at ";
            reportString += conf[driveIndex].devicePath;
            reportString += ")";
            break;
        default:
            reportString = "unknown";
    }
}
