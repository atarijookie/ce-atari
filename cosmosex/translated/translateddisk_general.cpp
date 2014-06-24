#include <string>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "../global.h"
#include "../debug.h"
#include "../settings.h"
#include "../utils.h"
#include "../mounter.h"
#include "translateddisk.h"
#include "gemdos.h"
#include "gemdos_errno.h"

TranslatedDisk::TranslatedDisk(void)
{
    dataTrans = 0;

    dataBuffer  = new BYTE[BUFFER_SIZE];
    dataBuffer2 = new BYTE[BUFFER_SIZE];

    detachAll();

    for(int i=0; i<MAX_FILES; i++) {        // initialize host file structures
        files[i].hostHandle     = NULL;
        files[i].atariHandle    = EIHNDL;
        files[i].hostPath       = "";
    }

    loadSettings();

    findStorage.buffer      = new BYTE[BUFFER_SIZE];
    findStorage.maxCount    = BUFFER_SIZE / 23;
    findStorage.count       = 0;
    
    mountAndAttachSharedDrive();					                    // if shared drive is enabled, try to mount it and attach it
    attachConfigDrive();                                                // if config drive is enabled, attach it
}

TranslatedDisk::~TranslatedDisk()
{
    delete []dataBuffer;
    delete []dataBuffer2;

    delete []findStorage.buffer;
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
    std::string addr, path;
	bool sharedEnabled;
	bool nfsNotSamba;

    addr			= s.getString((char *) "SHARED_ADDRESS",  (char *) "");
    path			= s.getString((char *) "SHARED_PATH",     (char *) "");
	
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

void TranslatedDisk::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
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

/*
    char *functionName = functionCodeToName(cmd[4]);
    Debug::out(LOG_DEBUG, "TranslatedDisk function - %s (%02x)", functionName, cmd[4]);
*/
//	dataTrans->dumpDataOnce();
	
    // now do all the command handling
    switch(cmd[4]) {
        // special CosmosEx commands for this module
        case TRAN_CMD_IDENTIFY:
        dataTrans->addDataBfr((unsigned char *)"CosmosEx translated disk", 24, true);       // add identity string with padding
        dataTrans->setStatus(E_OK);
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

        // date and time function
        case GEMDOS_Tgetdate:       onTgetdate(cmd);    break;
        case GEMDOS_Tsetdate:       onTsetdate(cmd);    break;
        case GEMDOS_Tgettime:       onTgettime(cmd);    break;
        case GEMDOS_Tsettime:       onTsettime(cmd);    break;

        // custom functions, which are not translated gemdos functions, but needed to do some other work
        case GD_CUSTOM_initialize:      onInitialize();     break;
        case GD_CUSTOM_getConfig:       onGetConfig(cmd);   break;
        case GD_CUSTOM_ftell:           onFtell(cmd);       break;
        case GD_CUSTOM_getRWdataCnt:    onRWDataCount(cmd); break;

        // BIOS functions we need to support
        case BIOS_Drvmap:               onDrvMap(cmd);      break;
        case BIOS_Mediach:              onMediach(cmd);     break;
        case BIOS_Getbpb:               onGetbpb(cmd);      break;

		// other functions
		case ACC_GET_MOUNTS:			onGetMounts(cmd);	break;
		
        // in other cases
        default:                                // in other cases
        dataTrans->setStatus(EINVFN);           // invalid function
        break;
    }

    dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
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

    findStorage.count = 0;

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onGetConfig(BYTE *cmd)
{
    WORD drives = getDrivesBitmap();

    dataTrans->addDataWord(drives);         // drive bits first
    dataTrans->padDataToMul16();            // pad to multiple of 16

    dataTrans->setStatus(E_OK);
}

bool TranslatedDisk::hostPathExists(std::string hostPath)
{
    if(!conf[currentDriveIndex].enabled) {      // we don't have this drive? fail
        return false;
    }

    // now check if it exists
    int res = access(hostPath.c_str(), F_OK);

    if(res != -1) {             // if it's not this error, then the file exists
        return true;
    }

    return false;
}

bool TranslatedDisk::createHostPath(std::string atariPath, std::string &hostPath)
{
    hostPath = "";

    pathSeparatorAtariToHost(atariPath);

    if(atariPath[1] == ':') {                               // if it's full path including drive letter

        int driveIndex = 0;
        char newDrive = atariPath[0];

        if(!isValidDriveLetter(newDrive)) {                 // not a valid drive letter?
            return false;
        }

        newDrive = toUpperCase(newDrive);                   // make sure it's upper case
        driveIndex = newDrive - 'A';                        // calculate drive index

        if(driveIndex < 2) {                                // drive A and B not handled
            return false;
        }

        if(!conf[driveIndex].enabled) {                     // that drive is not enabled?
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

        return true;

    }

    if(!conf[currentDriveIndex].enabled) {              // we're trying this on disabled drive?
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
		
        return true;
    }

    // starts without backslash? relative path then

	Utils::mergeHostPaths(hostPath, conf[currentDriveIndex].currentAtariPath);
	Utils::mergeHostPaths(hostPath, atariPath);
	
    removeDoubleDots(hostPath);                                 // search for '..' and simplify the path

	std::string longHostPath;
	conf[currentDriveIndex].dirTranslator.shortToLongPath(root, hostPath, longHostPath);	// now convert short to long path

	hostPath = root;
	Utils::mergeHostPaths(hostPath, longHostPath);
	
//    Debug::out(LOG_DEBUG, "host path: %s", (char *) hostPath.c_str());
    return true;
}

int TranslatedDisk::getDriveIndexFromAtariPath(std::string atariPath)
{
	// if it's full path including drive letter - calculate the drive index
    if(atariPath[1] == ':') {                               
        int driveIndex = 0;
        char newDrive = atariPath[0];

        if(!isValidDriveLetter(newDrive)) {                 // not a valid drive letter?
            return -1;
        }
		
        newDrive = toUpperCase(newDrive);                   // make sure it's upper case
        driveIndex = newDrive - 'A';                        // calculate drive index
		return driveIndex;
	}

	// if it wasn't full path, use current drive index
	return currentDriveIndex;
}

bool TranslatedDisk::isAtariPathReadOnly(std::string atariPath)
{
    int driveIndex = getDriveIndexFromAtariPath(atariPath);
    
    if(driveIndex == -1) {
        return false;
    }
    
    return isDriveIndexReadOnly(driveIndex);
}
    
bool TranslatedDisk::isDriveIndexReadOnly(int driveIndex)
{ 
    if(driveIndex < 0 || driveIndex > 15) {
        return false;
    }

    WORD mask = (1 << driveIndex);
    
    if((driveLetters.readOnly & mask) != 0) {               // if the bit representing the drive is set, it's read only
        return true;
    }
    
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

WORD TranslatedDisk::getWord(BYTE *bfr)
{
    WORD val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get lo

    return val;
}

DWORD TranslatedDisk::getDword(BYTE *bfr)
{
    DWORD val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get mid hi
    val = val << 8;

    val |= bfr[2];      // get mid lo
    val = val << 8;

    val |= bfr[3];      // get lo

    return val;
}

DWORD TranslatedDisk::get24bits(BYTE *bfr)
{
    DWORD val = 0;

    val  = bfr[0];       // get hi
    val  = val << 8;

    val |= bfr[1];      // get mid
    val  = val << 8;

    val |= bfr[2];      // get lo

    return val;
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

char *TranslatedDisk::functionCodeToName(int code)
{
    switch(code) {
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
        case BIOS_Drvmap:               return (char *)"BIOS_Drvmap";
        case BIOS_Mediach:              return (char *)"BIOS_Mediach";
        case BIOS_Getbpb:               return (char *)"BIOS_Getbpb";
        default:                        return (char *)"unknown";
    }
}


