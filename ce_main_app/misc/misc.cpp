// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <string>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

#include "../global.h"
#include "../debug.h"
#include "../settings.h"
#include "../utils.h"
#include "acsidatatrans.h"
#include "settingsreloadproxy.h"
#include "misc.h"
#include "translated/gemdos_errno.h"

extern THwConfig hwConfig;
extern InterProcessEvents events;

#define LICENSE_URL     "http://joo.kie.sk/cosmosex/license/?raw=1&serial="

Misc::Misc()
{
}

Misc::~Misc()
{

}

void Misc::setDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

void Misc::processCommand(uint8_t *cmd)
{
    if(dataTrans == 0 ) {
        Debug::out(LOG_ERROR, "processCommand was called without valid dataTrans!");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    if(cmd[1] != 'C' || cmd[2] != 'E' || cmd[3] != HOSTMOD_MISC) {   // not for us?
        return;
    }

    const char *functionName = functionCodeToName(cmd[4]);
    Debug::out(LOG_DEBUG, "Misc function - %s (%02x)", functionName, cmd[4]);

    // now do all the command handling
    switch(cmd[4]) {
        // special CosmosEx commands for this module
        case MISC_CMD_IDENTIFY:
            dataTrans->addDataBfr("CosmosEx Misc functions ", 24, true);       // add identity string with padding
            dataTrans->setStatus(E_OK);
            break;

        case MISC_CMD_SEND_SERIAL:      recvHwSerial(cmd);      break;
        case MISC_CMD_GET_SETTINGS:     getSettings(cmd);       break;
        case MISC_CMD_GET_UPDATE:       getUpdate(cmd);         break;
        case MISC_CMD_HOST_SHUTDOWN:    hostShutdown(cmd);      break;

        // in other cases
        default:                                // in other cases
        dataTrans->setStatus(EINVFN);           // invalid function
        break;
    }

    dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
}

void Misc::recvHwSerial(uint8_t *cmd)
{
    uint32_t res;
    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "Misc::recvHwSerial - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }

    if(dataBuffer[0] != 3) {                        // HW version not 3? fail
        Debug::out(LOG_DEBUG, "Misc::recvHwSerial - wrong HW version");
        dataTrans->setStatus(EINTRN);
        return;
    }

    memcpy(hwConfig.hwSerial, &dataBuffer[1], 12);    // store the current HW serial

    char tmp[32];
    Settings::binToHex(hwConfig.hwSerial, 12, tmp);   // HW serial as hexadecimal string
    Debug::out(LOG_DEBUG, "Misc::recvHwSerial - HW serial number: %s", tmp);

    dataTrans->setStatus(E_OK);
}

void Misc::getSettings(uint8_t *cmd)
{
    Settings s;
    FloppyConfig fc;

    s.loadFloppyConfig(&fc);

    dataTrans->addDataByte(fc.soundEnabled ? 1 : 0);    // floppy seek sound enabled?

    dataTrans->addZerosUntilSize(512);                  // keep adding zeros until full sector size
    dataTrans->setStatus(E_OK);
}

void Misc::getUpdate(uint8_t *cmd)
{
    // TODO: based on the command, return update size or update data of size and offset

    dataTrans->setStatus(E_OK);
}

void Misc::hostShutdown(uint8_t *cmd)
{
    Debug::out(LOG_DEBUG, "Misc::hostShutdown - device requested host shutdown!");

    system("shutdown now");         // turn off device now
    sigintReceived = 1;             // turn off app (probably not needed)

    dataTrans->setStatus(E_OK);
}

const char *Misc::functionCodeToName(int code)
{
    switch(code) {
        case MISC_CMD_SEND_SERIAL:      return "MISC_CMD_SEND_SERIAL";
        case MISC_CMD_GET_SETTINGS:     return "MISC_CMD_GET_SETTINGS";
        case MISC_CMD_GET_UPDATE:       return "MISC_CMD_GET_UPDATE";
        case MISC_CMD_HOST_SHUTDOWN:    return "MISC_CMD_HOST_SHUTDOWN";

        default:                        return "unknown";
    }
}
