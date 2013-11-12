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

        dataTrans->setStatus(getDrivesBitmap());    // return the drives bitmap
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

    // Note! The new path is relative (subpath) to the one which is currently set!

    if(!conf[currentDriveIndex].enabled) {
        dataTrans->setStatus(E_NOTHANDLED);         // if we don't have this, not handled
        return;
    }

    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }

    if(!hostPathExists(dataBuffer, true)) {         // path doesn't exists?
        dataTrans->setStatus(EPTHNF);               // path not found
        return;
    }

    // if path exists, store it and return OK
    conf[currentDriveIndex].currentPath = (char *) dataBuffer;
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
    dataTrans->addData((BYTE *) conf[whichDrive].currentPath.c_str(), conf[whichDrive].currentPath.length(), true);
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

}

void TranslatedDisk::onDdelete(BYTE *cmd)
{

}

void TranslatedDisk::onFrename(BYTE *cmd)
{

}

void TranslatedDisk::onFdatime(BYTE *cmd)
{

}

void TranslatedDisk::onFdelete(BYTE *cmd)
{

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

