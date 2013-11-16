#include <string>
#include <string.h>
#include <stdio.h>
#include <io.h>

#include "../global.h"
#include "translateddisk.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "../settings.h"

extern "C" void outDebugString(const char *format, ...);

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
}

TranslatedDisk::~TranslatedDisk()
{
    delete []dataBuffer;
    delete []dataBuffer2;

    delete []findStorage.buffer;
}

void TranslatedDisk::loadSettings(void)
{
    Settings s;
    char drive1, drive2, drive3;

    drive1 = s.getChar((char *) "DRIVELETTER_FIRST",      -1);
    drive2 = s.getChar((char *) "DRIVELETTER_SHARED",     -1);
    drive3 = s.getChar((char *) "DRIVELETTER_CONFDRIVE",  -1);

    driveLetters.firstTranslated    = drive1 - 'A';
    driveLetters.shared             = drive2 - 'A';
    driveLetters.confDrive          = drive3 - 'A';
}

void TranslatedDisk::configChanged_reload(void)
{
    // first load the settings
    loadSettings();

    // now move the attached drives around to match the new configuration

    TranslatedConf tmpConf[MAX_DRIVES];
    for(int i=2; i<MAX_DRIVES; i++) {           // first make the copy of the current state
        tmpConf[i] = conf[i];
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

    // todo: attach shared and config disk when they weren't attached before and now should be
    // todo: attach remainig DOS drives when they couldn't be attached before (not enough letters before)

    outDebugString("TranslatedDisk::configChanged_reload -- attached again, good %d, bad %d", good, bad);
}

void TranslatedDisk::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

bool TranslatedDisk::attachToHostPath(std::string hostRootPath, int translatedType)
{
    int index = -1;

    if(isAlreadyAttached(hostRootPath)) {                   // if already attached, return success
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

    conf[index].enabled             = true;
    conf[index].hostRootPath        = hostRootPath;
    conf[index].currentAtariPath    = HOSTPATH_SEPAR_STRING;
    conf[index].translatedType      = translatedType;
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

    conf[index].enabled             = false;            // mark as empty
    conf[index].hostRootPath        = "";
    conf[index].currentAtariPath    = HOSTPATH_SEPAR_STRING;
    conf[index].translatedType      = TRANSLATEDTYPE_NORMAL;

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
        outDebugString("processCommand was called without valid dataTrans!");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    if(cmd[1] != 'C' || cmd[2] != 'E' || cmd[3] != HOSTMOD_TRANSLATED_DISK) {   // not for us?
        return;
    }

    // now do all the command handling
    switch(cmd[4]) {
        // special CosmosEx commands for this module
        case TRAN_CMD_IDENTIFY:
        dataTrans->addData((unsigned char *)"CosmosEx translated disk", 24, true);       // add identity string with padding
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
        case GD_CUSTOM_initialize:  onInitialize();     break;
        case GD_CUSTOM_getConfig:   onGetConfig(cmd);   break;
        case GD_CUSTOM_ftell:       onFtell(cmd);       break;

        // in other cases
        default:                                // in other cases
        dataTrans->setStatus(EINVFN);           // invalid function
        break;
    }

    dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
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

    dataTrans->addData(drives >>    8);     // drive bits first
    dataTrans->addData(drives &  0xff);

    dataTrans->padDataToMul16();            // pad to multiple of 16

    dataTrans->setStatus(E_OK);
}

bool TranslatedDisk::hostPathExists(std::string hostPath)
{
    if(!conf[currentDriveIndex].enabled) {      // we don't have this drive? fail
        return false;
    }

    // now check if it exists
    int res = _access(hostPath.c_str(), 0);

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

        if(startsWith(atariPathWithoutDrive, HOSTPATH_SEPAR_STRING)) {               // if the atari path starts with \\, remove it
            atariPathWithoutDrive = atariPathWithoutDrive.substr(1);
        }

        hostPath += atariPathWithoutDrive;                          // final path = hostPath + newPath

        removeDoubleDots(hostPath);                                 // search for '..' and simplify the path

        if(startsWith(hostPath, HOSTPATH_SEPAR_STRING)) {                            // if host path starts with a backslash, remove it
            hostPath = hostPath.substr(1);
        }

        std::string root = conf[driveIndex].hostRootPath;

        if(!endsWith(root, HOSTPATH_SEPAR_STRING)) {                                 // if the host path does not end wit \\, add it
            root += HOSTPATH_SEPAR_STRING;
        }

        hostPath = root + hostPath;

        return true;

    }

    if(!conf[currentDriveIndex].enabled) {              // we're trying this on disabled drive?
        return false;
    }

    // need to handle with \\ at the begining, without \\ at the begining, with .. at the begining

    if(startsWith(atariPath, HOSTPATH_SEPAR_STRING)) {                               // starts with \\ == starts from root
        hostPath += atariPath.substr(1);                            // final path = hostPath + newPath
        return true;
    }

    // starts without backslash? relative path then

    if(startsWith(conf[currentDriveIndex].currentAtariPath, HOSTPATH_SEPAR_STRING)) {        // add without starting backslash
        hostPath += conf[currentDriveIndex].currentAtariPath.substr(1);
    } else {
        hostPath += conf[currentDriveIndex].currentAtariPath;
    }

    if(!endsWith(hostPath, HOSTPATH_SEPAR_STRING)) {                             // should add backslash at the end?
        hostPath += HOSTPATH_SEPAR_STRING;
    }

    hostPath += atariPath;                                      // final path = hostPath + currentPath + newPath

    removeDoubleDots(hostPath);                                 // search for '..' and simplify the path

    if(startsWith(hostPath, HOSTPATH_SEPAR_STRING)) {                            // if host path starts with a backslash, remove it
        hostPath = hostPath.substr(1);
    }

    std::string root = conf[currentDriveIndex].hostRootPath;

    if(!endsWith(root, HOSTPATH_SEPAR_STRING)) {                                 // if the host path does not end wit \\, add it
        root += HOSTPATH_SEPAR_STRING;
    }

    hostPath = root + hostPath;

//    outDebugString("host path: %s", (char *) hostPath.c_str());

    return true;
}

void TranslatedDisk::removeDoubleDots(std::string &path)
{
    #define MAX_DIR_NESTING     64

//    outDebugString("removeDoubleDots before: %s", (char *) path.c_str());

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
            outDebugString("removeDoubleDots has reached maximum dir nesting level, not removing double dost!");
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

//    outDebugString("removeDoubleDots after: %s", (char *) final.c_str());
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
        outDebugString("TranslatedDisk::createAtariPathFromHostPath -- WTF, this shouldn't happen!");
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
