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

        WORD drives = getDrivesBitmap();
        dataTrans->addData(drives >>   8);          // return the drives in data
        dataTrans->addData(drives & 0xff);

        dataTrans->padDataToMul16();                // and pad to 16 bytes for DMA chip

        dataTrans->setStatus(E_OK);                 // return OK
    }

    dataTrans->setStatus(E_NOTHANDLED);             // in other cases - not handled
}

void TranslatedDisk::onDgetdrv(BYTE *cmd)
{
    // Dgetdrv() returns the current GEMDOS drive code. Drive ‘A:’ is represented by
    // a return value of 0, ‘B:’ by a return value of 1, and so on.

    if(conf[currentDriveIndex].enabled) {           // if we got this drive, return the current drive
        dataTrans->setStatus(currentDriveIndex);
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

    // return the current path for current drive
    dataTrans->addData((BYTE *) conf[whichDrive].currentAtariPath.c_str(), conf[whichDrive].currentAtariPath.length(), true);
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
    // first verify if the file attributes are OK
    if((found->dwFileAttributes & FILE_ATTRIBUTE_READONLY)!=0   && (findAttribs & FA_READONLY)==0)  // is read only, but not searching for that
        return;

    if((found->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)!=0     && (findAttribs & FA_HIDDEN)==0)    // is hidden, but not searching for that
        return;

    if((found->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)!=0     && (findAttribs & FA_SYSTEM)==0)    // is system, but not searching for that
        return;

    if((found->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0  && (findAttribs & FA_DIR)==0)       // is dir, but not searching for that
        return;

    if((found->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)!=0    && (findAttribs & FA_ARCHIVE)==0)   // is archive, but not searching for that
        return;

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
    memcpy(&buf[9], fileName, 14);

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

void TranslatedDisk::onFcreate(BYTE *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 64);      // get data from Hans

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
    files[index].atariHandle    = index + 6;                        // handles 0 - 5 are reserved
    files[index].hostPath       = hostName;

    dataTrans->setStatus(files[index].atariHandle);                 // return the handle
}

void TranslatedDisk::onFopen(BYTE *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 64);      // get data from Hans

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
    files[index].atariHandle    = index + 6;                        // handles 0 - 5 are reserved
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
    bool res;

    int param       = cmd[5];
    int handle      = param & 0x7f;         // lowest 7 bits
    int setNotGet   = param >> 7;           // highest bit

    int index = findFileHandleSlot(handle);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    WORD atariTime = 0, atariDate = 0;

    if(setNotGet) {                         // on SET
        res = dataTrans->recvData(dataBuffer, 16);      // get data from Hans

        if(!res) {                                      // failed to get data? internal error!
            dataTrans->setStatus(EINTRN);
            return;
        }

        atariTime = getWord(dataBuffer);                // retrieve the time and date from ST
        atariDate = getWord(dataBuffer + 2);

        WORD year, month, day;
        year    = (atariDate >> 9)   + 1980;
        month   = (atariDate >> 5)   & 0x0f;
        day     =  atariDate         & 0x1f;

        WORD hours, minutes, seconds;
        hours   =  (atariTime >> 11) & 0x1f;
        minutes =  (atariTime >>  5) & 0x3f;
        seconds = ((atariTime >>  5) & 0x1f) * 2;


        // TODO: setting the date / time to the file


    } else {                                // on GET

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
    int param               = cmd[5];
    int atariHandle         = param & 0x3f;         // lowest 6 bits
    int transferSizeCode    = param >> 6;           // 2 highest bits

    int index = findFileHandleSlot(atariHandle);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    // transfer size:
    //  0 -   1 sector
    //	1 -   8 sectors ( 4 kB) [1 MB = 250 of these]
    //	2 -  32 sectors (16 kB) [1 MB =  64 of these]
    //	3 - 128 sectors (64 kB) [1 MB =  16 of these]
    // Note that 1st DWORD will be the number of actual data count that was read.

    int transferSizeSectors = 0;

    switch(transferSizeCode) {
        case 0: transferSizeSectors =   1; break;
        case 1: transferSizeSectors =   8; break;
        case 2: transferSizeSectors =  32; break;
        case 3: transferSizeSectors = 128; break;
    }

    int transferSizeBytes   = transferSizeSectors * 512;    // calculate the size in bytes
    transferSizeBytes       -= 4;                           // subtract 4 bytes (size of DWORD), because 1st DWORD is the actual fread size

    int cnt = fread (dataBuffer, 1, transferSizeBytes, files[index].hostHandle);

    dataTrans->addDataDword(cnt);                           // first store the count of bytes read
    dataTrans->addData(dataBuffer, transferSizeBytes);      // then store the data

    dataTrans->setStatus(E_OK);                             // and return OK status
}

void TranslatedDisk::onFwrite(BYTE *cmd)
{
    int param               = cmd[5];
    int atariHandle         = param & 0x3f;         // lowest 6 bits
    int transferSizeCode    = param >> 6;           // 2 highest bits

    int index = findFileHandleSlot(atariHandle);

    if(index == -1) {                                               // handle not found? not handled, try somewhere else
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    // transfer size:
    //  0 - partial sector - 1st word is the data size
    //	1 -   8 sectors ( 4 kB) [1 MB = 250 of these]
    //	2 -  32 sectors (16 kB) [1 MB =  64 of these]
    //	3 - 128 sectors (64 kB) [1 MB =  16 of these]
    int transferSizeSectors = 0;

    switch(transferSizeCode) {
        case 0: transferSizeSectors =   1; break;
        case 1: transferSizeSectors =   8; break;
        case 2: transferSizeSectors =  32; break;
        case 3: transferSizeSectors = 128; break;
    }

    int transferSizeBytes = transferSizeSectors * 512;

    bool res;
    res = dataTrans->recvData(dataBuffer, transferSizeBytes);   // get data from Hans

    if(!res) {                                                  // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    BYTE *data = dataBuffer;                                    // this points to the start of data

    if(transferSizeSectors == 1) {                              // when transfering only single sector, first word is the real data size
        transferSizeBytes = getWord(dataBuffer);                // read the real transfer size
        data += 2;                                              // skip this word when writing to file
    }

    int bWritten;
    bWritten = fwrite(data, 1, transferSizeBytes, files[index].hostHandle);    // write the data

    if(bWritten != transferSizeBytes) {                         // when didn't write all the data
        dataTrans->setStatus(EINTRN);
        return;
    }

    dataTrans->setStatus(E_OK);                                 // when all the data was written
}

void TranslatedDisk::onFseek(BYTE *cmd)
{
    bool res;

    res = dataTrans->recvData(dataBuffer, 16);      // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    // get seek params
    DWORD   offset      = getDword(dataBuffer);
    BYTE    atariHandle = dataBuffer[4];
    BYTE    seekMode    = dataBuffer[5];

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

    if(iRes == 0) {                         // on OK
        dataTrans->setStatus(E_OK);
    } else {                                // on error
        dataTrans->setStatus(EINTRN);
    }
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

void TranslatedDisk::onFdup(BYTE *cmd)
{

}

void TranslatedDisk::onFforce(BYTE *cmd)
{

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

