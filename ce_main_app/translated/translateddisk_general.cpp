// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
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
#include "acsidatatrans.h"
#include "acsicommand/screencastacsicommand.h"
#include "acsicommand/dateacsicommand.h"
#include "settingsreloadproxy.h"
#include "translateddisk.h"
#include "translatedhelper.h"
#include "gemdos.h"
#include "gemdos_errno.h"
#include "desktopcreator.h"
#include "../display/displaythread.h"
#include "../../libdospath/libdospath.h"

extern THwConfig hwConfig;
extern InterProcessEvents events;
extern SharedObjects shared;

TranslatedDisk * TranslatedDisk::instance = NULL;

void TranslatedDisk::mutexLock(void)
{
    pthread_mutex_lock(&shared.mtxHdd);
}

void TranslatedDisk::mutexUnlock(void)
{
    pthread_mutex_unlock(&shared.mtxHdd);
}

TranslatedDisk * TranslatedDisk::getInstance(void)
{
    return instance;
}

TranslatedDisk * TranslatedDisk::createInstance(AcsiDataTrans *dt)
{
    instance = new TranslatedDisk(dt);
    return instance;
}

void TranslatedDisk::deleteInstance(void)
{
    delete instance;
    instance = NULL;
    ldp_cleanup();          // clean up the LibDosPath internals
}

TranslatedDisk::TranslatedDisk(AcsiDataTrans *dt)
{
    dataTrans = dt;

    reloadProxy = NULL;
    useZipdirNotFile = true;

    dataBuffer  = new uint8_t[ACSI_BUFFER_SIZE];
    dataBuffer2 = new uint8_t[ACSI_BUFFER_SIZE];

    pexecImage  = new uint8_t[PEXEC_DRIVE_SIZE_BYTES];
    memset(pexecImage, 0, PEXEC_DRIVE_SIZE_BYTES);

    pexecImageReadFlags = new uint8_t[PEXEC_DRIVE_SIZE_SECTORS];
    memset(pexecImageReadFlags, 0, PEXEC_DRIVE_SIZE_SECTORS);

    prgSectorStart  = 0;
    prgSectorEnd    = PEXEC_DRIVE_SIZE_SECTORS;
    pexecDriveIndex = -1;

    for(int i=0; i<MAX_FILES; i++) {        // initialize host file structures
        files[i].hostHandle     = NULL;
        files[i].atariHandle    = EIHNDL;
        files[i].hostPath       = "";
    }

    findAttachedDisks();                // find all the currently mounted disks
    initFindStorages();

    //ACSI command "date"
    dateAcsiCommand         = new DateAcsiCommand(dataTrans);

    //ACSI commands "screencast"
    screencastAcsiCommand   = new ScreencastAcsiCommand(dataTrans);

    initAsciiTranslationTable();
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
}

void TranslatedDisk::setSettingsReloadProxy(SettingsReloadProxy *rp)
{
    reloadProxy = rp;
}

void TranslatedDisk::findAttachedDisks(void)
{
    Debug::out(LOG_DEBUG, "TranslatedDisk::findAttachedDisks starting");

    std::string dirTrans = Utils::dotEnvValue("MOUNT_DIR_TRANS");    // where the translated disks are symlinked
    Utils::mergeHostPaths(dirTrans, "/X");          // add placeholder
    int len = dirTrans.length();

    for(int i=2; i<MAX_DRIVES; i++) {               // go through all the possible drives
        dirTrans[len - 1] = (char) (65 + i);        // replace placeholder / drive character with the current drive char

        if(!Utils::dirExists(dirTrans)) {           // dir doesn't exist? skip rest
            conf[i].enabled = false;                // not enabled
            continue;
        }

        // fill in device info accordingly
        conf[i].enabled = true;
        conf[i].hostRootPath = dirTrans;
        conf[i].currentAtariPath = HOSTPATH_SEPAR_STRING;
        conf[i].mediaChanged = true;
        conf[i].label = "Device Label Here";        // Utils::getDeviceLabel(devicePath);

        Debug::out(LOG_DEBUG, "TranslatedDisk::findAttachedDisks: [%d] %c: -> %s", i, i + 65, dirTrans.c_str());
    }

    Settings s;
    char driveFirst, driveShared, driveConfig;

    driveFirst = s.getChar("DRIVELETTER_FIRST",      -1);
    driveShared = s.getChar("DRIVELETTER_SHARED",    -1);
    driveConfig = s.getChar("DRIVELETTER_CONFDRIVE", 'O');

    driveLetters.firstTranslated    = driveFirst - 'A';
    driveLetters.shared             = driveShared - 'A';
    driveLetters.confDrive          = driveConfig - 'A';

    useZipdirNotFile = s.getBool("USE_ZIP_DIR", 1);
}

