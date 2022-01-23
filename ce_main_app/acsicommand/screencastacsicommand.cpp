#include <stdio.h>

#include "screencastacsicommand.h"
#include "global.h"
#include "debug.h"

#include "translated/gemdos_errno.h"

#define SCREENCAST_BUFFER_SIZE (32000*2)

ScreencastAcsiCommand::ScreencastAcsiCommand(AcsiDataTrans *dt):dataTrans(dt)
{
    dataBuffer  = new uint8_t[SCREENCAST_BUFFER_SIZE];
}

ScreencastAcsiCommand::~ScreencastAcsiCommand() 
{
	delete []dataBuffer;
}
 
void ScreencastAcsiCommand::processCommand(uint8_t *command)
{
    cmd = command;

    if(dataTrans == 0) {
        Debug::out(LOG_ERROR, "ScreencastAcsiCommand::processCommand was called without valid dataTrans, can't tell ST that his went wrong...");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand");

    switch(cmd[4]) {
        case TRAN_CMD_SENDSCREENCAST:                    // ST sends screen buffer
            readScreen();
            break;
        case TRAN_CMD_SCREENCASTPALETTE:                    // ST sends screen buffer
            readPalette();
            break;
    }

    //>dataTrans->sendDataAndStatus();         // send all the stuff after handling, if we got any
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand done.");
}

uint32_t ScreencastAcsiCommand::get24bits(uint8_t *bfr)
{
    uint32_t val = 0;

    val  = bfr[0];       // get hi
    val  = val << 8;

    val |= bfr[1];      // get mid
    val  = val << 8;

    val |= bfr[2];      // get lo

    return val;
}

void ScreencastAcsiCommand::readScreen()
{
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand TRAN_CMD_SENDSCREENCAST");
    uint8_t iScreenmode         = cmd[5];
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand screenmode %d",iScreenmode);
    
    uint32_t byteCount         = get24bits(cmd + 6);
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand bytes %d",byteCount);
    
    if(iScreenmode<0 || iScreenmode>2) {                       // unknown screenmode?
        Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand unknown screenmode %d",iScreenmode);
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }
    
    if(byteCount > (254 * 512)) {                                   // requesting to transfer more than 254 sectors at once? fail!
        Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand too many sectors %d",byteCount);
        dataTrans->setStatus(EINTRN);
        return;
    }
    
    int mod = byteCount % 16;       // how many we got in the last 1/16th part?
    int pad = 0;
    
    if(mod != 0) {
        pad = 16 - mod;             // how many we need to add to make count % 16 equal to 0?
    }
    
    uint32_t transferSizeBytes = byteCount + pad;
    
    bool res;
    res = dataTrans->recvData(dataBuffer, transferSizeBytes);   // get data from Hans
    
    if(!res) {                                                  // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }
    
    dataTrans->setStatus(RW_ALL_TRANSFERED);                    // when all the data was written
}

void ScreencastAcsiCommand::readPalette()
{
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand TRAN_CMD_SENDSCREENCAST");
    uint8_t iScreenmode         = cmd[5];
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand screenmode %d",iScreenmode);
    
    uint32_t byteCount         = get24bits(cmd + 6);
    Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand bytes %d",byteCount);
    
    if(iScreenmode<0 || iScreenmode>2) {                       // unknown screenmode?
        Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand unknown screenmode %d",iScreenmode);
        dataTrans->setStatus(E_NOTHANDLED);
        return;
    }
    
    if(byteCount > (254 * 512)) {                                   // requesting to transfer more than 254 sectors at once? fail!
        Debug::out(LOG_DEBUG, "ScreencastAcsiCommand::processCommand too many sectors %d",byteCount);
        dataTrans->setStatus(EINTRN);
        return;
    }
    
    int mod = byteCount % 16;       // how many we got in the last 1/16th part?
    int pad = 0;
    
    if(mod != 0) {
        pad = 16 - mod;             // how many we need to add to make count % 16 equal to 0?
    }
    
    uint32_t transferSizeBytes = byteCount + pad;
    
    bool res;
    res = dataTrans->recvData(dataBuffer, transferSizeBytes);   // get data from Hans
    
    if(!res) {                                                  // failed to get data? internal error!
        dataTrans->setStatus(EINTRN);
        return;
    }
    
    dataTrans->setStatus(RW_ALL_TRANSFERED);                    // when all the data was written
}
