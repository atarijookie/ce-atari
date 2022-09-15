// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <string>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <errno.h>
#include <utime.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <linux/msdos_fs.h>

#include "../global.h"
#include "../debug.h"
#include "../utils.h"
#include "../settings.h"
#include "../settingsreloadproxy.h"
#include "acsidatatrans.h"
#include "translateddisk.h"
#include "translatedhelper.h"
#include "gemdos.h"
#include "gemdos_errno.h"

extern TFlags flags;

void TranslatedDisk::onDsetdrv(uint8_t *cmd)
{
    // Dsetdrv() sets the current GEMDOS drive and returns a bitmap of mounted drives.

    int newDrive = cmd[5];

    if(newDrive > 15) {                             // drive number out of range? not handled
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDsetdrv -- new drive index: %d (letter %c) -> out of range, FAIL", newDrive, newDrive + 'A');

        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(newDrive < 2) {                              // floppy drive selected? store current drive, but don't handle
        currentDriveLetter  = 'A' + newDrive;       // store the current drive
        currentDriveIndex   = newDrive;

        Debug::out(LOG_DEBUG, "TranslatedDisk::onDsetdrv -- new drive index: %d (letter %c) -> it's a floppy, not handled, current drive changed", newDrive, currentDriveLetter);

        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(conf[newDrive].enabled) {                    // if that drive is enabled in cosmosEx
        bool changed = false;
        if(currentDriveIndex != newDrive) {         // if the previous drive was different, mark that really changed
            changed = true;
        }

        currentDriveLetter  = 'A' + newDrive;       // store the current drive
        currentDriveIndex   = newDrive;

        uint16_t drives = getDrivesBitmap();
        dataTrans->addDataWord(drives);             // return the drives in data
        dataTrans->padDataToMul16();                // and pad to 16 bytes for DMA chip

        Debug::out(LOG_DEBUG, "TranslatedDisk::onDsetdrv -- new drive index: %d (letter %c) -> is enabled, current drive: %s", newDrive, currentDriveLetter, (changed ? "really changed" : "was already it, not changed"));

        dataTrans->setStatus(E_OK);                 // return OK
        return;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onDsetdrv -- new drive index: %d (letter %c) -> not handled", newDrive, newDrive + 'A');
    dataTrans->setStatus(E_NOTHANDLED);             // in other cases - not handled
}

void TranslatedDisk::onDgetdrv(uint8_t *cmd)
{
    // Dgetdrv() returns the current GEMDOS drive code. Drive A: is represented by
    // a return value of 0, B: by a return value of 1, and so on.

    if(conf[currentDriveIndex].enabled) {           // if we got this drive, return the current drive
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDgetdrv -- current drive is enabled");

        dataTrans->setStatus(currentDriveIndex);
        return;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onDgetdrv -- current drive not enabled, not handled");

    dataTrans->setStatus(E_NOTHANDLED);             // if we don't have this, not handled
}

void TranslatedDisk::onDsetpath(uint8_t *cmd)
{
    bool res;

    if(!conf[currentDriveIndex].enabled) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDsetpath - current Drive Index - %d, not enabled, so not handled", currentDriveIndex);
        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDsetpath - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string newAtariPath, hostPath;

    convertAtariASCIItoPc((char *) dataBuffer);     // try to fix the path with only allowed chars
    newAtariPath =        (char *) dataBuffer;

    bool        waitingForMount;
    int         atariDriveIndex, zipDirNestingLevel;
    std::string fullAtariPath;
    res = createFullAtariPathAndFullHostPath(newAtariPath, fullAtariPath, atariDriveIndex, hostPath, waitingForMount, zipDirNestingLevel);

    if(!res) {                                      // the path doesn't bellong to us?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDsetpath - newAtariPath: %s, createFullAtariPath failed!", newAtariPath.c_str());

        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    if(waitingForMount) {                           // if the path will be available in a while, but we're waiting for mount to finish now
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDsetpath -- waiting for mount, call this function again to succeed later.");

        dataTrans->setStatus(E_WAITING_FOR_MOUNT);
        return;
    }

    if(!hostPathExists(hostPath, true)) {           // path doesn't exists?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDsetpath - newAtariPath: %s, hostPathExist failed for %s", newAtariPath.c_str(), hostPath.c_str());

        dataTrans->setStatus(EPTHNF);               // path not found
        return;
    }

    //Debug::out(LOG_DEBUG, "TranslatedDisk::onDsetpath - newAtariPath: %s -> hostPath: %s", newAtariPath.c_str(), hostPath.c_str());

    if(currentDriveIndex != atariDriveIndex) {      // if we need to change the drive too
        currentDriveIndex   = atariDriveIndex;      // update the current drive index
        currentDriveLetter  = atariDriveIndex + 'A';

        Debug::out(LOG_DEBUG, "TranslatedDisk::onDsetpath - current drive changed to %c", currentDriveLetter);
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onDsetpath - newAtariPath: %s, host path: %s, fullAtariPath: %s - success", newAtariPath.c_str(), hostPath.c_str(), fullAtariPath.c_str());

    // if path exists, store it and return OK
    conf[currentDriveIndex].currentAtariPath = fullAtariPath;
    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onDgetpath(uint8_t *cmd)
{
    // Note! whichDrive 0 is the default drive, so drive numbers are +1
    int whichDrive = cmd[5];

    if(whichDrive == 0) {                           // current drive?
        whichDrive = currentDriveIndex;
    } else {                                        // the specified drive?
        whichDrive--;
    }

    if(!conf[whichDrive].enabled) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDgetpath - which drive: %d - not enabled!", whichDrive);

        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    std::string aPath = conf[whichDrive].currentAtariPath;
    pathSeparatorHostToAtari(aPath);

    Debug::out(LOG_DEBUG, "TranslatedDisk::onDgetpath - which drive: %d, atari path: %s", whichDrive, aPath.c_str());

    // return the current path for current drive
    dataTrans->addDataCString(aPath.c_str(), true); // add terminating zero, just in case
    dataTrans->setStatus(E_OK);
}

bool TranslatedDisk::buildGemdosFindstorageData(TFindStorage& fs, const std::string& hostSearchString, uint8_t findAttribs, bool isRootDir)
{
    fs.clear();                     // clear find storage

    SearchParams sp;
    DiskItem di;

    // fill the search params with correct values
    sp.internal = NULL;                     // set internal to null before 1st call
    sp.path = hostSearchString;             // path to directory + some file matching pattern (*.* or exact filename or something similar)
    sp.attribs = findAttribs;               // search only for items with these attributes
    sp.addUpDir = !isRootDir;               // will add '.' and '..' if this is NOT root dir

    // go through the dir, find all items, store them in buffer
    uint8_t* bfr = fs.buffer;
    while(ldp_findFirstAndNext(sp, di)) {   // try to find item
        if(fs.count < fs.maxCount) {        // if still can fit in the buffer
            diskItemToAtariFindStorageItem(di, bfr);    // disk item to this buffer position
            bfr += 23;                      // pointer in buffer to next item
            fs.count++;                     // count increment
        }
    }

    return true;    // when should we return false? even empty dir might be a valid result
}

void TranslatedDisk::diskItemToAtariFindStorageItem(DiskItem& di, uint8_t* buf)
{
    uint16_t atariTime = Utils::fileTimeToAtariTime(&di.datetime);
    uint16_t atariDate = Utils::fileTimeToAtariDate(&di.datetime);

    // GEMDOS File Attributes
    buf[0] = di.attribs;

    // GEMDOS Time
    buf[1] = atariTime >> 8;
    buf[2] = atariTime &  0xff;

    // GEMDOS Date
    buf[3] = atariDate >> 8;
    buf[4] = atariDate &  0xff;

    // file size with respect to GEMDOS maximum file size
    uint32_t size = (di.size > GEMDOS_FILE_MAXSIZE) ? GEMDOS_FILE_MAXSIZE : di.size;
    buf[5] = (size >>  24) & 0xff;
    buf[6] = (size >>  16) & 0xff;
    buf[7] = (size >>   8) & 0xff;
    buf[8] =  size         & 0xff;

    // Filename -- d_fname[14]
    memset(&buf[9], 0, 14);                             // first clear the mem
    strncpy((char *) &buf[9], di.name.c_str(), 14);     // copy the filename - 'FILE.C'
}

void TranslatedDisk::onFsfirst(uint8_t *cmd)
{
    bool res;

    // initialize find storage in case anything goes bad
    tempFindStorage.clear();

    //----------
    // first get the params
    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string atariSearchString, hostSearchString;

    uint32_t dta           = Utils::getDword(dataBuffer);  // bytes 0 to 3 contain address of DTA used on ST with this Fsfirst() - will be used as identifier for Fsfirst() / Fsnext()

    uint8_t findAttribs    = dataBuffer[4];                // get find attribs (dirs | hidden | ...)

    convertAtariASCIItoPc((char *) (dataBuffer + 5));   // try to fix the path with only allowed chars
    atariSearchString   = (char *) (dataBuffer + 5);    // get search string, e.g.: C:\\*.*

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFsfirst(%08x) - atari search string: %s, find attribs: 0x%02x", dta, atariSearchString.c_str(), findAttribs);

    if(LOG_DEBUG <= flags.logLevel) {                   // only when debug is enabled
        std::string atts;
        atariFindAttribsToString(findAttribs, atts);
        Debug::out(LOG_DEBUG, "find attribs: 0x%02x -> %s", findAttribs, atts.c_str());
    }

    bool        waitingForMount;
    int         atariDriveIndex, zipDirNestingLevel;
    std::string fullAtariPath;
    res = createFullAtariPathAndFullHostPath(atariSearchString, fullAtariPath, atariDriveIndex, hostSearchString, waitingForMount, zipDirNestingLevel);

    if(!res) {                                      // the path doesn't bellong to us?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFsfirst - atari search string: %s -- failed to create host path", atariSearchString.c_str());

        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    if(waitingForMount) {                           // if the path will be available in a while, but we're waiting for mount to finish now
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFsfirst -- waiting for mount, call this function again to succeed later.");

        dataTrans->setStatus(E_WAITING_FOR_MOUNT);
        return;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFsfirst - atari search string: %s, host search string: %s", atariSearchString.c_str(), hostSearchString.c_str());

    //----------
    // now get the dir translator for the right drive
    // check if this is a root directory
    std::string justPath, justSearchString;
    Utils::splitFilenameFromPath(hostSearchString, justPath, justSearchString);

    bool rootDir = isRootDir(justPath);

    bool useZipdirNotFileForThisSubDir = useZipdirNotFile;          // by default - use this useZipdirNotFile flag, if we're not nested in ZIP DIRs too deep
    if(zipDirNestingLevel >= MAX_ZIPDIR_NESTING) {                  // but if we are nested too deep, don't show ZIP files as DIRs anymore for this nested dir
        useZipdirNotFileForThisSubDir = false;
    }

    //now use the dir translator to get the dir content
    res = buildGemdosFindstorageData(tempFindStorage, hostSearchString, findAttribs, rootDir);

    if(!res) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFsfirst - host search string: %s -- failed to build gemdos find storage data", hostSearchString.c_str());

        dataTrans->setStatus(EFILNF);                               // file not found
        return;
    }

    if (tempFindStorage.count == 0) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFsfirst - host search string: %s 0 entries found", hostSearchString.c_str());

        dataTrans->setStatus(EFILNF);                               // file not found
        return;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFsfirst - host search string: %s -- found %d dir entries", hostSearchString.c_str(), tempFindStorage.count);

    //----------
    // now copy from temp findStorage to the some findStorages arrays
    int index;

    index = getFindStorageIndexByDta(dta);                              // see if we already have that DTA 

    if(index != -1) {                                                   // got the DTA, reuse it
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFsfirst - DTA %08x is in findStorage, reusing...", dta);
    } else {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFsfirst - DTA %08x not in findStorage, will try to find empty findStorage slot", dta);

        index = getEmptyFindStorageIndex();

        if(index == -1) {
            Debug::out(LOG_DEBUG, "TranslatedDisk::onFsfirst - failed to find empty storage slot!");
            dataTrans->setStatus(EFILNF);                               // file not found
            return;
        }
    }

    findStorages[index]->copyDataFromOther(&tempFindStorage);
    findStorages[index]->dta = dta;
    //----------

    dataTrans->setStatus(E_OK);                                     // OK!
}

void TranslatedDisk::onFsnext(uint8_t *cmd)
{
    uint32_t dta       = Utils::getDword(cmd + 5);                 // bytes 5 to 8   contain address of DTA used on ST with Fsfirst() - will be used as identifier for Fsfirst() / Fsnext()
    int   dirIndex  = Utils::getWord(cmd + 9);                  // bytes 9 and 10 contain the index of the item from which we should start sending data to ST

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFsnext -- DTA: %08x, dirIndex: %d", dta, dirIndex);

    int index = getFindStorageIndexByDta(dta);                  // now see if we have findStorage for this DTA
    if(index == -1) {                                           // not found?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFsnext(%08x) - the findBuffer for this DTA not found!", dta);
        dataTrans->setStatus(ENMFIL);                           // no more files!
        return;
    }

    TFindStorage *fs = findStorages[index];                     // this is the findStorage with which we will work

    int byteCount   = 512 - 2;                                  // how many bytes we have on the transfered sectors? -2 because 1st uint16_t is count of DTAs transfered
    int dtaSpace    = byteCount / 23;                           // how many DTAs we can fit in there?

    int dtaRemaining = fs->count - dirIndex;                    // calculate how many we have until the end

    if(dtaRemaining == 0) {                                     // nothing more to transfer?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFsnext(%08x) - no more DTA remaining", dta);

        fs->clear();                                            // we can clear this findStorage - you wouldn't get more from it anyway
        dataTrans->setStatus(ENMFIL);                           // no more files!
        return;
    }

    int dtaToSend = (dtaRemaining < dtaSpace) ? dtaRemaining : dtaSpace;    // we can send max. dtaSpace count of DTAs

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFsnext(%08x) - sending %d DTAs to ST", dta, dtaToSend);

    dataTrans->addDataWord(dtaToSend);                          // first word: how many DTAs we're sending

    uint32_t addr  = dirIndex * 23;                                // calculate offset from which we will start sending stuff
    uint8_t *buf   = &fs->buffer[addr];                            // and get pointer to this location

    dataTrans->addDataBfr(buf, dtaToSend * 23, true);           // now add the data to buffer

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onFsnext_last(uint8_t *cmd)                   // after last Fsnext() call this to release the findStorage
{
    uint32_t dta = Utils::getDword(cmd + 5);                       // bytes 5 to 8 contain address of DTA used on ST with Fsfirst() - will be used as identifier for Fsfirst() / Fsnext()

    int index = getFindStorageIndexByDta(dta);                  // now see if we have findStorage for this DTA
    if(index == -1) {                                           // not found?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFsnext_last(%08x) - the findBuffer for this DTA not found!", dta);
        dataTrans->setStatus(EIHNDL);                           // invalid handle
        return;
    }

    findStorages[index]->clear();                               // clear it
    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onDfree(uint8_t *cmd)
{
    int whichDrive = cmd[5];

    if(whichDrive == 0) {                           // current drive?
        whichDrive = currentDriveIndex;
    } else {                                        // the specified drive?
        whichDrive--;
    }

    if(!conf[whichDrive].enabled) {
        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    uint32_t clustersTotal     = 0;
    uint32_t clustersFree      = 0;
    uint32_t sectorsPerCluster = 0;

    struct statvfs *svfs;
    svfs = (struct statvfs *) malloc( sizeof(struct statvfs) );

    std::string pathToAnyFile = conf[whichDrive].hostRootPath + "/.";       // create path to any file in that file system

    int res = statvfs(pathToAnyFile.c_str(), svfs);                         // get filesystem info

    if(res == 0) {
        unsigned long blksize, blocks, freeblks;
        blksize     = svfs->f_bsize;
        blocks      = svfs->f_blocks;
        freeblks    = svfs->f_bfree;

        clustersFree        = freeblks;             // store count of free blocks
        clustersTotal       = blocks;               // store count of all blocks
        sectorsPerCluster   = blksize / 512;        // store count of sectors per block (cluster)

        Debug::out(LOG_DEBUG, "TranslatedDisk::onDfree - free: %lu, total: %lu, spc: %d", clustersFree, clustersTotal, (int) sectorsPerCluster);
    } else {
        Debug::out(LOG_ERROR, "TranslatedDisk::onDfree - statvfs FAIL");
    }

    free(svfs);

    if(isDriveIndexReadOnly(whichDrive)) {          // if drive is read only, then there is no free space
        clustersFree  = 0;
    }

    dataTrans->addDataDword(clustersFree);          // No. of Free Clusters
    dataTrans->addDataDword(clustersTotal);         // Clusters per Drive
    dataTrans->addDataDword(512);                   // Bytes per Sector
    dataTrans->addDataDword(sectorsPerCluster);     // Sectors per Cluster

    dataTrans->setStatus(E_OK);                     // everything OK
}

void TranslatedDisk::onDcreate(uint8_t *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    //---------------
    // find out if it's not ending with \ and thus having empty new dir name
    int len = strlen((char *) dataBuffer);

    if(len > 0) {                                           // there is some path specified?
        if(dataBuffer[len - 1] == ATARIPATH_SEPAR_CHAR) {   // if the path ends with \, then it's invalid (no new dir name specified), return PATH NOT FOUND
            dataTrans->setStatus(EPTHNF);
            return;
        }
    }

    //---------------
    // if the new directory name contains ? or *, fail with EACCDN
    if(pathContainsWildCards((char *) dataBuffer)) {
        dataTrans->setStatus(EACCDN);
        return;
    }

    //---------------
    // create absolute host path from relative atari path
    std::string newAtariPath, hostPath;

    convertAtariASCIItoPc((char *) dataBuffer);     // try to fix the path with only allowed chars
    newAtariPath =        (char *) dataBuffer;

    bool        waitingForMount;
    int         atariDriveIndex, zipDirNestingLevel;
    std::string fullAtariPath;
    res = createFullAtariPathAndFullHostPath(newAtariPath, fullAtariPath, atariDriveIndex, hostPath, waitingForMount, zipDirNestingLevel);

    if(!res) {                                      // the path doesn't bellong to us?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDcreate - newAtariPath: %s -- createFullAtariPath failed", newAtariPath.c_str());

        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    if(waitingForMount) {                           // if the path will be available in a while, but we're waiting for mount to finish now
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDcreate -- waiting for mount, call this function again to succeed later.");

        dataTrans->setStatus(E_WAITING_FOR_MOUNT);
        return;
    }

    //---------------
    // if it's read only (or inside of ZIP DIR - that's also read only), quit
    if(isDriveIndexReadOnly(atariDriveIndex) || zipDirNestingLevel > 0) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDcreate - newAtariPath: %s -- path is read only", newAtariPath.c_str());

        dataTrans->setStatus(EACCDN);
        return;
    }

    //---------------
    // if the new dir already exists, return ACCESS DENIED
    bool newPathAlreadyExists = hostPathExists(hostPath, true);
    if(newPathAlreadyExists) {
        dataTrans->setStatus(EACCDN);
        return;
    }

    //---------------
    // check if the specified new path doesn't contain some path, which doesn't exist
    std::string path, file;
    Utils::splitFilenameFromPath(hostPath, path, file);

    bool subPathForNewDirExists = hostPathExists(path);
    if(!subPathForNewDirExists) {                   // if the path before the name of new dir doesn't exist, return PATH NOT FOUND
        dataTrans->setStatus(EPTHNF);
        return;
    }

    //---------------
    // now try to create the dir
    int status = mkdir(hostPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);        // mod: 0x775

    if(status == 0) {                               // directory created?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDcreate - newAtariPath: %s -> hostPath: %s -- dir created", newAtariPath.c_str(), hostPath.c_str());

        dataTrans->setStatus(E_OK);
        return;
    }

    status = errno;

    if(status == EEXIST || status == EACCES) {      // path already exists or other access problem?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDcreate - newAtariPath: %s -> hostPath: %s -- failed to create dir", newAtariPath.c_str(), hostPath.c_str());

        dataTrans->setStatus(EACCDN);
        return;
    }

    dataTrans->setStatus(EINTRN);                   // some other error
}

void TranslatedDisk::onDdelete(uint8_t *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string newAtariPath, hostPath;

    convertAtariASCIItoPc((char *) dataBuffer);     // try to fix the path with only allowed chars
    newAtariPath =        (char *) dataBuffer;

    bool        waitingForMount;
    int         atariDriveIndex, zipDirNestingLevel;
    std::string fullAtariPath;
    res = createFullAtariPathAndFullHostPath(newAtariPath, fullAtariPath, atariDriveIndex, hostPath, waitingForMount, zipDirNestingLevel);

    if(!res) {                                      // the path doesn't bellong to us?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDdelete - newAtariPath: %s -- createFullAtariPath failed, the path doesn't bellong to us", newAtariPath.c_str());

        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    if(waitingForMount) {                           // if the path will be available in a while, but we're waiting for mount to finish now
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDdelete -- waiting for mount, call this function again to succeed later.");

        dataTrans->setStatus(E_WAITING_FOR_MOUNT);
        return;
    }

    if(isDriveIndexReadOnly(atariDriveIndex) || zipDirNestingLevel > 0) {     // if it's read only (or inside of ZIP DIR - that's also read only), quit
        Debug::out(LOG_DEBUG, "TranslatedDisk::onDdelete - newAtariPath: %s -> hostPath: %s -- path is read only", newAtariPath.c_str(), hostPath.c_str());

        dataTrans->setStatus(EACCDN);
        return;
    }

    int ires = deleteDirectoryPlain(hostPath.c_str());

    Debug::out(LOG_DEBUG, "TranslatedDisk::onDdelete - deleting directory hostPath: %s, result is %d", hostPath.c_str(), ires);

    dataTrans->setStatus(ires);
}

void TranslatedDisk::onFrename(uint8_t *cmd)
{
    bool res, res2;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string oldAtariName, newAtariName;

    convertAtariASCIItoPc((char *) dataBuffer);                                 // try to fix the path with only allowed chars
    oldAtariName =        (char *) dataBuffer;                                  // get old name

    convertAtariASCIItoPc((char *) (dataBuffer + oldAtariName.length() + 1));   // try to fix the path with only allowed chars
    newAtariName =        (char *) (dataBuffer + oldAtariName.length() + 1);    // get new name

    int         atariDriveIndexOld, atariDriveIndexNew;
    std::string fullAtariPathOld,   fullAtariPathNew;
    bool        wfm1, wfm2;
    int zipDirNestingLevel;
    std::string oldHostName, newHostName;
    res  = createFullAtariPathAndFullHostPath(oldAtariName, fullAtariPathOld, atariDriveIndexOld, oldHostName, wfm1, zipDirNestingLevel);
    res2 = createFullAtariPathAndFullHostPath(newAtariName, fullAtariPathNew, atariDriveIndexNew, newHostName, wfm2, zipDirNestingLevel);

    if(wfm1) {                                                          // if the path will be available in a while, but we're waiting for mount to finish now
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFrename -- waiting for mount, call this function again to succeed later.");

        dataTrans->setStatus(E_WAITING_FOR_MOUNT);
        return;
    }

    if(!res || !res2) {                                             // the path doesn't bellong to us?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFrename - failed to createFullAtariPath for %s or %s", oldAtariName.c_str(), newAtariName.c_str());

        dataTrans->setStatus(E_NOTHANDLED);                         // if we don't have this, not handled
        return;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFrename - rename %s to %s", oldHostName.c_str(), newHostName.c_str());

    if(isDriveIndexReadOnly(atariDriveIndexOld) || zipDirNestingLevel > 0) {                  // if it's read only (or inside of ZIP DIR - that's also read only), quit
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFrename - it's read only");

        dataTrans->setStatus(EACCDN);
        return;
    }

    if(!hostPathExists(oldHostName, true)) {                        // old path does not exist? fail, NOT FOUND
        dataTrans->setStatus(EFILNF);
        return;
    }

    int ires = rename(oldHostName.c_str(), newHostName.c_str());    // rename host file

    if(ires == 0) {                                                 // good
        dataTrans->setStatus(E_OK);
    } else {                                                        // error
        dataTrans->setStatus(EACCDN);
    }
}

void TranslatedDisk::onFdelete(uint8_t *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string newAtariPath, hostPath;

    convertAtariASCIItoPc((char *) dataBuffer);     // try to fix the path with only allowed chars
    newAtariPath =        (char *) dataBuffer;

    bool        waitingForMount;
    int         atariDriveIndex, zipDirNestingLevel;
    std::string fullAtariPath;
    res = createFullAtariPathAndFullHostPath(newAtariPath, fullAtariPath, atariDriveIndex, hostPath, waitingForMount, zipDirNestingLevel);

    if(!res) {                                      // the path doesn't bellong to us?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFdelete - %s - createFullAtariPath failed", newAtariPath.c_str());

        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    if(waitingForMount) {                           // if the path will be available in a while, but we're waiting for mount to finish now
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFdelete  -- waiting for mount, call this function again to succeed later.");

        dataTrans->setStatus(E_WAITING_FOR_MOUNT);
        return;
    }

    if(isDriveIndexReadOnly(atariDriveIndex) || zipDirNestingLevel > 0) {     // if it's read only (or inside of ZIP DIR - that's also read only), quit
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFdelete - %s - it's read only", newAtariPath.c_str());

        dataTrans->setStatus(EACCDN);
        return;
    }

    //----------
    // File does not exist? fail, NOT FOUND
    if(!hostPathExists(hostPath, true)) {
        dataTrans->setStatus(EFILNF);
        return;
    }

    //----------
    // check if not trying to delete directory using Fdelete
    struct stat attr;
    res = stat(hostPath.c_str(), &attr);                            // get the file status

    if(res != 0) {
        Debug::out(LOG_ERROR, "TranslatedDisk::onFdelete() -- stat(%s) failed", hostPath.c_str());
        dataTrans->setStatus(EINTRN);
        return;
    }

    bool isDir = (S_ISDIR(attr.st_mode) != 0);                      // check if it's a directory

    if(isDir) {                                                     // if it's a directory, return FILE NOT FOUND
        Debug::out(LOG_ERROR, "TranslatedDisk::onFdelete() -- can't use Fdelete() to delete a directory!");
        dataTrans->setStatus(EFILNF);
        return;
    }

    //----------
    res = unlink(hostPath.c_str());

    if(res == 0) {                                  // file deleted?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFdelete - %s - deleted, success", hostPath.c_str());

        dataTrans->setStatus(E_OK);
        return;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFdelete - %s - unlink failed", hostPath.c_str());

    int err = errno;

    if(err == ENOENT) {                             // file not found?
        dataTrans->setStatus(EFILNF);
        return;
    }

    if(err == EPERM || err == EACCES) {             // access denied?
        dataTrans->setStatus(EACCDN);
        return;
    }

    dataTrans->setStatus(EINTRN);                   // some other error
}

void TranslatedDisk::onFattrib(uint8_t *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string atariName, hostName;

    bool setNotInquire  = dataBuffer[0];
    uint8_t attrAtariNew   = dataBuffer[1];

    convertAtariASCIItoPc((char *) (dataBuffer + 2));   // try to fix the path with only allowed chars
    atariName =           (char *) (dataBuffer + 2);    // get file name

    bool        waitingForMount;
    int         atariDriveIndex, zipDirNestingLevel;
    std::string fullAtariPath;
    res = createFullAtariPathAndFullHostPath(atariName, fullAtariPath, atariDriveIndex, hostName, waitingForMount, zipDirNestingLevel);

    if(!res) {                                                      // the path doesn't bellong to us?
        dataTrans->setStatus(E_NOTHANDLED);                         // if we don't have this, not handled
        return;
    }

    if(waitingForMount) {                           // if the path will be available in a while, but we're waiting for mount to finish now
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFattrib -- waiting for mount, call this function again to succeed later.");

        dataTrans->setStatus(E_WAITING_FOR_MOUNT);
        return;
    }

    uint8_t    oldAttrAtari;

    // first read the attributes
    struct stat attr;
    res = stat(hostName.c_str(), &attr);                            // get the file status

    if(res != 0) {
        Debug::out(LOG_ERROR, "TranslatedDisk::onFattrib() -- stat(%s) failed", hostName.c_str());
        dataTrans->setStatus(EINTRN);
        return;
    }

    bool isDir = (S_ISDIR(attr.st_mode) != 0);                      // check if it's a directory

    bool hostIsFAT = false;
    bool isReadOnly = false;
    // TODO: checking of read only for file

    Utils::attributesHostToAtari(isReadOnly, isDir, oldAttrAtari);
    {
        int fd = open(hostName.c_str(), O_RDONLY);
        if(fd >= 0) {
            __u32 dosattrs = 0;
            if(ioctl(fd, FAT_IOCTL_GET_ATTRIBUTES, &dosattrs) >= 0) {
                hostIsFAT = true;   // success, that means the underlying FS is FAT
                if(dosattrs & ATTR_RO) oldAttrAtari |= FA_READONLY;
                if(dosattrs & ATTR_HIDDEN) oldAttrAtari |= FA_HIDDEN;
                if(dosattrs & ATTR_SYS) oldAttrAtari |= FA_SYSTEM;
                //if(dosattrs & ATTR_ARCH) oldAttrAtari |= FA_ARCHIVE;
            }
            close(fd);
        }
    }

    if(setNotInquire) {     // SET attribs? - SET FAT Attributes!
        if(hostIsFAT) {
            int fd = open(hostName.c_str(), O_RDWR);
            if(fd < 0) {
                Debug::out(LOG_ERROR, "TranslatedDisk::onFattrib() -- setting attributes open(%s) failed", hostName.c_str());
                dataTrans->setStatus(EACCDN);
                return;
            }
            __u32 dosattrs = ATTR_NONE;
            if(attrAtariNew & FA_READONLY) dosattrs |= ATTR_RO;
            if(attrAtariNew & FA_HIDDEN) dosattrs |= ATTR_HIDDEN;
            if(attrAtariNew & FA_SYSTEM) dosattrs |= ATTR_SYS;
            if(attrAtariNew & FA_VOLUME) dosattrs |= ATTR_VOLUME;
            if(attrAtariNew & FA_DIR) dosattrs |= ATTR_DIR;
            if(attrAtariNew & FA_ARCHIVE) dosattrs |= ATTR_ARCH;
            if(ioctl(fd, FAT_IOCTL_SET_ATTRIBUTES, &dosattrs) < 0) {
                Debug::out(LOG_ERROR, "TranslatedDisk::onFattrib() -- ioctl(%s, FAT_IOCTL_SET_ATTRIBUTES, %x) failed errno %d", hostName.c_str(), (int)dosattrs, errno);
            } else {
                Debug::out(LOG_DEBUG, "TranslatedDisk::onFattrib() -- FAT attributes %x set %s", (int)dosattrs, hostName.c_str());
            }
            close(fd);
        }
    /*
        attributesAtariToHost(attrAtariNew, attrHost);

        res = SetFileAttributesA(hostName.c_str(), attrHost);

        if(!res) {                              // failed to set attribs?
            dataTrans->setStatus(EACCDN);
            return;
        }
    */
    }

    // for GET: returns current attribs, for SET: returns old attribs
    dataTrans->setStatus(oldAttrAtari);         // return attributes
}

// notes to Fcreate on TOS 2.06
// 1st  handle returned:  6
// last handle returned: 45
// Calling fdup eats some handles, so then the 1st handle starts at higher number, but still ends up on 45
// On the atari side we could convert CosmosEx handles from 0-40 to 100-140 (or similar) to identify CosmosEx handles

void TranslatedDisk::onFcreate(uint8_t *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    if(pathContainsWildCards((char *) (dataBuffer + 1))) {          // if the new filename name contains ? or *, fail with EACCDN
        dataTrans->setStatus(EACCDN);
        return;
    }

    uint8_t attribs = dataBuffer[0];

    std::string atariName, hostName;

    convertAtariASCIItoPc((char *) (dataBuffer + 1));               // try to fix the path with only allowed chars
    atariName =           (char *) (dataBuffer + 1);                // get file name

    bool        waitingForMount;
    int         atariDriveIndex, zipDirNestingLevel;
    std::string fullAtariPath;
    res = createFullAtariPathAndFullHostPath(atariName, fullAtariPath, atariDriveIndex, hostName, waitingForMount, zipDirNestingLevel);

    if(!res) {                                                      // the path doesn't bellong to us?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFcreate - %s - createFullAtariPath failed", atariName.c_str());

        dataTrans->setStatus(E_NOTHANDLED);                         // if we don't have this, not handled
        return;
    }

    if(waitingForMount) {                           // if the path will be available in a while, but we're waiting for mount to finish now
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFcreate -- waiting for mount, call this function again to succeed later.");

        dataTrans->setStatus(E_WAITING_FOR_MOUNT);
        return;
    }

    if(isDriveIndexReadOnly(atariDriveIndex) || zipDirNestingLevel > 0) {                     // if it's read only (or inside of ZIP DIR - that's also read only), quit
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFcreate - %s - it's read only", atariName.c_str());

        dataTrans->setStatus(EACCDN);
        return;
    }

    int index = findEmptyFileSlot();

    if(index == -1) {                                               // no place for new file? No more handles.
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFcreate - %s - no empty slot", atariName.c_str());

        dataTrans->setStatus(ENHNDL);
        return;
    }

    // create file and close it
    FILE *f = fopen(hostName.c_str(), "wb+");                       // write/update - create empty / truncate existing

    if(!f) {
        int error = errno;
        int status;

        switch(error) {
            case ENFILE:
            case EMFILE:
                status = ENHNDL;                       // no more file handles left
                break;
            case ENOENT:
                status = EPTHNF;                       // Access path is incorrect
                break;
            default:
                status = EACCDN;                       // if failed to create, access error
        }

        Debug::out(LOG_DEBUG, "TranslatedDisk::onFcreate - %s - fopen failed : %s (ERROR %d)", hostName.c_str(), strerror(errno), status);
        dataTrans->setStatus(status);
        return;
    }

    // now set it's attributes
    int fd = fileno(f);
    if(fd >= 0) {
        __u32 dosattrs = ATTR_NONE;
        if(attribs & FA_READONLY) dosattrs |= ATTR_RO;
        if(attribs & FA_HIDDEN) dosattrs |= ATTR_HIDDEN;
        if(attribs & FA_SYSTEM) dosattrs |= ATTR_SYS;
        if(attribs & FA_VOLUME) dosattrs |= ATTR_VOLUME;
        if(attribs & FA_DIR) dosattrs |= ATTR_DIR;
        if(attribs & FA_ARCHIVE) dosattrs |= ATTR_ARCH;
        if(ioctl(fd, FAT_IOCTL_SET_ATTRIBUTES, &dosattrs) < 0) {
            Debug::out(LOG_ERROR, "TranslatedDisk::onFcreate -- failed to set (FAT) files attributes on %s", hostName.c_str());
        } else {
            Debug::out(LOG_DEBUG, "TranslatedDisk::onFcreate -- attributes %x set on %s", (int)dosattrs, hostName.c_str());
        }
    }

    fclose(f);


/*
    uint32_t attrHost;
    attributesAtariToHost(attribs, attrHost);

    res = SetFileAttributesA(hostName.c_str(), attrHost);

    if(!res) {                                                      // failed to set attribs?
        dataTrans->setStatus(EACCDN);
        return;
    }
*/

    // now open the file again
    f = fopen(hostName.c_str(), "rb+");                             // read/update - file must exist

    if(!f) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFcreate - %s - fopen failed for reopening", hostName.c_str());

        dataTrans->setStatus(EACCDN);                               // if failed to create, access error
        return;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFcreate - %s - success, index is: %d", hostName.c_str(), index);

    // store the params
    files[index].hostHandle     = f;
    files[index].atariHandle    = index;                            // handles 0 - 5 are reserved on Atari
    files[index].hostPath       = hostName;

    dataTrans->setStatus(files[index].atariHandle);                 // return the handle
}

void TranslatedDisk::onFopen(uint8_t *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    uint8_t mode = dataBuffer[0];

    std::string atariName, hostName;

    convertAtariASCIItoPc((char *) (dataBuffer + 1));               // try to fix the path with only allowed chars
    atariName =           (char *) (dataBuffer + 1);                // get file name

    bool        waitingForMount;
    int         atariDriveIndex, zipDirNestingLevel;
    std::string fullAtariPath;
    res = createFullAtariPathAndFullHostPath(atariName, fullAtariPath, atariDriveIndex, hostName, waitingForMount, zipDirNestingLevel);

    if(!res) {                                                      // the path doesn't bellong to us?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFopen - %s - createFullAtariPath failed", atariName.c_str());

        dataTrans->setStatus(E_NOTHANDLED);                         // if we don't have this, not handled
        return;
    }

    if(waitingForMount) {                           // if the path will be available in a while, but we're waiting for mount to finish now
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFopen -- waiting for mount, call this function again to succeed later.");

        dataTrans->setStatus(E_WAITING_FOR_MOUNT);
        return;
    }

    if((mode & 0x07) != 0 && (isDriveIndexReadOnly(atariDriveIndex) || zipDirNestingLevel > 0)) {      // if it's WRITE mode and read only (or inside of ZIP DIR - that's also read only), quit
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFopen - %s - fopen mode is write, but file is read only, fail! ", atariName.c_str());

        dataTrans->setStatus(EACCDN);
        return;
    }

    int index = findEmptyFileSlot();

    if(index == -1) {                                               // no place for new file? No more handles.
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFopen - %s - no empty slot, failed", atariName.c_str());

        dataTrans->setStatus(ENHNDL);
        return;
    }

    // opening for S_WRITE doesn't truncate file, but allows changing content (tested with PRGFLAGS.PRG)
    const char *fopenMode;

    const char *mode_S_READ       = "rb";
    const char *mode_S_WRITE      = "rb+";
    const char *mode_S_READWRITE  = "rb+";

    mode = mode & 0x07;         // leave only lowest 3 bits

    bool justRead = false;

    switch(mode) {
        case 0:     fopenMode = mode_S_READ;        justRead = true;    break;
        case 1:     fopenMode = mode_S_WRITE;                           break;
        case 2:     fopenMode = mode_S_READWRITE;                       break;
        default:    fopenMode = mode_S_READ;        justRead = true;    break;
    }

    if(justRead) {                              // if we should just read from the file (not write and thus create if does not exist)
        if(!hostPathExists(hostName, true)) {   // and the file does not exist
            Debug::out(LOG_DEBUG, "TranslatedDisk::onFopen - %s - fopen mode is just read, but the file does not exist, failed!", hostName.c_str());

            dataTrans->setStatus(EFILNF);       // quit with FILE NOT FOUND
            return;
        }
    }

    // create file and close it
    FILE *f = fopen(hostName.c_str(), fopenMode);                   // open according to required mode

    if(!f) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFopen - %s - fopen failed!", hostName.c_str());

        dataTrans->setStatus(EACCDN);                               // if failed to create, access error
        return;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFopen - %s - success, index is %d", hostName.c_str(), index);

    // store the params
    files[index].hostHandle     = f;
    files[index].atariHandle    = index;                            // handles 0 - 5 are reserved on Atari
    files[index].hostPath       = hostName;

    dataTrans->setStatus(files[index].atariHandle);                 // return the handle
}

void TranslatedDisk::onFclose(uint8_t *cmd)
{
    int handle = cmd[5];

    int index = findFileHandleSlot(handle);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFclose - handle %d not found, not handled", handle);

        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    bool res = dataTrans->recvData(dataBuffer, 16);                 // get data from Hans -- this is now here just to tell the whole software chain that this is a DMA WRITE operation (no real data needed)

    if(!res) {                                                      // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFclose - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFclose - closing handle %d (index %d)", handle, index);

    fclose(files[index].hostHandle);                                // close the file

    files[index].hostHandle     = NULL;                             // clear the struct
    files[index].atariHandle    = EIHNDL;
    files[index].hostPath       = "";

    dataTrans->setStatus(E_OK);                                     // ok!
}

void TranslatedDisk::onFdatime(uint8_t *cmd)
{
    int param       = cmd[5];
    int handle      = param & 0x7f;             // lowest 7 bits
    int setNotGet   = param >> 7;               // highest bit

    uint16_t atariTime = 0, atariDate = 0;

    atariTime       = Utils::getWord(cmd + 6);         // retrieve the time and date from command from ST
    atariDate       = Utils::getWord(cmd + 8);

    int index = findFileHandleSlot(handle);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(setNotGet) {                                                 // on SET
        tm          timeStruct;
        time_t      timeT;
        utimbuf     uTimBuf;

        Utils::fileDateTimeToHostTime(atariDate, atariTime, &timeStruct);   // convert atari date and time to struct tm
        //timeT = timelocal(&timeStruct);                               // convert tm to time_t
        timeT = mktime(&timeStruct);                                // convert tm to time_t
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFdatime %hu %hu => %d (%04d-%02d-%02d %02d:%02d:%02d)",
                   atariTime, atariDate, timeT, timeStruct.tm_year+1900, timeStruct.tm_mon+1, timeStruct.tm_mday,
                   timeStruct.tm_hour, timeStruct.tm_min, timeStruct.tm_sec);

        uTimBuf.actime  = timeT;                                    // store access time
        uTimBuf.modtime = timeT;                                    // store modification time

        int ires = utime(files[index].hostPath.c_str(), &uTimBuf);  // try to set the access and modification time

        if(ires != 0) {                                             // if failed to set the date and time, fail
            dataTrans->setStatus(EINTRN);
            return;
        }
    } else {                                                        // on GET
        int res;
        struct stat attr;
        res = stat(files[index].hostPath.c_str(), &attr);           // get the file status

        if(res != 0) {
            Debug::out(LOG_ERROR, "TranslatedDisk::appendFoundToFindStorage -- stat(%s) failed", files[index].hostPath.c_str());
            dataTrans->setStatus(EINTRN);
            return;
        }

        tm *time = localtime(&attr.st_mtime);                       // convert time_t to tm structure

        uint16_t atariTime = Utils::fileTimeToAtariTime(time);
        uint16_t atariDate = Utils::fileTimeToAtariDate(time);

        dataTrans->addDataWord(atariTime);
        dataTrans->addDataWord(atariDate);
        dataTrans->padDataToMul16();
    }

    dataTrans->setStatus(E_OK);                                // ok!
}

void TranslatedDisk::onFread(uint8_t *cmd)
{
    int atariHandle         = cmd[5];
    uint32_t byteCount         = Utils::get24bits(cmd + 6);
    int seekOffset          = (signed char) cmd[9];

    int index = findFileHandleSlot(atariHandle);

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFread - atariHandle: %d, byteCount: %d, seekOffset: %d", (int) atariHandle, (int) byteCount, (int) seekOffset);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFread - atari handle %d not found, not handling", atariHandle);

        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(byteCount > (254 * 512)) {                                   // requesting to transfer more than 254 sectors at once? fail!
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFread - trying to transfer more than MAX SECTORS in one transfer (byteCount: %d)", byteCount);

        dataTrans->setStatus(EINTRN);
        return;
    }

    if(seekOffset != 0) {                                           // if we should seek before read
        int res = fseek(files[index].hostHandle, seekOffset, SEEK_CUR);

        if(res != 0) {                                              // if seek failed
            Debug::out(LOG_DEBUG, "TranslatedDisk::onFread - fseek %d failed", seekOffset);

            dataTrans->setStatus(EINTRN);
            return;
        }
    }

    int mod = byteCount % 16;       // how many we got in the last 1/16th part?
    int pad = 0;

    if(mod != 0) {
        pad = 16 - mod;             // how many we need to add to make count % 16 equal to 0?
    }

    uint32_t transferSizeBytes = byteCount + pad;

    uint32_t cnt = fread (dataBuffer, 1, transferSizeBytes, files[index].hostHandle);
    dataTrans->addDataBfr(dataBuffer, cnt, false);  // then store the data
    dataTrans->padDataToMul16();

    files[index].lastDataCount = cnt;                       // store how much data was read

    if(cnt == byteCount) {                                  // if we read data count as requested
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFread - all %d bytes transfered", byteCount);

        dataTrans->setStatus(RW_ALL_TRANSFERED);
        return;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFread - only %d bytes out of %d bytes transfered", cnt, byteCount);

    dataTrans->setStatus(RW_PARTIAL_TRANSFER);
}

void TranslatedDisk::onFwrite(uint8_t *cmd)
{
    int atariHandle         = cmd[5];
    uint32_t byteCount         = Utils::get24bits(cmd + 6);

    int index = findFileHandleSlot(atariHandle);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFwrite - atariHandle %d not found, not handled", atariHandle);

        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(byteCount > (254 * 512)) {                                   // requesting to transfer more than 254 sectors at once? fail!
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFwrite - trying to transfer more than MAX SECTORS count of bytes (%d)", byteCount);

        dataTrans->setStatus(EINTRN);
        return;
    }

    int mod = byteCount % 16;       // how many we got in the last 1/16th part?
    int pad = 0;

    if(mod != 0) {
        pad = 16 - mod;             // how many we need to add to make count % 16 equal to 0?
    }

    uint32_t transferSizeBytes = byteCount + pad;

    bool res;
    res = dataTrans->recvData(dataBuffer, transferSizeBytes);   // get data from Hans

    if(!res) {                                                  // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFwrite - failed to get data from Hans");

        dataTrans->setStatus(EINTRN);
        return;
    }

    uint32_t bWritten = fwrite(dataBuffer, 1, byteCount, files[index].hostHandle);    // write the data

    files[index].lastDataCount = bWritten;                      // store data written count

    if(bWritten != byteCount) {                                 // when didn't write all the data
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFwrite - didn't write all data - only %d bytes out of %d were written", bWritten, byteCount);

        dataTrans->setStatus(RW_PARTIAL_TRANSFER);
        return;
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFwrite - all %d bytes were written", bWritten);
    dataTrans->setStatus(RW_ALL_TRANSFERED);                    // when all the data was written
}

void TranslatedDisk::onRWDataCount(uint8_t *cmd)                   // when Fread / Fwrite doesn't process all the data, this returns the count of processed data
{
    int atariHandle = cmd[5];
    int index       = findFileHandleSlot(atariHandle);

    if(index == -1) {                                           // handle not found? not handled, try somewhere else
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    dataTrans->addDataDword(files[index].lastDataCount);
    dataTrans->padDataToMul16();

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onFseek(uint8_t *cmd)
{
    // get seek params
    uint32_t   offset      = Utils::getDword(cmd + 5);
    uint8_t    atariHandle = cmd[9];
    uint8_t    seekMode    = cmd[10];

    int index = findFileHandleSlot(atariHandle);

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFseek - atariHandle: %d, offset: %d, seekMode: %d", (int) atariHandle, (int) offset, (int) seekMode);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFseek - atariHandle %d not found, not handled", atariHandle);

        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    int hostSeekMode = SEEK_SET;
    switch(seekMode) {
        case 0: hostSeekMode = SEEK_SET; break;
        case 1: hostSeekMode = SEEK_CUR; break;
        case 2: hostSeekMode = SEEK_END; break;
    }

    int iRes = fseek(files[index].hostHandle, offset, hostSeekMode);

    if(iRes != 0) {                         // on ERROR
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFseek - fseek %d, %d failed", offset, hostSeekMode);

        dataTrans->setStatus(EINTRN);
        return;
    }

    /* now for the atari specific stuff - return current file position */
    int pos = ftell(files[index].hostHandle);                       // get stream position

    if(pos == -1) {                                                 // failed to get position?
        Debug::out(LOG_DEBUG, "TranslatedDisk::onFseek - ftell failed");

        dataTrans->setStatus(EINTRN);
        return;
    }

    uint32_t bytesToEnd = getByteCountToEOF(files[index].hostHandle);  // get count of bytes to EOF

    Debug::out(LOG_DEBUG, "TranslatedDisk::onFseek - ok, current position is %d, and we got %d bytes to end of file", pos, (int) bytesToEnd);

    dataTrans->addDataDword(pos);                                   // return the position in file
    dataTrans->addDataDword(bytesToEnd);                            // also byte count to end of file
    dataTrans->padDataToMul16();

    dataTrans->setStatus(E_OK);                                     // OK!
}

void TranslatedDisk::onFtell(uint8_t *cmd)
{
    int handle = cmd[5];

    int index = findFileHandleSlot(handle);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    int pos = ftell(files[index].hostHandle);                       // get stream position

    if(pos == -1) {                                                 // failed to get position?
        dataTrans->setStatus(EINTRN);
        return;
    }

    dataTrans->addDataDword(pos);                                   // return the position padded with zeros
    dataTrans->padDataToMul16();

    dataTrans->setStatus(E_OK);                                     // OK!
}

int TranslatedDisk::findEmptyFileSlot(void)
{
    for(int i=0; i<MAX_FILES; i++) {
        if(files[i].hostHandle == 0) {
            return i;
        }
    }

    return -1;
}

int TranslatedDisk::findFileHandleSlot(int atariHandle)
{
    for(int i=0; i<MAX_FILES; i++) {
        if(files[i].atariHandle == atariHandle) {
            return i;
        }
    }

    return -1;
}

// BIOS functions we need to support
void TranslatedDisk::onDrvMap(uint8_t *cmd)
{
    uint16_t drives = getDrivesBitmap();

    if(flags.logLevel >= LOG_DEBUG) {
        char tmp2[3];
        memset(tmp2, 0, 3);

        char tmp[128];
        strcpy(tmp, "TranslatedDisk::onDrvMap -- translated drives: ");
        for(int i=2; i<16; i++) {
            if(drives & (1 << i)) {
                tmp2[0] = 'A' + i;
                strcat(tmp, tmp2);
            }
        }

        Debug::out(LOG_DEBUG, tmp);
    }

    dataTrans->addDataWord(drives);         // drive bits
    dataTrans->addDataWord(0);              // add empty uint16_t - for future extension to 32 drives

    dataTrans->padDataToMul16();            // pad to multiple of 16

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onMediach(uint8_t *cmd)
{
    uint16_t mediach = 0;
    int chgCount = 0;

    for(int i=2; i<MAX_DRIVES; i++) {       // create media changed bits
        if(conf[i].enabled && conf[i].mediaChanged) {
            mediach |= (1 << i);            // set the bit

            Debug::out(LOG_DEBUG, "TranslatedDisk::onMediach() - drive %c: changed", 'A' + i);
            chgCount++;
        }
    }

    Debug::out(LOG_DEBUG, "TranslatedDisk::onMediach() - drives changed count: %d", chgCount);

    dataTrans->addDataWord(mediach);
    dataTrans->padDataToMul16();            // pad to multiple of 16

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onGetbpb(uint8_t *cmd)
{
    uint16_t drive = cmd[5];

    if(drive >= MAX_DRIVES || !conf[drive].enabled) {   // index would be out of range, or drive not enabled?
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    conf[drive].mediaChanged = false;                   // mark as media not changed
    onPexec_getBpb(cmd);                                // let just always use Pexec() RAW drive BPB
}

void TranslatedDisk::atariFindAttribsToString(uint8_t attr, std::string &out)
{
    out = "";

    if(attr & FA_READONLY)  out += "FA_READONLY ";
    if(attr & FA_HIDDEN)    out += "FA_HIDDEN ";
    if(attr & FA_SYSTEM)    out += "FA_SYSTEM ";
    if(attr & FA_VOLUME)    out += "FA_VOLUME ";
    if(attr & FA_DIR)       out += "FA_DIR ";
    if(attr & FA_ARCHIVE)   out += "FA_ARCHIVE ";

    if(out.empty()) {
        out = "(none)";
    }
}

void TranslatedDisk::getByteCountToEndOfFile(uint8_t *cmd)
{
    int atariHandle = cmd[5];

    int index = findFileHandleSlot(atariHandle);

    if(index == -1) {                               // handle not found? not handled, try somewhere else
        Debug::out(LOG_DEBUG, "TranslatedDisk::getByteCountToEndOfFile - atari handle %d not found, not handling", atariHandle);

        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    FILE *f = files[index].hostHandle;              // store the handle to 'f'

    uint32_t bytesToEnd = getByteCountToEOF(f);

    //-----------
    // now send it to ST
    Debug::out(LOG_DEBUG, "TranslatedDisk::getByteCountToEndOfFile - for atari handle %d - there are %d bytes until the end of file", atariHandle, (int) bytesToEnd);

    dataTrans->addDataDword(bytesToEnd);            // store the byte count to buffer
    dataTrans->padDataToMul16();

    dataTrans->setStatus(E_OK);
}

uint32_t TranslatedDisk::getByteCountToEOF(FILE *f)
{
    if(!f) {
        return 0;
    }

    // find out and calculate, how many bytes there are until the end of file from current position
    uint32_t posCurrent, posEnd, bytesToEnd;

    posCurrent = ftell(f);                          // store current position from start
    fseek(f, 0, SEEK_END);                          // move to the end of file
    posEnd = ftell(f);                              // store the position of the end of file
    fseek(f, posCurrent, SEEK_SET);                 // restore the position to what it was before

    bytesToEnd = posEnd - posCurrent;               // calculate how many bytes there are until the end of file

    return bytesToEnd;
}

void TranslatedDisk::onTestRead(uint8_t *cmd)
{
    int     byteCount   = Utils::get24bits(cmd + 5);    // 5,6,7 -- byte count
    uint16_t    xorVal      = Utils::getWord(cmd + 8);      // 8,9   -- xor value

    byteCount = (byteCount < ACSI_MAX_TRANSFER_SIZE_BYTES) ? byteCount : ACSI_MAX_TRANSFER_SIZE_BYTES;      // if would try to send too much data (that would cause a crash), limit it

    int i;
    uint16_t counter = 0;
    for(i=0; i<byteCount; i += 2) {
        dataTrans->addDataWord(counter ^ xorVal);       // store word
        counter++;
    }

    if(byteCount & 1) {                                 // odd number of bytes? add last byte
        uint8_t lastByte = (counter ^ xorVal) >> 8;
        dataTrans->addDataByte(lastByte);
    }

    dataTrans->padDataToMul16();
    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onTestWrite(uint8_t *cmd)
{
    int     byteCount   = Utils::get24bits(cmd + 5);    // 5,6,7 -- byte count
    uint16_t    xorVal      = Utils::getWord(cmd + 8);      // 8,9   -- xor value

    byteCount = (byteCount < ACSI_MAX_TRANSFER_SIZE_BYTES) ? byteCount : ACSI_MAX_TRANSFER_SIZE_BYTES;      // if would try to send too much data (that would cause a crash), limit it

    bool res = dataTrans->recvData(dataBuffer, byteCount);   // get data from Hans

    if(!res) {
        Debug::out(LOG_DEBUG, "TranslatedDisk::onTestWrite - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }

    int i;
    uint16_t counter = 0;
    for(i=0; i<byteCount; i += 2) {                     // verify all data words
        uint16_t valSt  = Utils::getWord(dataBuffer + i);

        uint16_t valGen = counter ^ xorVal;
        counter++;

        if(valSt != valGen) {                           // data mismatch? fail
            Debug::out(LOG_DEBUG, "TranslatedDisk::onTestWrite - data check failed on byte %d out of %d, values: %04X != %04X", i, byteCount, valSt, valGen);

            int iFail = i;
            int start = MAX(i - 6, 0);                  // go 6 bytes back before error, if it would be bellow index 0, just use 0

            for(i=start; i<(start + 12); i += 2) {      // put 6 uint16_t values into log to see them
                uint16_t valSt  = Utils::getWord(dataBuffer + i);
                Debug::out(LOG_DEBUG, "  [%d] %04X %s", i, valSt, (i == iFail) ? "<--- BAD" : "");
            }

            dataTrans->setStatus(E_CRC);
            return;
        }
    }

    if(byteCount & 1) {                                 // odd number of bytes? verify last byte
        uint8_t lastByteGen = (counter ^ xorVal) >> 8;
        uint8_t lastByteSt  = dataBuffer[i];

        if(lastByteSt != lastByteGen) {                 // data mismatch? fail
            dataTrans->setStatus(E_CRC);
            return;
        }
    }

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onTestGetACSIids(uint8_t *cmd)
{
    AcsiIDinfo  acsiIdInfo;
    Settings    s;
    s.loadAcsiIDs(&acsiIdInfo);                                 // read the list of device types from settings

    for(int id=0; id<8; id++) {                                 // now store it one after another to buffer
        dataTrans->addDataByte(acsiIdInfo.acsiIDdevType[id]);
    }

    dataTrans->addDataByte(acsiIdInfo.enabledIDbits);           // store the enabled ACSI IDs

    dataTrans->padDataToMul16();                                // pad to multiple of 16
    dataTrans->setStatus(E_OK);
}

int TranslatedDisk::findCurrentIDforDevType(int devType, AcsiIDinfo *aii)
{
    int id;

    for(id=0; id<8; id++) {
        if(aii->acsiIDdevType[id] == devType) {
            return id;
        }
    }

    return -1;
}

void TranslatedDisk::onSetACSIids(uint8_t *cmd)
{
    bool res = dataTrans->recvData(dataBuffer, 16);         // get data from Hans

    if(!res) {                                              // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "TranslatedDisk::onSetACSIids - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }

    int newId   = dataBuffer[0];                            // get new ID...
    int devType = dataBuffer[1];                            // get desired dev type

    Debug::out(LOG_DEBUG, "TranslatedDisk::onSetACSIids - setting devType %d to ID %d", devType, newId);

    AcsiIDinfo  acsiIdInfo;
    Settings    s;
    s.loadAcsiIDs(&acsiIdInfo);                             // read the list of device types from settings

    char key[32];

    if(devType == DEVTYPE_TRANSLATED || devType == DEVTYPE_SD) {        // if the device type is one of these, then disable them on their current ID, because we can have only one of them
        int currentId = findCurrentIDforDevType(devType, &acsiIdInfo);  // try to find it

        if(currentId == newId) {                            // ID not changed? quit
            dataTrans->setStatus(E_OK);
            Debug::out(LOG_DEBUG, "TranslatedDisk::onSetACSIids - ID not changed, not saving it (it's OK)");
            return;
        }

        if(currentId != -1) {                               // if it currently exists, disable it
            sprintf(key, "ACSI_DEVTYPE_%d", currentId);     // create settings KEY, e.g. ACSI_DEVTYPE_0
            s.setInt(key, DEVTYPE_OFF);

            Debug::out(LOG_DEBUG, "TranslatedDisk::onSetACSIids - the devType %d was already on ID %d, disabling it there", devType, currentId);
        }
    }

    sprintf (key, "ACSI_DEVTYPE_%d", newId);                // create settings KEY, e.g. ACSI_DEVTYPE_0
    s.setInt(key, devType);                                 // save new devType for that ID

    Debug::out(LOG_DEBUG, "TranslatedDisk::onSetACSIids - new ID %d was set to devType %d", newId, devType);
    dataTrans->setStatus(E_OK);

    if(reloadProxy) {                                       // if got settings reload proxy, invoke reload
        reloadProxy->reloadSettings(SETTINGSUSER_ACSI);
    }

    Utils::forceSync();                                     // tell system to flush the filesystem caches
}
