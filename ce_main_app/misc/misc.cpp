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
#include "../downloader.h"
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

void Misc::processCommand(BYTE *cmd)
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

        case MISC_CMD_SEND_SERIAL:      recvHwSerialAndDeleteLicense(cmd);  break;
        case MISC_CMD_GET_LICENSE:      getLicense(cmd);                    break;
        case MISC_CMD_GET_SETTINGS:     getSettings(cmd);                   break;
        case MISC_CMD_GET_UPDATE:       getUpdate(cmd);                     break;
        case MISC_CMD_HOST_SHUTDOWN:    hostShutdown(cmd);                  break;

        // in other cases
        default:                                // in other cases
        dataTrans->setStatus(EINVFN);           // invalid function
        break;
    }

    dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
}

void Misc::recvHwSerialAndDeleteLicense(BYTE *cmd)
{
    DWORD res;
    res = dataTrans->recvData(dataBuffer, 512);     // get data from Hans

    if(!res) {                                      // failed to get data? internal error!
        Debug::out(LOG_DEBUG, "Misc::sendSerialAndDeleteLicense - failed to receive data...");
        dataTrans->setStatus(EINTRN);
        return;
    }

    bool deleteLicenseOnHost = (cmd[5] == 1);       // if this is 1, then should delete (invalid) license on this host

    if(dataBuffer[0] != 3) {                        // HW version not 3? fail
        Debug::out(LOG_DEBUG, "Misc::sendSerialAndDeleteLicense - wrong HW version");
        dataTrans->setStatus(EINTRN);
        return;
    }

    memcpy(hwConfig.hwSerial, &dataBuffer[1], 13);    // store the current HW serial

    char tmp[32];
    Settings::binToHex(hwConfig.hwSerial, 13, tmp);   // HW serial as hexadecimal string
    Debug::out(LOG_DEBUG, "Misc::recvHwSerialAndDeleteLicense - HW serial number: %s", tmp);

    // if should delete current license
    if(deleteLicenseOnHost) {
        // find current license in settings
        char keyName[64];
        generateLicenseKeyName(keyName);                        // generate key name containing HW serial number

        Settings s;
        char *storedLicense = s.getBinaryString(keyName, 10);   // get current license

        // if license from settings matched the received license from HW, delete it
        if(memcmp(&dataBuffer[14], storedLicense, 10) == 0) {
            Debug::out(LOG_DEBUG, "Misc::recvHwSerialAndDeleteLicense - stored license wrong, deleting");

            BYTE zeros[10];
            memset(zeros, 0, 10);                   // create buffer of 10 zeros

            s.setBinaryString(keyName, zeros, 10);  // set stored license to zeros

            // retrieve license from web
            retrieveLicenseForSerial();
        } else {
            Debug::out(LOG_DEBUG, "Misc::recvHwSerialAndDeleteLicense - stored license different than the device license, not deleting");
        }
    }

    dataTrans->setStatus(E_OK);
}

void Misc::getLicense(BYTE *cmd)
{
    // take serial from hwSerial and see if got stored license for it
    BYTE license[10];
    bool good = getLicenseForSerialFromSettings(license);

    dataTrans->addDataBfr(hwConfig.hwSerial, 13, false);  // add current serial to buffer
    dataTrans->addDataBfr(license, 10, false);          // add license to buffer

    char tmp[32];
    Settings::binToHex(license, 10, tmp);               // HW license as hexadecimal string
    Debug::out(LOG_DEBUG, "Misc::getLicense - HW license: %s", tmp);

    // no stored license, retrieve license from web
    if(!good) {
        retrieveLicenseForSerial();
    }

    dataTrans->addZerosUntilSize(512);                  // keep adding zeros until full sector size
    dataTrans->setStatus(E_OK);
}

void Misc::generateLicenseKeyName(char *keyName)
{
    strcpy(keyName, "HW_LICENSE_");                 // start with "HW_LICENSE_"
    Settings::binToHex(hwConfig.hwSerial, 13, keyName + 11);  // take hwSerial and convert it from binary to hex string and append it to key name
}

bool Misc::getLicenseForSerialFromSettings(BYTE *bfrLicense)
{
    char keyName[64];
    generateLicenseKeyName(keyName);                // generate key name containing HW serial number

    Settings s;
    char *license = s.getBinaryString(keyName, 10);

    memcpy(bfrLicense, license, 10);                // copy from static char to supplied buffer

    char zeros[10];
    memset(zeros, 0, 10);                           // create buffer of 10 zeros

    bool good = (memcmp(license, zeros, 10) != 0);  // if retrieved license is something other than zeros, we're good
    return good;
}

void Misc::retrieveLicenseForSerial(void)
{
    char keyName[64];
    generateLicenseKeyName(keyName);                // generate key name containing HW serial number

    // construct url for license retrieval
    char getLicenseUrl[100];
    strcpy(getLicenseUrl, LICENSE_URL);             // base url (~50 bytes)

    int len = strlen(getLicenseUrl);                // find the length of current url
    Settings::binToHex(hwConfig.hwSerial, 13, getLicenseUrl + len);   // append HW serial as hexadecimal string to url (26 bytes)

    Debug::out(LOG_DEBUG, "Misc::retrieveLicenseForSerial - will try to retrieve HW license from url: %s", getLicenseUrl);

    // start the download
    TDownloadRequest tdr;
    tdr.srcUrl          = getLicenseUrl;            // this URL will contain also serial number
    tdr.checksum        = 0;                        // don't check checksum
    tdr.dstDir          = keyName;                  // instead of destination directory we will have settings key name
    tdr.downloadType    = DWNTYPE_HW_LICENSE;
    tdr.pStatusByte     = NULL;                     // don't update this status byte

    Downloader::add(tdr);
}

void Misc::getSettings(BYTE *cmd)
{
    Settings s;
    FloppyConfig fc;

    s.loadFloppyConfig(&fc);

    dataTrans->addDataByte(fc.soundEnabled ? 1 : 0);    // floppy seek sound enabled?

    dataTrans->addZerosUntilSize(512);                  // keep adding zeros until full sector size
    dataTrans->setStatus(E_OK);
}

void Misc::getUpdate(BYTE *cmd)
{
    // TODO: based on the command, return update size or update data of size and offset

    dataTrans->setStatus(E_OK);
}

void Misc::hostShutdown(BYTE *cmd)
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
        case MISC_CMD_GET_LICENSE:      return "MISC_CMD_GET_LICENSE";
        case MISC_CMD_GET_SETTINGS:     return "MISC_CMD_GET_SETTINGS";
        case MISC_CMD_GET_UPDATE:       return "MISC_CMD_GET_UPDATE";
        case MISC_CMD_HOST_SHUTDOWN:    return "MISC_CMD_HOST_SHUTDOWN";

        default:                        return "unknown";
    }
}
