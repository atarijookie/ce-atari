// vim: expandtab shiftwidth=4 tabstop=4
#include <stdio.h>
#include <string.h>

#include "../global.h"
#include "../native/scsi_defs.h"
#include "../acsidatatrans.h"
#include "../update.h"
#include "../translated/translateddisk.h"

#include "../settings.h"
#include "../utils.h"
#include "keys.h"
#include "configstream.h"
#include "config_commands.h"
#include "../debug.h"

uint8_t isUpdateStartingFlag = 0;

ConfigStream::ConfigStream(int whereItWillBeShown)
{
    stScreenWidth   = 40;
    gotoOffset      = 0;

    dataTrans   = NULL;
    reloadProxy = NULL;

    lastCmdTime = 0;
}

ConfigStream::~ConfigStream()
{

}

void ConfigStream::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

void ConfigStream::setSettingsReloadProxy(SettingsReloadProxy *rp)
{
    reloadProxy = rp;
}

void ConfigStream::processCommand(uint8_t *cmd, int writeToFd)
{
    static uint8_t readBuffer[READ_BUFFER_SIZE];
    int streamCount;

    if(cmd[1] != 'C' || cmd[2] != 'E' || cmd[3] != HOSTMOD_CONFIG) {        // not for us?
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    lastCmdTime = Utils::getCurrentMs();

    switch(cmd[4]) {
    case CFG_CMD_IDENTIFY:          // identify?
        dataTrans->addDataBfr("CosmosEx config console", 23, true);       // add identity string with padding
        dataTrans->setStatus(SCSI_ST_OK);
        break;

    case CFG_CMD_KEYDOWN:
//        onKeyDown(cmd[5]);                                                // first send the key down signal

//        if(enterKeyEventLater) {                                            // if we should handle some event
//            enterKeyHandler(enterKeyEventLater);                            // handle it
//            enterKeyEventLater = 0;                                         // and don't let it handle next time
//        }
//
//        streamCount = getStream(false, readBuffer, READ_BUFFER_SIZE);     // then get current screen stream

        dataTrans->addDataBfr(readBuffer, streamCount, true);                              // add data and status, with padding to multiple of 16 bytes
        dataTrans->setStatus(SCSI_ST_OK);

        Debug::out(LOG_DEBUG, "handleConfigStream -- CFG_CMD_KEYDOWN -- %d bytes", streamCount);
        break;

    case CFG_CMD_SET_RESOLUTION:
        switch(cmd[5]) {
        case ST_RESOLUTION_LOW:     stScreenWidth = 40; break;
        case ST_RESOLUTION_MID:
        case ST_RESOLUTION_HIGH:    stScreenWidth = 80; break;
        }

        gotoOffset = (stScreenWidth - 40) / 2;

//        destroyCurrentScreen();                     // the resolution might have changed, so destroy and screate the home screen again
//        createScreen_homeScreen();

        dataTrans->setStatus(SCSI_ST_OK);
        Debug::out(LOG_DEBUG, "handleConfigStream -- CFG_CMD_SET_RESOLUTION -- %d", cmd[5]);
        break;

    case CFG_CMD_SET_CFGVALUE:
        //onSetCfgValue();
        break;

    case CFG_CMD_UPDATING_QUERY:
    {
        uint8_t updateComponentsWithValidityNibble = 0xC0 | 0x0f;          // for now pretend all needs to be updated

        dataTrans->addDataByte(isUpdateStartingFlag);
        dataTrans->addDataByte(updateComponentsWithValidityNibble);
        dataTrans->padDataToMul16();

        dataTrans->setStatus(SCSI_ST_OK);

        Debug::out(LOG_DEBUG, "handleConfigStream -- CFG_CMD_UPDATING_QUERY -- isUpdateStartingFlag: %d, updateComponentsWithValidityNibble: %x", isUpdateStartingFlag, updateComponentsWithValidityNibble);
        break;
    }

    case CFG_CMD_REFRESH:
//        screenChanged = true;                                           // get full stream, not only differences
//        streamCount = getStream(false, readBuffer, READ_BUFFER_SIZE);   // then get current screen stream

        dataTrans->addDataBfr(readBuffer, streamCount, true);              // add data and status, with padding to multiple of 16 bytes
        dataTrans->setStatus(SCSI_ST_OK);

        Debug::out(LOG_DEBUG, "handleConfigStream -- CFG_CMD_REFRESH -- %d bytes", streamCount);
        break;

    case CFG_CMD_GO_HOME:
//        streamCount = getStream(true, readBuffer, READ_BUFFER_SIZE);      // get homescreen stream

        dataTrans->addDataBfr(readBuffer, streamCount, true);                              // add data and status, with padding to multiple of 16 bytes
        dataTrans->setStatus(SCSI_ST_OK);

        Debug::out(LOG_DEBUG, "handleConfigStream -- CFG_CMD_GO_HOME -- %d bytes", streamCount);
        break;

    case CFG_CMD_LINUXCONSOLE_GETSTREAM:                                // get the current bash console stream
        if(cmd[5] != 0) {                                               // if it's a real key, send it
            linuxConsole_KeyDown(cmd[5]);
        }

//        streamCount = linuxConsole_getStream(readBuffer, 3 * 512);      // get the stream from shell
        dataTrans->addDataBfr(readBuffer, streamCount, true);           // add data and status, with padding to multiple of 16 bytes
        dataTrans->setStatus(SCSI_ST_OK);

        break;

    default:                            // other cases: error
        dataTrans->setStatus(SCSI_ST_CHECK_CONDITION);
        break;
    }

    if(writeToFd == -1) {
        dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
    } else {
        dataTrans->sendDataToFd(writeToFd);
    }
}

int ConfigStream::getStream(bool homeScreen, uint8_t *bfr, int maxLen)
{
    memset(bfr, 0, maxLen);                             // clear the buffer

    int totalCnt = 0;

    // first turn off the cursor to avoid cursor blinking on the screen
    *bfr++ = 27;
    *bfr++ = 'f';       // CUR_OFF
    totalCnt += 2;

//    if(screenChanged) {                                 // if screen changed, clear screen (CLEAR_HOME) and draw it all
//        *bfr++ = 27;
//        *bfr++ = 'E';   // CLEAR_HOME
//        totalCnt += 2;
//    }

    // add code here... later...

    //-------
    // get update components, if on update screen
//    if(isUpdScreen) {
//        *bfr++ = 0xC0 | 0x0f;   // store update components and the validity nibble -- for now pretend all needs to be updated
//    } else {
        *bfr++ = 0;     // store update components - not updating anything
//    }
    totalCnt++;
    //-------

    return totalCnt;                                    // return the count of bytes used
}
