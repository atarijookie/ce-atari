#include <string.h>
#include <stdio.h>

#include "../global.h"
#include "translateddisk.h"
#include "gemdos.h"
#include "gemdos_errno.h"

extern "C" void outDebugString(const char *format, ...);

#define BUFFER_SIZE             (1024*1024)
#define BUFFER_SIZE_SECTORS     (BUFFER_SIZE / 512)

TranslatedDisk::TranslatedDisk(void)
{
    dataTrans = 0;

    dataBuffer  = new BYTE[BUFFER_SIZE];
    dataBuffer2 = new BYTE[BUFFER_SIZE];

    for(int i=0; i<16; i++) {               // initialize the config structs
        conf[i].enabled         = false;
        conf[i].stDriveLetter   = 'C' + i;
        conf[i].currentPath     = "\\";
    }

    currentDriveLetter  = 'C';
    currentDriveIndex   = 0;
}

TranslatedDisk::~TranslatedDisk()
{
    delete []dataBuffer;
    delete []dataBuffer2;
}

void TranslatedDisk::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
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
        case GEMDOS_Fsetdta:        onFsetdta(cmd);     break;
        case GEMDOS_Fgetdta:        onFgetdta(cmd);     break;
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

void TranslatedDisk::onDsetdrv(BYTE *cmd)
{
    // Dsetdrv() sets the current GEMDOS drive and returns a bitmap of mounted drives.

    int param = cmd[5];

    if(param < 0 || param > 15) {                   // drive number out of range? not handled
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(param < 2) {                                 // floppy drive selected? store current drive, but don't handle
        currentDriveLetter  = 'A' + param;          // store the current drive
        currentDriveIndex   = param;

        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }

    if(conf[param].enabled) {                       // if that drive is enabled in cosmosEx
        currentDriveLetter  = 'A' + param;          // store the current drive
        currentDriveIndex   = param;

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
    if(conf[currentDriveIndex].enabled) {           // if we got this drive
        // TODO: check if the path on host exists, then handle the situation

        dataTrans->setStatus(EPTHNF);

        // if path exists, store it and return OK
        conf[currentDriveIndex].currentPath = newPath;
        dataTrans->setStatus(E_OK);
    }

    dataTrans->setStatus(E_NOTHANDLED);             // if we don't have this, not handled
}

void TranslatedDisk::onDgetpath(BYTE *cmd)
{

}

void TranslatedDisk::onFsetdta(BYTE *cmd)
{

}

void TranslatedDisk::onFgetdta(BYTE *cmd)
{

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

