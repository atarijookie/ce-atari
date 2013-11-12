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

    for(int i=0; i<14; i++) {               // initialize the config structs
        conf[i].enabled         = false;
        conf[i].stDriveLetter   = 'C' + i;
    }
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

void TranslatedDisk::onGetConfig(BYTE *cmd)
{
    WORD drives = 0;

    for(int i=0; i<14; i++) {               // create enabled drive bits
        if(conf[i].enabled) {
            drives |= (1 << (i + 2));       // set the bit
        }
    }

    dataTrans->addData(drives >>    8);     // drive bits first
    dataTrans->addData(drives &  0xff);

    dataTrans->setStatus(E_OK);
}

void TranslatedDisk::onDsetdrv(BYTE *cmd)
{

}

void TranslatedDisk::onDgetdrv(BYTE *cmd)
{

}

void TranslatedDisk::onDsetpath(BYTE *cmd)
{

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

