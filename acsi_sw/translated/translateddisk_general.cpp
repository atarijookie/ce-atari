#include <string.h>
#include <stdio.h>
#include <io.h>

#include "../global.h"
#include "translateddisk.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "settings.h"

extern "C" void outDebugString(const char *format, ...);

TranslatedDisk::TranslatedDisk(void)
{
    dataTrans = 0;

    dataBuffer  = new BYTE[BUFFER_SIZE];
    dataBuffer2 = new BYTE[BUFFER_SIZE];

    dettachAll();

    for(int i=0; i<MAX_FILES; i++) {        // initialize host file structures
        files[i].hostHandle     = NULL;
        files[i].atariHandle    = EIHNDL;
        files[i].hostPath       = "";
    }

    loadSettings();
}

TranslatedDisk::~TranslatedDisk()
{
    delete []dataBuffer;
    delete []dataBuffer2;
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

    TranslatedConf tmpConf[16];
    for(int i=2; i<16; i++) {           // first make the copy of the current state
        tmpConf[i] = conf[i];
    }

    dettachAll();                       // then deinit the conf structures

    int good = 0, bad = 0;
    bool res;

    for(int i=2; i<16; i++) {           // and now find new places
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
            conf[driveLetters.shared].enabled             = true;
            conf[driveLetters.shared].hostRootPath        = hostRootPath;
            conf[driveLetters.shared].currentAtariPath    = "\\";
            conf[driveLetters.shared].translatedType      = translatedType;

            return true;
        } else {
            return false;
        }
    }

    // are we attaching config drive?
    if(translatedType == TRANSLATEDTYPE_CONFIGDRIVE) {
        if(driveLetters.confDrive > 0) {              // we have config drive letter defined
            conf[driveLetters.confDrive].enabled             = true;
            conf[driveLetters.confDrive].hostRootPath        = hostRootPath;
            conf[driveLetters.confDrive].currentAtariPath    = "\\";
            conf[driveLetters.confDrive].translatedType      = translatedType;

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

    for(int i=start; i<16; i++) {                       // find the empty slot for the new drive
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

    conf[index].enabled             = true;
    conf[index].hostRootPath        = hostRootPath;
    conf[index].currentAtariPath    = "\\";
    conf[index].translatedType      = translatedType;

    return true;
}

bool TranslatedDisk::isAlreadyAttached(std::string hostRootPath)
{
    for(int i=0; i<16; i++) {                       // see if the specified path is already attached
        if(!conf[i].enabled) {                      // not used yet? skip
            continue;
        }

        if(conf[i].hostRootPath == hostRootPath) {    // found the matching path?
            return true;
        }
    }

    return false;
}

void TranslatedDisk::dettachAll(void)
{
    for(int i=0; i<16; i++) {               // initialize the config structs
        conf[i].enabled             = false;
        conf[i].stDriveLetter       = 'C' + i;
        conf[i].currentAtariPath    = "\\";
        conf[i].translatedType      = TRANSLATEDTYPE_NORMAL;
    }

    currentDriveLetter  = 'C';
    currentDriveIndex   = 0;
}

void TranslatedDisk::dettachFromHostPath(std::string hostRootPath)
{
    int index = -1;

    for(int i=2; i<16; i++) {                           // find where the storage the existing empty slot for the new drive
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
    conf[index].currentAtariPath    = "\\";
    conf[index].translatedType      = TRANSLATEDTYPE_NORMAL;
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

        case TRAN_CMD_GET_CONFIG:   onGetConfig(cmd);   break;

        // path functions
        case GEMDOS_Dsetdrv:        onDsetdrv(cmd);     break;
        case GEMDOS_Dgetdrv:        onDgetdrv(cmd);     break;
        case GEMDOS_Dsetpath:       onDsetpath(cmd);    break;
        case GEMDOS_Dgetpath:       onDgetpath(cmd);    break;

        // directory & file search
//        case GEMDOS_Fsetdta:        onFsetdta(cmd);     break;        // this function needs to be handled on ST only
//        case GEMDOS_Fgetdta:        onFgetdta(cmd);     break;        // this function needs to be handled on ST only
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

    for(int i=0; i<16; i++) {               // create enabled drive bits
        if(i == 0 || i == 1) {              // A and B enabled by default
            drives |= (1 << i);
        }

        if(conf[i].enabled) {
            drives |= (1 << i);             // set the bit
        }
    }

    return drives;
}

void TranslatedDisk::onGetConfig(BYTE *cmd)
{
    WORD drives = getDrivesBitmap();

    dataTrans->addData(drives >>    8);     // drive bits first
    dataTrans->addData(drives &  0xff);

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

        hostPath = conf[driveIndex].hostRootPath;

        if(!endsWith(hostPath, "\\")) {                             // if the host path does not end wit \\, add it
            hostPath += "\\";
        }

        if(startsWith(atariPathWithoutDrive, "\\")) {               // if the atari path starts with \\, remove it
            atariPathWithoutDrive = atariPathWithoutDrive.substr(1);
        }

        hostPath += atariPathWithoutDrive;                          // final path = hostPath + newPath
        return true;

    }

    // need to handle with \\ at the begining, without \\ at the begining, with .. at the begining
    hostPath = conf[currentDriveIndex].hostRootPath;

    if(!endsWith(hostPath, "\\")) {                                 // should add backslash at the end?
        hostPath += "\\";
    }

    if(atariPath[0] == '\\') {                                      // starts with \\ == starts from root
        hostPath += atariPath.substr(1);                            // final path = hostPath + newPath
        return true;
    }

    // starts without backslash? relative path then
    hostPath += conf[currentDriveIndex].currentAtariPath;

    if(!endsWith(hostPath, "\\")) {                         // should add backslash at the end?
        hostPath += "\\";
    }

    hostPath += atariPath;                                  // final path = hostPath + currentPath + newPath
    return true;
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
