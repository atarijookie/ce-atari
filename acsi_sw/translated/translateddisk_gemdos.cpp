#include <string>
#include <string.h>
#include <stdio.h>

#include "../global.h"
#include "translateddisk.h"
#include "gemdos.h"
#include "gemdos_errno.h"

extern "C" void outDebugString(const char *format, ...);

void TranslatedDisk::onDsetdrv(BYTE *cmd)
{
    // Dsetdrv() sets the current GEMDOS drive and returns a bitmap of mounted drives.

    int newDrive = cmd[5];

    if(newDrive > 15) {                             // drive number out of range? not handled
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(newDrive < 2) {                              // floppy drive selected? store current drive, but don't handle
        currentDriveLetter  = 'A' + newDrive;       // store the current drive
        currentDriveIndex   = newDrive;

        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(conf[newDrive].enabled) {                    // if that drive is enabled in cosmosEx
        currentDriveLetter  = 'A' + newDrive;       // store the current drive
        currentDriveIndex   = newDrive;

        dataTrans->setStatus(E_OK);                 // return OK
        return;
    }

    dataTrans->setStatus(E_NOTHANDLED);             // in other cases - not handled
}

void TranslatedDisk::onDgetdrv(BYTE *cmd)
{
    // Dgetdrv() returns the current GEMDOS drive code. Drive �A:� is represented by
    // a return value of 0, �B:� by a return value of 1, and so on.

    if(conf[currentDriveIndex].enabled) {           // if we got this drive, return the current drive
        dataTrans->setStatus(currentDriveIndex);
        return;
    }

    dataTrans->setStatus(E_NOTHANDLED);             // if we don't have this, not handled
}

void TranslatedDisk::onDsetpath(BYTE *cmd)
{
    bool res;

    // the path can be:
    // with \\    as first char -- that means starting at root
    // without \\ as first char -- relative to the current dir
    // with ..                  -- means one dir up

    if(!conf[currentDriveIndex].enabled) {
        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string newAtariPath, hostPath;
    newAtariPath = (char *) dataBuffer;
    res = createHostPath(newAtariPath, hostPath);

    if(!res) {                                      // the path doesn't bellong to us?
        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    if(!hostPathExists(hostPath)) {                 // path doesn't exists?
        dataTrans->setStatus(EPTHNF);               // path not found
        return;
    }

    int newDriveIndex;
    if(newPathRequiresCurrentDriveChange(newAtariPath, newDriveIndex)) {    // if we need to change the drive too
        currentDriveIndex   = newDriveIndex;                                // update the current drive index
        currentDriveLetter  = newDriveIndex + 'A';
    }

    createAtariPathFromHostPath(hostPath, newAtariPath);    // remove the host root path

    // if path exists, store it and return OK
    conf[currentDriveIndex].currentAtariPath = newAtariPath;
    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onDgetpath(BYTE *cmd)
{
    // Note! whichDrive 0 is the default drive, so drive numbers are +1
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

    std::string aPath = conf[whichDrive].currentAtariPath;
    pathSeparatorHostToAtari(aPath);

    // return the current path for current drive
    dataTrans->addData((BYTE *) aPath.c_str(), aPath.length(), true);
    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onFsfirst(BYTE *cmd)
{
    bool res;

    // initialize find storage in case anything goes bad
    findStorage.count       = 0;
    findStorage.fsnextStart = 0;

    //----------
    // first get the params
    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string atariSearchString, hostSearchString;

    BYTE findAttribs    = dataBuffer[0];
    atariSearchString   = (char *) (dataBuffer + 1);

    outDebugString("onFsfirst -- %s, findAttribs: %x", atariSearchString.c_str(), findAttribs);

    res = createHostPath(atariSearchString, hostSearchString);   // create the host path

    if(!res) {                                      // the path doesn't bellong to us?
        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }
    //----------
    // then build the found files list
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA(hostSearchString.c_str(), &found);    // find first

    if(h == INVALID_HANDLE_VALUE) {                                 // not found?
        dataTrans->setStatus(EFILNF);                               // file not found
        return;
    }

    appendFoundToFindStorage(&found, findAttribs);                  // append first found

    while(1) {                                                  // while there are more files, store them
        res = FindNextFileA(h, &found);

        if(!res) {
            break;
        }

        appendFoundToFindStorage(&found, findAttribs);          // append next found

        if(findStorage.count >= findStorage.maxCount) {         // avoid buffer overflow
            break;
        }
    }

    dataTrans->setStatus(E_OK);                                 // OK!
}

void TranslatedDisk::appendFoundToFindStorage(WIN32_FIND_DATAA *found, BYTE findAttribs)
{
    // TODO: verify on ST that the find attributes work like this

    // TOS 1.04 searches with findAttribs set to 0x10, that's INCLUDE DIRs

    outDebugString("appendFoundToFindStorage zaciatok");

    // first verify if the file attributes are OK
    if((found->dwFileAttributes & FILE_ATTRIBUTE_READONLY)!=0   && (findAttribs & FA_READONLY)==0)  // is read only, but not searching for that
        return;

    if((found->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)!=0     && (findAttribs & FA_HIDDEN)==0)    // is hidden, but not searching for that
        return;

    if((found->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)!=0     && (findAttribs & FA_SYSTEM)==0)    // is system, but not searching for that
        return;

    if((found->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0  && (findAttribs & FA_DIR)==0)       // is dir, but not searching for that
        return;

//    // this one is now disabled as on Win almost everything has archive bit set, and thus TOS didn't show any files
//    if((found->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)!=0    && (findAttribs & FA_ARCHIVE)==0)   // is archive, but not searching for that
//        return;

    //--------
    // add this file
    DWORD addr  = findStorage.count * 23;           // calculate offset
    BYTE *buf   = &findStorage.buffer[addr];        // and get pointer to this location

    BYTE atariAttribs;
    attributesHostToAtari(found->dwFileAttributes, atariAttribs);

    WORD atariTime = fileTimeToAtariTime(&found->ftLastWriteTime);
    WORD atariDate = fileTimeToAtariDate(&found->ftLastWriteTime);

    char *fileName = found->cFileName;              // use the original name

    if(strlen(found->cAlternateFileName) != 0) {    // if original is long and this is not empty, use this short one
        fileName = found->cAlternateFileName;
    }

    // GEMDOS File Attributes
    buf[0] = atariAttribs;

    // GEMDOS Time
    buf[1] = atariTime >> 8;
    buf[2] = atariTime &  0xff;

    // GEMDOS Date
    buf[3] = atariDate >> 8;
    buf[4] = atariDate &  0xff;

    // File Length
    buf[5] = found->nFileSizeLow >>  24;
    buf[6] = found->nFileSizeLow >>  16;
    buf[7] = found->nFileSizeLow >>   8;
    buf[8] = found->nFileSizeLow & 0xff;

    // Filename -- d_fname[14]
    memset(&buf[9], 0, 14);                     // first clear the mem
    strncpy((char *) &buf[9], fileName, 14);    // then copy only valid part of the string, max 14 chars

    // filename to upper case!
    for(int i=0; i<14; i++) {
        buf[9 + i] = toUpperCase(buf[9 + i]);
    }

    outDebugString("appendFoundToFindStorage - %s", &buf[9]);

    findStorage.count++;
}

void TranslatedDisk::onFsnext(BYTE *cmd)
{
    int sectorCount = cmd[5];

    int byteCount   = (sectorCount * 512) - 2;                  // how many bytes we have on the transfered sectors? -2 because 1st WORD is count of DTAs transfered
    int dtaSpace    = byteCount / 23;                           // how many DTAs we can fit in there?

    int dtaRemaining = findStorage.count - findStorage.fsnextStart;

    if(dtaRemaining == 0) {                                     // nothing more to transfer?
        dataTrans->setStatus(ENMFIL);                           // no more files!
        return;
    }

    int dtaToSend = (dtaRemaining < dtaSpace) ? dtaRemaining : dtaSpace;    // we can send max. dtaSpace count of DTAs

    dataTrans->addDataWord(dtaToSend);                          // first word: how many DTAs we're sending

    DWORD addr  = findStorage.fsnextStart * 23;                 // calculate offset from which we will start sending stuff
    BYTE *buf   = &findStorage.buffer[addr];                    // and get pointer to this location
    dataTrans->addData(buf, dtaToSend * 23, true);              // now add the data to buffer

    dataTrans->setStatus(E_OK);

    findStorage.fsnextStart += dtaToSend;                       // and move to the next position for the next fsNext
}

void TranslatedDisk::onDfree(BYTE *cmd)
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

    // TODO: get the real drive size
    DWORD clustersTotal = 32768;
    DWORD clustersFree  = 16384;

    dataTrans->addDataDword(clustersFree);          // No. of Free Clusters
    dataTrans->addDataDword(clustersTotal);         // Clusters per Drive
    dataTrans->addDataDword(512);                   // Bytes per Sector
    dataTrans->addDataDword(2);                     // Sectors per Cluster

    dataTrans->setStatus(E_OK);                     // everything OK
}

void TranslatedDisk::onDcreate(BYTE *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string newAtariPath, hostPath;
    newAtariPath = (char *) dataBuffer;

    res = createHostPath(newAtariPath, hostPath);   // create the host path

    if(!res) {                                      // the path doesn't bellong to us?
        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    res = CreateDirectoryA(hostPath.c_str(), NULL);

    if(res) {                                       // directory created?
        dataTrans->setStatus(E_OK);
        return;
    }

    DWORD err = GetLastError();

    if(err == ERROR_PATH_NOT_FOUND) {               // path not found?
        dataTrans->setStatus(EPTHNF);
        return;
    }

    if(err == ERROR_ALREADY_EXISTS) {               // path already exists?
        dataTrans->setStatus(EACCDN);
        return;
    }

    dataTrans->setStatus(EINTRN);                   // some other error
}

void TranslatedDisk::onDdelete(BYTE *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string newAtariPath, hostPath;
    newAtariPath = (char *) dataBuffer;

    res = createHostPath(newAtariPath, hostPath);   // create the host path

    if(!res) {                                      // the path doesn't bellong to us?
        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    res = RemoveDirectoryA(hostPath.c_str());

    if(res) {                                       // directory deleted?
        dataTrans->setStatus(E_OK);
        return;
    }

//    DWORD err = GetLastError();

//    if(err == ERROR_PATH_NOT_FOUND) {               // path not found?
//        dataTrans->setStatus(EPTHNF);
//        return;
//    }

//    if(err == ERROR_ALREADY_EXISTS) {               // path already exists?
//        dataTrans->setStatus(EACCDN);
//        return;
//    }

    dataTrans->setStatus(EINTRN);                   // some other error
}

void TranslatedDisk::onFrename(BYTE *cmd)
{
    bool res, res2;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string oldAtariName, newAtariName;
    oldAtariName = (char *)  dataBuffer;                                // get old name
    newAtariName = (char *) (dataBuffer + oldAtariName.length() + 1);   // get new name

    std::string oldHostName, newHostName;
    res     = createHostPath(oldAtariName, oldHostName);            // create the host path
    res2    = createHostPath(newAtariName, newHostName);            // create the host path

    if(!res || !res2) {                                             // the path doesn't bellong to us?
        dataTrans->setStatus(E_NOTHANDLED);                         // if we don't have this, not handled
        return;
    }

    res = MoveFileA(oldHostName.c_str(), newHostName.c_str());      // rename host file

    if(res) {                                                       // good
        dataTrans->setStatus(E_OK);
    } else {                                                        // error
        dataTrans->setStatus(EACCDN);
    }
}

void TranslatedDisk::onFdelete(BYTE *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string newAtariPath, hostPath;
    newAtariPath = (char *) dataBuffer;

    res = createHostPath(newAtariPath, hostPath);   // create the host path

    if(!res) {                                      // the path doesn't bellong to us?
        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    res = DeleteFileA(hostPath.c_str());

    if(res) {                                       // directory deleted?
        dataTrans->setStatus(E_OK);
        return;
    }

    DWORD err = GetLastError();

    if(err == ERROR_FILE_NOT_FOUND) {               // file not found?
        dataTrans->setStatus(EFILNF);
        return;
    }

    if(err == ERROR_ACCESS_DENIED) {                // access denied?
        dataTrans->setStatus(EACCDN);
        return;
    }

    dataTrans->setStatus(EINTRN);                   // some other error
}

void TranslatedDisk::onFattrib(BYTE *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    std::string atariName, hostName;

    bool setNotInquire  = dataBuffer[0];
    BYTE attrAtariNew   = dataBuffer[1];

    atariName = (char *)  (dataBuffer + 2);                         // get file name

    res = createHostPath(atariName, hostName);                      // create the host path

    if(!res) {                                                      // the path doesn't bellong to us?
        dataTrans->setStatus(E_NOTHANDLED);                         // if we don't have this, not handled
        return;
    }

    DWORD   attrHost;
    BYTE    oldAttrAtari;

    // first read the attributes
    attrHost = GetFileAttributesA(hostName.c_str());

    if(attrHost == INVALID_FILE_ATTRIBUTES) {   // failed to get attribs?
        dataTrans->setStatus(EACCDN);
        return;
    }

    attributesHostToAtari(attrHost, oldAttrAtari);

    if(setNotInquire) {     // SET attribs?
        attributesAtariToHost(attrAtariNew, attrHost);

        res = SetFileAttributesA(hostName.c_str(), attrHost);

        if(!res) {                              // failed to set attribs?
            dataTrans->setStatus(EACCDN);
            return;
        }
    }

    // for GET: returns current attribs, for SET: returns old attribs
    dataTrans->setStatus(oldAttrAtari);         // return attributes
}

// notes to Fcreate on TOS 2.06
// 1st  handle returned:  6
// last handle returned: 45
// Calling fdup eats some handles, so then the 1st handle starts at higher number, but still ends up on 45
// On the atari side we could convert CosmosEx handles from 0-40 to 100-140 (or similar) to identify CosmosEx handles

void TranslatedDisk::onFcreate(BYTE *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    BYTE attribs = dataBuffer[0];

    std::string atariName, hostName;
    atariName = (char *) (dataBuffer + 1);                          // get file name

    res = createHostPath(atariName, hostName);                      // create the host path

    if(!res) {                                                      // the path doesn't bellong to us?
        dataTrans->setStatus(E_NOTHANDLED);                         // if we don't have this, not handled
        return;
    }

    int index = findEmptyFileSlot();

    if(index == -1) {                                               // no place for new file? No more handles.
        dataTrans->setStatus(ENHNDL);
        return;
    }

    // create file and close it
    FILE *f = fopen(hostName.c_str(), "wb+");                       // write/update - create empty / truncate existing

    if(!f) {
        dataTrans->setStatus(EACCDN);                               // if failed to create, access error
        return;
    }

    fclose(f);

    // now set it's attributes
    DWORD attrHost;
    attributesAtariToHost(attribs, attrHost);

    res = SetFileAttributesA(hostName.c_str(), attrHost);

    if(!res) {                                                      // failed to set attribs?
        dataTrans->setStatus(EACCDN);
        return;
    }

    // now open the file again
    f = fopen(hostName.c_str(), "rb+");                             // read/update - file must exist

    if(!f) {
        dataTrans->setStatus(EACCDN);                               // if failed to create, access error
        return;
    }

    // store the params
    files[index].hostHandle     = f;
    files[index].atariHandle    = index;                            // handles 0 - 5 are reserved on Atari
    files[index].hostPath       = hostName;

    dataTrans->setStatus(files[index].atariHandle);                 // return the handle
}

void TranslatedDisk::onFopen(BYTE *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    BYTE mode = dataBuffer[0];

    std::string atariName, hostName;
    atariName = (char *) (dataBuffer + 1);                          // get file name

    res = createHostPath(atariName, hostName);                      // create the host path

    if(!res) {                                                      // the path doesn't bellong to us?
        dataTrans->setStatus(E_NOTHANDLED);                         // if we don't have this, not handled
        return;
    }

    int index = findEmptyFileSlot();

    if(index == -1) {                                               // no place for new file? No more handles.
        dataTrans->setStatus(ENHNDL);
        return;
    }

    // TODO: check if S_WRITE and S_READWRITE truncate existing file or just append to it and modify mode for following fopen

    char *fopenMode;

    char *mode_S_READ       = (char *) "rb";
    char *mode_S_WRITE      = (char *) "wb";
    char *mode_S_READWRITE  = (char *) "rb+";

    mode = mode & 0x07;         // leave only lowest 3 bits

    switch(mode) {
        case 0:     fopenMode = mode_S_READ;        break;
        case 1:     fopenMode = mode_S_WRITE;       break;
        case 2:     fopenMode = mode_S_READWRITE;   break;
        default:    fopenMode = mode_S_READ;        break;
    }

    // create file and close it
    FILE *f = fopen(hostName.c_str(), fopenMode);                   // open according to required mode

    if(!f) {
        dataTrans->setStatus(EACCDN);                               // if failed to create, access error
        return;
    }

    // store the params
    files[index].hostHandle     = f;
    files[index].atariHandle    = index;                            // handles 0 - 5 are reserved on Atari
    files[index].hostPath       = hostName;

    dataTrans->setStatus(files[index].atariHandle);                 // return the handle
}

void TranslatedDisk::onFclose(BYTE *cmd)
{
    int handle = cmd[5];

    int index = findFileHandleSlot(handle);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    fclose(files[index].hostHandle);                                // close the file

    files[index].hostHandle     = NULL;                             // clear the struct
    files[index].atariHandle    = EIHNDL;
    files[index].hostPath       = "";

    dataTrans->setStatus(E_OK);                                     // ok!
}

void TranslatedDisk::onFdatime(BYTE *cmd)
{
    int param       = cmd[5];
    int handle      = param & 0x7f;             // lowest 7 bits
    int setNotGet   = param >> 7;               // highest bit

    WORD atariTime = 0, atariDate = 0;

    atariTime       = getWord(cmd + 6);         // retrieve the time and date from command from ST
    atariDate       = getWord(cmd + 8);

    int index = findFileHandleSlot(handle);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(setNotGet) {                             // on SET
        WORD year, month, day;
        year    = (atariDate >> 9)   + 1980;
        month   = (atariDate >> 5)   & 0x0f;
        day     =  atariDate         & 0x1f;

        WORD hours, minutes, seconds;
        hours   =  (atariTime >> 11) & 0x1f;
        minutes =  (atariTime >>  5) & 0x3f;
        seconds = ((atariTime >>  5) & 0x1f) * 2;


        // TODO: setting the date / time to the file


    } else {                                    // on GET

        // TODO: retrieving date / time from the file


        atariTime |= (12                    ) << 11;        // hours
        atariTime |= (00                    ) << 5;         // minutes
        atariTime |= (30               / 2  );              // seconds

        atariDate |= (2013           - 1980) << 9;          // year
        atariDate |= (11                   ) << 5;          // month
        atariDate |= (15                   );               // day

        dataTrans->addDataWord(atariTime);
        dataTrans->addDataWord(atariDate);
        dataTrans->padDataToMul16();
    }

    dataTrans->setStatus(E_OK);                                     // ok!
}

void TranslatedDisk::onFread(BYTE *cmd)
{
    int atariHandle         = cmd[5];
    DWORD byteCount         = get24bits(cmd + 6);
    int seekOffset          = (char) cmd[9];

    int index = findFileHandleSlot(atariHandle);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(byteCount > (254 * 512)) {                                   // requesting to transfer more than 254 sectors at once? fail!
        dataTrans->setStatus(EINTRN);
        return;
    }

    if(seekOffset != 0) {                                           // if we should seek before read
        int res = fseek(files[index].hostHandle, seekOffset, SEEK_CUR);

        if(res != 0) {                                              // if seek failed
            dataTrans->setStatus(EINTRN);
            return;
        }
    }

    int mod = byteCount % 16;       // how many we got in the last 1/16th part?
    int pad = 0;

    if(mod != 0) {
        pad = 16 - mod;             // how many we need to add to make count % 16 equal to 0?
    }

    DWORD transferSizeBytes = byteCount + pad;

    DWORD cnt = fread (dataBuffer, 1, transferSizeBytes, files[index].hostHandle);
    dataTrans->addData(dataBuffer, transferSizeBytes);      // then store the data
    dataTrans->padDataToMul16();

    files[index].lastDataCount = cnt;                       // store how much data was read

    if(cnt == byteCount) {                                  // if we read data count as requested
        dataTrans->setStatus(RW_ALL_TRANSFERED);
        return;
    }

    dataTrans->setStatus(RW_PARTIAL_TRANSFER);
}

void TranslatedDisk::onFwrite(BYTE *cmd)
{
    int atariHandle         = cmd[5];
    DWORD byteCount         = get24bits(cmd + 6);

    int index = findFileHandleSlot(atariHandle);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(byteCount > (254 * 512)) {                                   // requesting to transfer more than 254 sectors at once? fail!
        dataTrans->setStatus(EINTRN);
        return;
    }

    int mod = byteCount % 16;       // how many we got in the last 1/16th part?
    int pad = 0;

    if(mod != 0) {
        pad = 16 - mod;             // how many we need to add to make count % 16 equal to 0?
    }

    DWORD transferSizeBytes = byteCount + pad;

    bool res;
    res = dataTrans->recvData(dataBuffer, transferSizeBytes);   // get data from Hans

    if(!res) {                                                  // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    DWORD bWritten = fwrite(dataBuffer, 1, byteCount, files[index].hostHandle);    // write the data

    files[index].lastDataCount = bWritten;                      // store data written count

    if(bWritten != byteCount) {                                 // when didn't write all the data
        dataTrans->setStatus(RW_PARTIAL_TRANSFER);
        return;
    }

    dataTrans->setStatus(RW_ALL_TRANSFERED);                    // when all the data was written
}

void TranslatedDisk::onRWDataCount(BYTE *cmd)                   // when Fread / Fwrite doesn't process all the data, this returns the count of processed data
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

void TranslatedDisk::onFseek(BYTE *cmd)
{
    // get seek params
    DWORD   offset      = getDword(cmd + 5);
    BYTE    atariHandle = cmd[9];
    BYTE    seekMode    = cmd[10];

    int index = findFileHandleSlot(atariHandle);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
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
        dataTrans->setStatus(EINTRN);
    }

    /* now for the atari specific stuff - return current file position */
    int pos = ftell(files[index].hostHandle);                       // get stream position

    if(pos == -1) {                                                 // failed to get position?
        dataTrans->setStatus(EINTRN);
        return;
    }

    dataTrans->addDataDword(pos);                                   // return the position padded with zeros
    dataTrans->padDataToMul16();

    dataTrans->setStatus(E_OK);                                     // OK!
}

void TranslatedDisk::onFtell(BYTE *cmd)
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

void TranslatedDisk::onTgetdate(BYTE *cmd)
{
    SYSTEMTIME  hostTime;
    WORD        atariDate;

    GetLocalTime(&hostTime);

    atariDate = 0;

    atariDate |= (hostTime.wYear - 1980) << 9;
    atariDate |= (hostTime.wMonth      ) << 5;
    atariDate |= (hostTime.wDay        );

    dataTrans->addDataWord(atariDate);      // WORD: atari date
    dataTrans->padDataToMul16();            // 14 bytes of padding

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onTsetdate(BYTE *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 16);      // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    WORD newAtariDate = (((WORD) dataBuffer[0]) << 8) | dataBuffer[1];

    WORD year, month, day;
    year    = (newAtariDate >> 9)   + 1980;
    month   = (newAtariDate >> 5)   & 0x0f;
    day     =  newAtariDate         & 0x1f;

    // todo: setting of the new date




    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onTgettime(BYTE *cmd)
{
    SYSTEMTIME  hostTime;
    WORD        atariTime;

    GetLocalTime(&hostTime);

    atariTime = 0;

    atariTime |= (hostTime.wHour        ) << 11;
    atariTime |= (hostTime.wMinute      ) << 5;
    atariTime |= (hostTime.wSecond / 2  );

    dataTrans->addDataWord(atariTime);      // WORD: atari time
    dataTrans->padDataToMul16();            // 14 bytes of padding

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onTsettime(BYTE *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 16);      // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    WORD newAtariTime = (((WORD) dataBuffer[0]) << 8) | dataBuffer[1];

    BYTE hour, minute, second;
    hour   = (newAtariTime >> 11);
    minute = (newAtariTime >> 5)   & 0x3f;
    second = (newAtariTime         & 0x1f) * 2;

    // todo: setting of the new time




    dataTrans->setStatus(E_OK);
}

WORD TranslatedDisk::fileTimeToAtariDate(FILETIME *ft)
{
    WORD atariDate = 0;

    atariDate |= (2013 - 1980) << 9;            // year
    atariDate |= (11         ) << 5;            // month
    atariDate |= (14         );                 // day

    return atariDate;
}

WORD TranslatedDisk::fileTimeToAtariTime(FILETIME *ft)
{
    WORD atariTime = 0;

    // TODO: do the real conversion


    atariTime |= (12                    ) << 11;        // hours
    atariTime |= (00                    ) << 5;         // minutes
    atariTime |= (01               / 2  );              // seconds

    return atariTime;
}

void TranslatedDisk::attributesHostToAtari(DWORD attrHost, BYTE &attrAtari)
{
    attrAtari = 0;

    if(attrHost & FILE_ATTRIBUTE_READONLY)
        attrAtari |= FA_READONLY;

    if(attrHost & FILE_ATTRIBUTE_HIDDEN)
        attrAtari |= FA_HIDDEN;

    if(attrHost & FILE_ATTRIBUTE_SYSTEM)
        attrAtari |= FA_SYSTEM;

    // if(attrHost &                      )
    //  attrAtari |= FA_VOLUME;

    if(attrHost & FILE_ATTRIBUTE_DIRECTORY)
        attrAtari |= FA_DIR;

    if(attrHost & FILE_ATTRIBUTE_ARCHIVE)
        attrAtari |= FA_ARCHIVE;
}

void TranslatedDisk::attributesAtariToHost(BYTE attrAtari, DWORD &attrHost)
{
    attrHost = 0;

    if(attrAtari & FA_READONLY)
        attrHost |= FILE_ATTRIBUTE_READONLY;

    if(attrAtari & FA_HIDDEN)
        attrHost |= FILE_ATTRIBUTE_HIDDEN;

    if(attrAtari & FA_SYSTEM)
        attrHost |= FILE_ATTRIBUTE_SYSTEM;

    // if(attrAtari & FA_VOLUME)
    //  attrHost |=                     ;

    if(attrAtari & FA_DIR)
        attrHost |= FILE_ATTRIBUTE_DIRECTORY;

    if(attrAtari & FA_ARCHIVE)
        attrHost |= FILE_ATTRIBUTE_ARCHIVE;
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
void TranslatedDisk::onDrvMap(BYTE *cmd)
{
    WORD drives = getDrivesBitmap();

    dataTrans->addDataWord(drives);         // drive bits
    dataTrans->padDataToMul16();            // pad to multiple of 16

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onMediach(BYTE *cmd)
{
    WORD mediach = 0;

    for(int i=2; i<MAX_DRIVES; i++) {       // create media changed bits
        if(conf[i].mediaChanged) {
            mediach |= (1 << i);            // set the bit
        }
    }

    dataTrans->addDataWord(mediach);
    dataTrans->padDataToMul16();            // pad to multiple of 16

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onGetbpb(BYTE *cmd)
{
    WORD drive = cmd[5];

    if(drive >= MAX_DRIVES) {                       // index would be out of range?
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    conf[drive].mediaChanged = false;               // mark as media not changed

    if(!conf[drive].enabled) {                      // if drive not enabled
        for(int i=0; i<9; i++) {                    // add empty data - just in case
            dataTrans->addDataWord(0);
        }
        dataTrans->padDataToMul16();

        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    // if we got here, we should return the BPB data for this drive
    dataTrans->addDataWord(512);                // bytes per sector
    dataTrans->addDataWord(4);                  // sectors per cluster - just a guess :)
    dataTrans->addDataWord(4 * 512);            // bytes per cluster
    dataTrans->addDataWord(32);                 // sector length of root directory - this would be 512 entries in root directory
    dataTrans->addDataWord(8192);               // sectors per FAT - this would be just enough for 1 GB
    dataTrans->addDataWord(1000 + 8192);        // starting sector of second FAT
    dataTrans->addDataWord(1000 + 2*8192);      // starting sector of data
    dataTrans->addDataWord(524288);             // clusters per disk
    dataTrans->addDataWord(1);                  // bit 0=1 - 16 bit FAT, else 12 bit

    dataTrans->padDataToMul16();
    dataTrans->setStatus(E_OK);
}
