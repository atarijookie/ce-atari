#include <string.h>
#include <stdio.h>
#include <io.h>

#include "../global.h"
#include "translateddisk.h"
#include "gemdos.h"
#include "gemdos_errno.h"

extern "C" void outDebugString(const char *format, ...);

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

bool TranslatedDisk::hostPathExists(BYTE *atariPath, bool relativeNotAbsolute)
{
    if(!conf[currentDriveIndex].enabled) {      // we don't have this drive? fail
        return false;
    }

    // first construct the tested path
    std::string fullPath;

    fullPath = conf[currentDriveIndex].hostPath + "\\";             // first add the host path

    if(relativeNotAbsolute) {                                       // relative path means that you start from the currentPath
        fullPath += conf[currentDriveIndex].currentPath + "\\";
    }

    fullPath += ((char *) atariPath);                               // then add the atari path

    // now check if it exists
    int res = _access(fullPath.c_str(), 0);

    if(res != -1) {             // if it's not this error, then the file exists
        return true;
    }

    return false;
}


