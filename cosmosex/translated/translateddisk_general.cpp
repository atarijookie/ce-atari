#include <string>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include "../global.h"
#include "../debug.h"
#include "../settings.h"
#include "../utils.h"
#include "../mounter.h"
#include "translateddisk.h"
#include "gemdos.h"
#include "gemdos_errno.h"

TranslatedDisk::TranslatedDisk(AcsiDataTrans *dt, ConfigService *cs, ScreencastService *scs)
{
    dataTrans = dt;
    configService = cs;

    dataBuffer  = new BYTE[BUFFER_SIZE];
    dataBuffer2 = new BYTE[BUFFER_SIZE];

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
}

TranslatedDisk::~TranslatedDisk()
{
    delete dateAcsiCommand;
    delete screencastAcsiCommand;

    delete []dataBuffer;
    delete []dataBuffer2;

    destroyFindStorages();
}

void TranslatedDisk::loadSettings(void)
{
    Debug::out(LOG_DEBUG, "TranslatedDisk::loadSettings");

    Settings s;
    char drive1, drive2, drive3;

    drive1 = s.getChar((char *) "DRIVELETTER_FIRST",      -1);
    drive2 = s.getChar((char *) "DRIVELETTER_SHARED",     -1);
    drive3 = s.getChar((char *) "DRIVELETTER_CONFDRIVE",  'O');

    driveLetters.firstTranslated    = drive1 - 'A';
    driveLetters.shared             = drive2 - 'A';
    driveLetters.confDrive          = drive3 - 'A';

    // now set the read only drive flags
    driveLetters.readOnly = 0;

    if(driveLetters.confDrive >= 0 && driveLetters.confDrive <=15) {        // if got a valid drive letter for config drive
        driveLetters.readOnly = (1 << driveLetters.confDrive);              // make config drive read only
    }
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
    }

    detachAll();                                // then deinit the conf structures

    int good = 0, bad = 0;
    bool res;

    for(int i=2; i<MAX_DRIVES; i++) {           // and now find new places
        if(!tmpConf[i].enabled) {       // skip the not used positions
            continue;
        }

        res = attachToHostPath(tmpConf[i].hostRootPath, tmpConf[i].translatedType);   // now attach back

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

    addr			= s.getString((char *) "SHARED_ADDRESS",  (char *) "");
    path			= s.getString((char *) "SHARED_PATH",     (char *) "");

    username		= s.getString((char *) "SHARED_USERNAME",  (char *) "");
    password		= s.getString((char *) "SHARED_PASSWORD",  (char *) "");

	sharedEnabled	= s.getBool((char *) "SHARED_ENABLED", false);
	nfsNotSamba		= s.getBool((char *) "SHARED_NFS_NOT_SAMBA", false);

	if(!sharedEnabled) {
		Debug::out(LOG_INFO, "mountAndAttachSharedDrive: shared drive not enabled, not mounting and not attaching...");
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
	mountAdd(tmr);

	bool res = attachToHostPath(mountPath, TRANSLATEDTYPE_SHAREDDRIVE);	// try to attach

	if(!res) {																// if didn't attach, skip the rest
		Debug::out(LOG_ERROR, "mountAndAttachSharedDrive: failed to attach shared drive %s", (char *) mountPath.c_str());
	}
}

void TranslatedDisk::attachConfigDrive(void)
{
    std::string configDrivePath = CONFIG_DRIVE_PATH;
	bool res = attachToHostPath(configDrivePath, TRANSLATEDTYPE_CONFIGDRIVE);   // try to attach

	if(!res) {																                // if didn't attach, skip the rest
		Debug::out(LOG_ERROR, "attachConfigDrive: failed to attach config drive %s", (char *) configDrivePath.c_str());
	}
}

bool TranslatedDisk::attachToHostPath(std::string hostRootPath, int translatedType)
{
    int index = -1;

    if(isAlreadyAttached(hostRootPath)) {                   // if already attached, return success
        Debug::out(LOG_DEBUG, "TranslatedDisk::attachToHostPath - already attached");
        return true;
    }

    // are we attaching shared drive?
    if(translatedType == TRANSLATEDTYPE_SHAREDDRIVE) {
        if(driveLetters.shared > 0) {                       // we have shared drive letter defined
            attachToHostPathByIndex(driveLetters.shared, hostRootPath, translatedType);

            return true;
        } else {
            return false;
        }
    }

    // are we attaching config drive?
    if(translatedType == TRANSLATEDTYPE_CONFIGDRIVE) {
        if(driveLetters.confDrive > 0) {              // we have config drive letter defined
            attachToHostPathByIndex(driveLetters.confDrive, hostRootPath, translatedType);

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

    attachToHostPathByIndex(index, hostRootPath, translatedType);
    return true;
}

void TranslatedDisk::attachToHostPathByIndex(int index, std::string hostRootPath, int translatedType)
{
    if(index < 0 || index > MAX_DRIVES) {
        return;
    }

	conf[index].dirTranslator.clear();
    conf[index].enabled             = true;
    conf[index].hostRootPath        = hostRootPath;
    conf[index].currentAtariPath    = HOSTPATH_SEPAR_STRING;
    conf[index].translatedType      = translatedType;
    conf[index].mediaChanged        = true;

    Debug::out(LOG_INFO, "TranslatedDisk::attachToHostPath - path %s attached to index %d (letter %c)", hostRootPath.c_str(), index, 'A' + index);
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

    char *functionName = functionCodeToName(cmd[4]);
    Debug::out(LOG_DEBUG, "TranslatedDisk function - %s (%02x)", functionName, cmd[4]);
	//>dataTrans->dumpDataOnce();

    // now do all the command handling
    switch(cmd[4]) {
        // special CosmosEx commands for this module
        case TRAN_CMD_IDENTIFY:
        dataTrans->addDataBfr((unsigned char *)"CosmosEx translated disk", 24, true);       // add identity string with padding
        dataTrans->setStatus(E_OK);
        break;

        case TRAN_CMD_GETDATETIME:
        dateAcsiCommand->processCommand(cmd);
        break;

		case TRAN_CMD_SCREENCASTPALETTE:
        case TRAN_CMD_SENDSCREENCAST:
        	screencastAcsiCommand->processCommand(cmd);
        	break;

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
	mountAdd(tmr);

    detachByIndex(drive);                                                   // detach drive from translated disk module

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onGetMounts(BYTE *cmd)
{
	char tmp[256];
	std::string mounts;
	int index;

	char *trTypeStr[4] = {(char *) "", (char *) "USB drive", (char *) "shared drive", (char *) "config drive"};
	char *mountStr;

    for(int i=2; i<MAX_DRIVES; i++) {       // create enabled drive bits
        if(conf[i].enabled) {
			index = conf[i].translatedType + 1;
		} else {
			index = 0;
		}

		mountStr = trTypeStr[index];
		sprintf(tmp, "%c: %s\n", ('A' + i), mountStr);

		mounts += tmp;
    }

	dataTrans->addDataBfr((BYTE *) mounts.c_str(), mounts.length(), true);
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
    setDateTime = s.getBool ((char *) "TIME_SET",        true);
    utcOffset   = s.getFloat((char *) "TIME_UTC_OFFSET", 0);

    int iUtcOffset = (int) (utcOffset * 10.0);
    int secsOffset = (int) (utcOffset * (60*60));               // transform float hours to int seconds

    time_t timenow      = time(NULL) + secsOffset;              // get time with offset
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
    int frameSkip = s.getInt ((char *) "SCREENCAST_FRAMESKIP",        20);
    dataTrans->addDataByte(frameSkip);                     		// byte 24 - frame skip for screencast

    //-----------------
    dataTrans->addDataWord(TRANSLATEDDISK_VERSION);             // byte 25 & 26 - version of translated disk interface / protocol -- driver will check this, and will refuse to work in cases of mismatch
    //-----------------

    dataTrans->padDataToMul16();                                // pad to multiple of 16

    dataTrans->setStatus(E_OK);
}

bool TranslatedDisk::hostPathExists(std::string hostPath)
{
    // now check if it exists
    int res = access(hostPath.c_str(), F_OK);

    if(res != -1) {             // if it's not this error, then the file exists
        Debug::out(LOG_DEBUG, "TranslatedDisk::hostPathExists( %s ) == TRUE (file / dir exists)", (char *) hostPath.c_str());
        return true;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::hostPathExists( %s ) == FALSE (file / dir does not exist)", (char *) hostPath.c_str());
    return false;
}

bool TranslatedDisk::createHostPath(std::string atariPath, std::string &hostPath)
{
    hostPath = "";

    Debug::out(LOG_DEBUG, "TranslatedDisk::createHostPath - atariPath: %s", (char *) atariPath.c_str());

    pathSeparatorAtariToHost(atariPath);

    if(atariPath[1] == ':') {                               // if it's full path including drive letter

        int driveIndex = 0;
        char newDrive = atariPath[0];

        if(!isValidDriveLetter(newDrive)) {                 // not a valid drive letter?
            Debug::out(LOG_DEBUG, "TranslatedDisk::createHostPath - invalid drive letter");
            return false;
        }

        newDrive = toUpperCase(newDrive);                   // make sure it's upper case
        driveIndex = newDrive - 'A';                        // calculate drive index

        if(driveIndex < 2) {                                // drive A and B not handled
            Debug::out(LOG_DEBUG, "TranslatedDisk::createHostPath - drive A & B not handled");
            return false;
        }

        if(!conf[driveIndex].enabled) {                     // that drive is not enabled?
            Debug::out(LOG_DEBUG, "TranslatedDisk::createHostPath - drive not enabled");
            return false;
        }

        std::string atariPathWithoutDrive = atariPath.substr(2);    // skip drive and semicolon (C:)

		Utils::mergeHostPaths(hostPath, atariPathWithoutDrive);		// final path = hostPath + newPath

        removeDoubleDots(hostPath);                                 // search for '..' and simplify the path

        std::string root = conf[driveIndex].hostRootPath;

		std::string longHostPath;
		conf[driveIndex].dirTranslator.shortToLongPath(root, hostPath, longHostPath);	// now convert short to long path

		hostPath = root;
		Utils::mergeHostPaths(hostPath, longHostPath);

        Debug::out(LOG_DEBUG, "TranslatedDisk::createHostPath - fill path including drive letter: atariPath: %s -> hostPath: %s", (char *) atariPath.c_str(), (char *) hostPath.c_str());
        return true;

    }

    if(!conf[currentDriveIndex].enabled) {              // we're trying this on disabled drive?
        Debug::out(LOG_DEBUG, "TranslatedDisk::createHostPath - the current drive is not enabled (not translated drive)");
        return false;
    }

    std::string root = conf[currentDriveIndex].hostRootPath;

    // need to handle with \\ at the begining, without \\ at the begining, with .. at the begining

    if(startsWith(atariPath, HOSTPATH_SEPAR_STRING)) {                  // starts with \\ == starts from root
        hostPath = atariPath.substr(1);
        removeDoubleDots(hostPath);                                     // search for '..' and simplify the path

		std::string longHostPath;
		conf[currentDriveIndex].dirTranslator.shortToLongPath(root, hostPath, longHostPath);	// now convert short to long path

		hostPath = root;
		Utils::mergeHostPaths(hostPath, longHostPath);

        Debug::out(LOG_DEBUG, "TranslatedDisk::createHostPath - starting from root -- atariPath: %s -> hostPath: %s", (char *) atariPath.c_str(), (char *) hostPath.c_str());
        return true;
    }
    
    // starts without backslash? relative path then

    if(startsWith(atariPath, "./")) {                          // if starts with current dir prefix ( ".\\" ), remove it - we're going from current dir anyway 
        atariPath = atariPath.substr(2);
        Debug::out(LOG_DEBUG, "TranslatedDisk::createHostPath - .\\ removed from start, now it's: %s", (char *) atariPath.c_str());
    }

	Utils::mergeHostPaths(hostPath, conf[currentDriveIndex].currentAtariPath);
	Utils::mergeHostPaths(hostPath, atariPath);

    removeDoubleDots(hostPath);                                 // search for '..' and simplify the path

	std::string longHostPath;
	conf[currentDriveIndex].dirTranslator.shortToLongPath(root, hostPath, longHostPath);	// now convert short to long path

	hostPath = root;
	Utils::mergeHostPaths(hostPath, longHostPath);

    Debug::out(LOG_DEBUG, "TranslatedDisk::createHostPath - relative path -- atariPath: %s -> hostPath: %s", (char *) atariPath.c_str(), (char *) hostPath.c_str());
    return true;
}

int TranslatedDisk::getDriveIndexFromAtariPath(std::string atariPath)
{
	// if it's full path including drive letter - calculate the drive index
    if(atariPath[1] == ':') {
        int driveIndex = 0;
        char newDrive = atariPath[0];

        if(!isValidDriveLetter(newDrive)) {                 // not a valid drive letter?
            Debug::out(LOG_DEBUG, "TranslatedDisk::getDriveIndexFromAtariPath -- %s -> invalid drive %c ", (char *) atariPath.c_str(), newDrive);
            return -1;
        }

        newDrive = toUpperCase(newDrive);                   // make sure it's upper case
        driveIndex = newDrive - 'A';                        // calculate drive index

        Debug::out(LOG_DEBUG, "TranslatedDisk::getDriveIndexFromAtariPath -- %s -> drive index: %d ", (char *) atariPath.c_str(), driveIndex);
		return driveIndex;
	}

	// if it wasn't full path, use current drive index
    Debug::out(LOG_DEBUG, "TranslatedDisk::getDriveIndexFromAtariPath -- %s -> current drive index: %d ", (char *) atariPath.c_str(), currentDriveIndex);
	return currentDriveIndex;
}

bool TranslatedDisk::isAtariPathReadOnly(std::string atariPath)
{
    int driveIndex = getDriveIndexFromAtariPath(atariPath);

    if(driveIndex == -1) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::isAtariPathReadOnly -- %s -> can't get drive index, not READ ONLY ", (char *) atariPath.c_str());
        return false;
    }

    return isDriveIndexReadOnly(driveIndex);
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

//    Debug::out(LOG_DEBUG, "removeDoubleDots before: %s", (char *) path.c_str());

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

    // now remove the double dots
    for(int i=0; i<found; i++) {            // go forward to find double dot
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

//    Debug::out(LOG_DEBUG, "removeDoubleDots after: %s", (char *) final.c_str());
    path = final;
}

bool TranslatedDisk::newPathRequiresCurrentDriveChange(std::string atariPath, int &newDriveIndex)
{
    newDriveIndex = currentDriveIndex;                      // initialize with the current drive index

    if(atariPath[1] != ':') {                               // no drive change sign?
        return false;
    }

    int driveIndex = 0;
    char newDrive = atariPath[0];

    if(!isValidDriveLetter(newDrive)) {                     // not a valid drive letter? we're not changing drive then
        return false;
    }

    newDrive = toUpperCase(newDrive);                       // make sure it's upper case
    driveIndex = newDrive - 'A';                            // calculate drive index

    if(driveIndex < 2) {                                    // drive A and B not handled - we're not changing drive
        return false;
    }

    if(!conf[driveIndex].enabled) {                         // that drive is not enabled? we're not changing drive
        return false;
    }

    if(driveIndex == currentDriveIndex) {                   // the new drive index is the same as the one we have now
        return false;
    }

    newDriveIndex = driveIndex;                             // store the new drive index
    return true;                                            // ok, change the drive letter
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

void TranslatedDisk::createAtariPathFromHostPath(std::string hostPath, std::string &atariPath)
{
    std::string hostRoot = conf[currentDriveIndex].hostRootPath;

    if(hostPath.find(hostRoot) == 0) {                          // the full host path contains host root
        atariPath = hostPath.substr(hostRoot.length());         // atari path = hostPath - hostRoot
    } else {                                                    // this shouldn't happen
        Debug::out(LOG_ERROR, "TranslatedDisk::createAtariPathFromHostPath -- WTF, this shouldn't happen!");
        atariPath = "";
    }
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

int TranslatedDisk::deleteDirectory(char *path)
{
    //---------
    // first recursively delete the content of this dir
    DIR *dir = opendir((char *) path);						        // try to open the dir
	
    if(dir == NULL) {                                 				// not found?
        return EPTHNF;
    }
	
    char fullPath[1024];
    
	while(1) {                                                  	// while there are more files, store them
		struct dirent *de = readdir(dir);							// read the next directory entry
	
		if(de == NULL) {											// no more entries?
			break;
		}
        
        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {     // skip current-dir and up-dir
            continue;
        }

        strcpy(fullPath, path);                                     // create full path out of parent path + this directory item
        strcat(fullPath, "/");
        strcat(fullPath, de->d_name);
        
        if(de->d_type == DT_DIR) {                                  // it's a dir? recursive delete
            deleteDirectory(fullPath);
        } else if(de->d_type == DT_REG || de->d_type == DT_LNK) {   // it's a file? unlink it
            unlink(fullPath);
        } 
    }

	closedir(dir);
    //------------
    // the dir should now be empty, delete it
    int res = rmdir(path);
    
    if(res == 0) {              // on success return success
        return E_OK;
    }
    
    return EACCDN;
}

bool TranslatedDisk::isRootDir(std::string hostPath)
{
    for(int i=2; i<MAX_DRIVES; i++) {                   // go through all translated drives
        if(!conf[i].enabled) {                          // skip disabled drives
            continue;
        }
        
        if(conf[i].hostRootPath == hostPath) {          // ok, this is root dir!
            Debug::out(LOG_DEBUG, "TranslatedDisk::isRootDir - hostPath: %s -- yes, it's a root dir", (char *) hostPath.c_str());
            return true;
        }
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::isRootDir - hostPath: %s -- no, it's NOT a root dir", (char *) hostPath.c_str());
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

char *TranslatedDisk::functionCodeToName(int code)
{
    switch(code) {
        case TRAN_CMD_IDENTIFY:      	return (char *)"TRAN_CMD_IDENTIFY";
        case TRAN_CMD_GETDATETIME:      return (char *)"TRAN_CMD_GETDATETIME";
        case TRAN_CMD_SENDSCREENCAST:   return (char *)"TRAN_CMD_SENDSCREENCAST";
        case TRAN_CMD_SCREENCASTPALETTE: return (char *)"TRAN_CMD_SCREENCASTPALETTE";
        case ST_LOG_TEXT:               return (char *)"ST_LOG_TEXT";
        case GEMDOS_Dsetdrv:            return (char *)"GEMDOS_Dsetdrv";
        case GEMDOS_Dgetdrv:            return (char *)"GEMDOS_Dgetdrv";
        case GEMDOS_Dsetpath:           return (char *)"GEMDOS_Dsetpath";
        case GEMDOS_Dgetpath:           return (char *)"GEMDOS_Dgetpath";
        case GEMDOS_Fsetdta:            return (char *)"GEMDOS_Fsetdta";
        case GEMDOS_Fgetdta:            return (char *)"GEMDOS_Fgetdta";
        case GEMDOS_Fsfirst:            return (char *)"GEMDOS_Fsfirst";
        case GEMDOS_Fsnext:             return (char *)"GEMDOS_Fsnext";
        case GEMDOS_Dfree:              return (char *)"GEMDOS_Dfree";
        case GEMDOS_Dcreate:            return (char *)"GEMDOS_Dcreate";
        case GEMDOS_Ddelete:            return (char *)"GEMDOS_Ddelete";
        case GEMDOS_Frename:            return (char *)"GEMDOS_Frename";
        case GEMDOS_Fdatime:            return (char *)"GEMDOS_Fdatime";
        case GEMDOS_Fdelete:            return (char *)"GEMDOS_Fdelete";
        case GEMDOS_Fattrib:            return (char *)"GEMDOS_Fattrib";
        case GEMDOS_Fcreate:            return (char *)"GEMDOS_Fcreate";
        case GEMDOS_Fopen:              return (char *)"GEMDOS_Fopen";
        case GEMDOS_Fclose:             return (char *)"GEMDOS_Fclose";
        case GEMDOS_Fread:              return (char *)"GEMDOS_Fread";
        case GEMDOS_Fwrite:             return (char *)"GEMDOS_Fwrite";
        case GEMDOS_Fseek:              return (char *)"GEMDOS_Fseek";
        case GEMDOS_Tgetdate:           return (char *)"GEMDOS_Tgetdate";
        case GEMDOS_Tsetdate:           return (char *)"GEMDOS_Tsetdate";
        case GEMDOS_Tgettime:           return (char *)"GEMDOS_Tgettime";
        case GEMDOS_Tsettime:           return (char *)"GEMDOS_Tsettime";
        case GD_CUSTOM_initialize:      return (char *)"GD_CUSTOM_initialize";
        case GD_CUSTOM_getConfig:       return (char *)"GD_CUSTOM_getConfig";
        case GD_CUSTOM_ftell:           return (char *)"GD_CUSTOM_ftell";
        case GD_CUSTOM_getRWdataCnt:    return (char *)"GD_CUSTOM_getRWdataCnt";
        case GD_CUSTOM_Fsnext_last:     return (char *)"GD_CUSTOM_Fsnext_last";
        case GD_CUSTOM_getBytesToEOF:   return (char *)"GD_CUSTOM_getBytesToEOF";
        case BIOS_Drvmap:               return (char *)"BIOS_Drvmap";
        case BIOS_Mediach:              return (char *)"BIOS_Mediach";
        case BIOS_Getbpb:               return (char *)"BIOS_Getbpb";
        case TEST_READ:                 return (char *)"TEST_READ";
        case TEST_WRITE:                return (char *)"TEST_WRITE";
        default:                        return (char *)"unknown";
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

void TranslatedDisk::convertAtariASCIItoPc(char *path)
{
    int i, len;

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

        if(in == '|' || in == '~') {                                    // these two? ok
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


