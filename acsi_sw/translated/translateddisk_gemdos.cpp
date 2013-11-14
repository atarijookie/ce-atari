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

}

void TranslatedDisk::onFsnext(BYTE *cmd)
{

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

}

void TranslatedDisk::onFopen(BYTE *cmd)
{

}

void TranslatedDisk::onFclose(BYTE *cmd)
{

}

void TranslatedDisk::onFdatime(BYTE *cmd)
{

}

void TranslatedDisk::onFread(BYTE *cmd)
{

}

void TranslatedDisk::onFwrite(BYTE *cmd)
{

}

void TranslatedDisk::onFseek(BYTE *cmd)
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

    BYTE year, month, day;
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
