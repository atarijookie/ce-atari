#include <string.h>
#include <stdio.h>

#include "../global.h"
#include "../debug.h"

#include "netadapter.h"
#include "netadapter_commands.h"

NetAdapter::NetAdapter(void)
{
    dataTrans = 0;

    loadSettings();
}

NetAdapter::~NetAdapter()
{

}

void NetAdapter::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

void NetAdapter::reloadSettings(int type)
{
    loadSettings();
}

void NetAdapter::loadSettings(void)
{
    Debug::out(LOG_INFO, "NetAdapter::loadSettings");

    // first read the new settings
    Settings s;


}

void NetAdapter::processCommand(BYTE *command)
{
    cmd = command;

    if(dataTrans == 0) {
        Debug::out(LOG_ERROR, "NetAdapter::processCommand was called without valid dataTrans, can't tell ST that his went wrong...");
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    BYTE *pCmd;
    BYTE isIcd = false;

    isIcd   = ((command[0] & 0x1f) == 0x1f);            // it's an ICD command, if lowest 5 bits are all set in the cmd[0]
    pCmd    = (!isIcd) ? command : (command + 1);       // get the pointer to where the command starts

    // pCmd[1] & pCmd[2] are 'CE' tag, pCmd[3] is host module ID
    // pCmd[4] will be command, the rest will be params - depending on command type
    
    switch(pCmd[4]) {
        case NET_CMD_IDENTIFY:              break;

        // TCP functions
        case NET_CMD_TCP_OPEN:              break;
        case NET_CMD_TCP_CLOSE:             break;
        case NET_CMD_TCP_SEND:              break;
        case NET_CMD_TCP_WAIT_STATE:        break;
        case NET_CMD_TCP_ACK_WAIT:          break;
        case NET_CMD_TCP_INFO:              break;

        // UDP FUNCTION
        case NET_CMD_UDP_OPEN:              break;
        case NET_CMD_UDP_CLOSE:             break;
        case NET_CMD_UDP_SEND:              break;

        // ICMP FUNCTIONS
        case NET_CMD_ICMP_SEND_EVEN:        break;
        case NET_CMD_ICMP_SEND_ODD:         break;
        case NET_CMD_ICMP_HANDLER:          break;
        case NET_CMD_ICMP_DISCARD:          break;
        case NET_CMD_ICMP_GET_DGRAMS:       break;

        // CONNECTION MANAGER
        case NET_CMD_CNKICK:                break;
        case NET_CMD_CNBYTE_COUNT:          break;
        case NET_CMD_CNGET_CHAR:            break;
        case NET_CMD_CNGET_NDB:             break;
        case NET_CMD_CNGET_BLOCK:           break;
        case NET_CMD_CNGETINFO:             break;
        case NET_CMD_CNGETS:                break;
        case NET_CMD_CN_UPDATE_INFO:        break;
        case NET_CMD_CN_READ_DATA:          break;
        case NET_CMD_CN_GET_DATA_COUNT:     break;
        case NET_CMD_CN_LOCATE_DELIMITER:   break;

        // MISC
        case NET_CMD_RESOLVE:               break;
        case NET_CMD_ON_PORT:               break;
        case NET_CMD_OFF_PORT:              break;
        case NET_CMD_QUERY_PORT:            break;
        case NET_CMD_CNTRL_PORT:            break;

        case NET_CMD_RESOLVE_GET_RESPONSE:  break;
    }

    dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
}
//----------------------------------------------