void TranslatedDisk::processCommand(uint8_t *cmd)
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
        case ACC_GET_MOUNTS:            onGetMounts(cmd);               break;
        case ACC_UNMOUNT_DRIVE:         onUnmountDrive(cmd);            break;
        case ST_LOG_TEXT:               onStLog(cmd);                   break;
        case ST_LOG_HTTP:               onStHttp(cmd);                  break;
        case TEST_READ:                 onTestRead(cmd);                break;
        case TEST_WRITE:                onTestWrite(cmd);               break;
        case TEST_GET_ACSI_IDS:         onTestGetACSIids(cmd);          break;
        case TEST_SET_ACSI_ID:          onSetACSIids(cmd);              break;

        // in other cases
        default:                                // in other cases
        dataTrans->setStatus(EINVFN);           // invalid function
        break;
    }

    dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
}

void TranslatedDisk::onUnmountDrive(uint8_t *cmd)
{
    int drive = cmd[5];

    if(drive < 2 || drive > 15) {               // index out of range?
        dataTrans->setStatus(EDRVNR);
        return;
    }

    Debug::out(LOG_DEBUG, "onUnmountDrive -- drive: %d, hostRootPath: %s", drive, conf[drive].hostRootPath.c_str());

    // TODO: unmount here

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onGetMounts(uint8_t *cmd)
{
    char tmp[256];
    std::string mounts;
    int index;

    static const char *trTypeStr[4] = {"", "USB drive", "shared drive", "config drive"};
    const char *mountStr;

    for(int i=2; i<MAX_DRIVES; i++) {       // create enabled drive bits
        if(conf[i].enabled) {
            index = 1;
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

uint16_t TranslatedDisk::getDrivesBitmap(void)
{
    uint16_t drives = 0;

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

    uint32_t res;
    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "TranslatedDisk::onInitialize - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }

    // get the current machine info and generate DESKTOP.INF file
    uint16_t tosVersion, curRes, drives;

    tosVersion  = Utils::getWord(dataBuffer + 0);           // 0, 1: TOS version
    curRes      = Utils::getWord(dataBuffer + 2);           // 2, 3: current screen resolution
    drives      = Utils::getWord(dataBuffer + 4);           // 4, 5: current TOS drives present

    Debug::out(LOG_DEBUG, "tosVersion: %x, drives: %04x", tosVersion, drives);

    //------------------------------------
    // depending on TOS major version determine the machine, on which this SCSI device is used, and limit the available SCSI IDs depending on that
    uint8_t tosVersionMajor = tosVersion >> 8;

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

    uint16_t translatedDrives = getDrivesBitmap();          // get bitmap of all translated drives we got

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

    DesktopCreator::createToFile(&dc);                  // create the DESKTOP.INF / NEWDESK.INF file

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onGetConfig(uint8_t *cmd)
{
    // 1st uint16_t (bytes 0, 1) - bitmap of CosmosEx translated drives
    uint16_t drives = getDrivesBitmap();
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
    uint8_t tmp[10];
    Utils::getIpAdds(tmp);

    dataTrans->addDataBfr(tmp, 10, false);                      // store it to buffer - bytes 14 to 23 (byte 14 is eth0_enabled, byte 19 is wlan0_enabled)
    //------------------

    //after sending screencast skip frameSkip frames
    int frameSkip = s.getInt ("SCREENCAST_FRAMESKIP",        20);
    dataTrans->addDataByte(frameSkip);                          // byte 24 - frame skip for screencast

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

bool TranslatedDisk::hostPathExists(std::string &hostPath, bool alsoCheckCaseInsensitive)
{
    // now check if it exists
    int res = access(hostPath.c_str(), F_OK);

    if(res != -1) {             // if it's not this error, then the file exists
        Debug::out(LOG_DEBUG, "TranslatedDisk::hostPathExists( %s ) == TRUE (file / dir exists)", hostPath.c_str());
        return true;
    }

    // if file / dir not found

    if(alsoCheckCaseInsensitive) {  // should also check case insensitive?
        std::string justPath, originalFileName, foundFileName;

        if(hostPathExists_caseInsensitive(hostPath, justPath, originalFileName, foundFileName)) { // if found this file with case insensitive search
            // update dir translator: replace originalFileName with foundFileName
            ldp_updateFileName(justPath, originalFileName, foundFileName);

            hostPath = justPath;    // justPath to hostPath, and we'll merge in new file name in the next step
            Utils::mergeHostPaths(hostPath, foundFileName);     // now the hostPath should contain newly found file
            return true;            // found this file, case insensitive
        }
    }

    // if got here, file / dir not found case sensitive and possibly case insensitive
    Debug::out(LOG_DEBUG, "TranslatedDisk::hostPathExists( %s ) == FALSE (file / dir does not exist)", hostPath.c_str());
    return false;
}

bool TranslatedDisk::hostPathExists_caseInsensitive(std::string hostPath, std::string &justPath, std::string &originalFileName, std::string &foundFileName)
{
    // clear path and filename which caller might use for found file
    foundFileName.clear();

    std::string inFileUpperCase;
    Utils::splitFilenameFromPath(hostPath, justPath, originalFileName);     // split hostpath into path and filename

    inFileUpperCase = originalFileName;
    Utils::toUpperCaseString(inFileUpperCase);          // convert the filename to upper case

    // open dir for searching
    DIR *dir = opendir(justPath.c_str());                       // try to open the dir

    if(dir == NULL) {                                           // not found?
        return false;
    }

    bool res = false;                                           // file not found yet
    std::string foundFile;
    while(1) {                                                      // while there are more files, store them
        struct dirent *de = readdir(dir);                           // read the next directory entry

        if(de == NULL) {                                            // no more entries?
            break;
        }

        if(de->d_type != DT_DIR && de->d_type != DT_REG) {          // not  a file, not a directory?
            continue;
        }

        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) { // skip special dirs
            continue;
        }

        foundFile = std::string(de->d_name);            // char * to std::string
        Utils::toUpperCaseString(foundFile);    // found file to upper case

        if(inFileUpperCase.compare(foundFile) == 0) {   // in input upper case file matches this found file
            Debug::out(LOG_DEBUG, "TranslatedDisk::hostPathExists_caseInsensitive -- found that input file %s matches file %s", originalFileName.c_str(), de->d_name);

            foundFileName = std::string(de->d_name);    // store matching filename
            res = true;                                 // we found it - success
            break;
        }
    }

    closedir(dir);
    return res;
}

bool TranslatedDisk::createFullAtariPathAndFullHostPath(const std::string &inPartialAtariPath, std::string &outFullAtariPath, int &outAtariDriveIndex, std::string &outFullHostPath, bool &waitingForMount)
{
    bool res;

    // initialize variables - in case that anything following fails
    outFullAtariPath    = "";
    outAtariDriveIndex  = -1;
    outFullHostPath     = "";
    waitingForMount     = false;

    // convert partial atari path to full atari path
    res = createFullAtariPath(inPartialAtariPath, outFullAtariPath, outAtariDriveIndex);

    if(!res) {                  // if createFullAtariPath() failed, don't do the rest
        return false;
    }

    // got full atari path, now try to create full host path
    createFullHostPath(outFullAtariPath, outAtariDriveIndex, outFullHostPath, waitingForMount);
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

bool TranslatedDisk::hasArchiveExtension(const std::string& longPath, std::string& archiveSubPath)
{
    std::string longPathCopy = longPath;                        // create a copy of the input long path
    std::transform(longPathCopy.begin(), longPathCopy.end(), longPathCopy.begin(), ::tolower);  // path copy to lowercase

    std::size_t extPos = longPathCopy.find(".zip");             // find supported extension in lowercase copy

    if(extPos != std::string::npos) {                           // extension found?
        longPathCopy = longPath;                                // create a copy of the input long path, but preserve upper / lower case this time
        archiveSubPath = longPathCopy.substr(0, extPos + 4);    // get substring containing only path to archive
        return true;            // supported archive was found
    }

    archiveSubPath.clear();
    return false;               // supported archive not found
}

void TranslatedDisk::createFullHostPath(const std::string &inFullAtariPath, int inAtariDriveIndex, std::string &outFullHostPath, bool &waitingForMount)
{
    waitingForMount     = false;

    std::string root = conf[inAtariDriveIndex].hostRootPath;        // get root path

    std::string shortPath = root;
    Utils::mergeHostPaths(shortPath, inFullAtariPath);          // short path = root + full atari path
    ldp_shortToLongPath(shortPath, outFullHostPath, true);      // short path to long path

    Debug::out(LOG_DEBUG, "TranslatedDisk::createFullHostPath - shortPath: %s -> outFullHostPath: %s", shortPath.c_str(), outFullHostPath.c_str());

    if(useZipdirNotFile) {                                      // if ZIP DIRs are enabled
        std::string archiveSubPath;
        bool hasArchive = hasArchiveExtension(outFullHostPath, archiveSubPath);     // archive in this long path?

        if(hasArchive) {                                                // it's an archive, handle it
            bool gotThisSymlink = ldp_gotSymlink(archiveSubPath);       // see if symlink for this archive was added

            if(!gotThisSymlink) {       // if don't have this symlink, we need to mount this archive
                Debug::out(LOG_DEBUG, "TranslatedDisk::createFullHostPath - will now create symlink for %s", archiveSubPath.c_str());

                // create virtual symlink in libdospath
                std::string nextMountPath = Utils::dotEnvValue("MOUNT_DIR_ZIP_FILE");   // get where the next ZIP file will be mounted
                ldp_symlink(archiveSubPath, nextMountPath);             // create virtual symlink from archive path to next mount path
                ldp_shortToLongPath(shortPath, outFullHostPath, true);  // now do the short path to long path translation again, which will apply the new symlink

                // send command to mounter to mount this archive
                std::string jsonString = "{\"cmd_name\": \"mount_zip\", \"path\": \"";
                jsonString += archiveSubPath;                           // add archiveSubPath
                jsonString += "\"}";
                Utils::sendToMounter(jsonString);                       // send to socket
            } else {                    // got this symlink already
                Debug::out(LOG_DEBUG, "TranslatedDisk::createFullHostPath - already have symlink %s", archiveSubPath.c_str());
            }
        }
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

int TranslatedDisk::getFindStorageIndexByDta(uint32_t dta)         // find the findStorage with the specified DTA
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

void TranslatedDisk::onStLog(uint8_t *cmd)
{
    uint32_t res;
    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "TranslatedDisk::onStLog - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }

    Debug::out(LOG_DEBUG, "ST log: %s", dataBuffer);
    dataTrans->setStatus(E_OK);
}

//do a simple HTTP GET request - for logging from ST to Http
//ignore result
void TranslatedDisk::onStHttp(uint8_t *cmd)
{
    uint32_t res;
    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "TranslatedDisk::onStHttp - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }

//    TDownloadRequest tdr;
//    tdr.srcUrl          = std::string((const char*)dataBuffer);
//    tdr.dstDir          = "";
//    tdr.downloadType    = DWNTYPE_LOG_HTTP;
//    tdr.checksum        = 0;                        // special case - don't check checsum
//    tdr.pStatusByte     = NULL;      // update status byte
//    Downloader::add(tdr);
//    Debug::out(LOG_DEBUG, "TranslatedDisk::onStHttp() %s", dataBuffer);

    Debug::out(LOG_DEBUG, "ST HTTP: %s", dataBuffer);
    dataTrans->setStatus(E_OK);
}


const char *TranslatedDisk::functionCodeToName(int code)
{
    switch(code) {
        case TRAN_CMD_IDENTIFY:             return "TRAN_CMD_IDENTIFY";
        case TRAN_CMD_GETDATETIME:          return "TRAN_CMD_GETDATETIME";
        case TRAN_CMD_SENDSCREENCAST:       return "TRAN_CMD_SENDSCREENCAST";
        case TRAN_CMD_SCREENCASTPALETTE:    return "TRAN_CMD_SCREENCASTPALETTE";
        case TRAN_CMD_SCREENSHOT_CONFIG:    return "TRAN_CMD_SCREENSHOT_CONFIG";
        case ST_LOG_TEXT:                   return "ST_LOG_TEXT";
        case ST_LOG_HTTP:                   return "ST_LOG_HTTP";

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

void TranslatedDisk::getScreenShotConfig(uint8_t *cmd)
{
    dataTrans->addDataByte(events.screenShotVblEnabled);

    dataTrans->addDataByte(events.doScreenShot);
    events.doScreenShot = false;                                        // unset this flag so we will send only one screenshot

    dataTrans->padDataToMul16();
    dataTrans->setStatus(E_OK);
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

    reportString = "drive";     // TODO: get drive type (config / usb / shared) from mounter
}

void TranslatedDisk::fillDisplayLines(void)
{
    char tmp[64];

    // mount USB drives as raw/translated + ZIP files are dirs/files
    Settings s;
    bool mountRawNotTrans = s.getBool("MOUNT_RAW_NOT_TRANS", 0);
    strcpy(tmp, mountRawNotTrans ? "USB raw    " : "USB trans  ");
    strcat(tmp, useZipdirNotFile ? "ZIP dir"     : "ZIP file");
    display_setLine(DISP_LINE_TRAN_SETT, tmp);

    // what letter is for config and shared drive?
    sprintf(tmp, "config:%c   shared:%c", driveLetters.confDrive + 'A', driveLetters.shared + 'A');
    display_setLine(DISP_LINE_CONF_SHAR, tmp);

    // other drives
    strcpy(tmp, "drvs: ");
    for(int i=2; i<MAX_DRIVES; i++) {   // if drive is normal and enabled
        if(conf[i].enabled) {
            char tmp2[2];
            tmp2[0] = 'A' + i;  // drive letter
            tmp2[1] = 0;        // string terminator
            strcat(tmp, tmp2);  // append
        }
    }
    display_setLine(DISP_LINE_TRAN_DRIV, tmp);
}
