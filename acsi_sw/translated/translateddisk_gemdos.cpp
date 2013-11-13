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

        for(int i=0; i<14; i++) {                   // and pad to 16 bytes for DMA chip
            dataTrans->addData(0);
        }

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

}

void TranslatedDisk::onFdatime(BYTE *cmd)
{

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

}

void TranslatedDisk::onTsetdate(BYTE *cmd)
{

}

void TranslatedDisk::onTgettime(BYTE *cmd)
{

}

void TranslatedDisk::onTsettime(BYTE *cmd)
{

}

