//----------------------------------------
// CosmosEx fake STiNG - by Jookie, 2014
// Based on sources of original STiNG
//----------------------------------------

#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>
#include <stdint.h>
#include <stdio.h>

#include <gem.h>

#include "globdefs.h"
#include "tcp.h"
#include "con_man.h"

//---------------------
// ACSI / CosmosEx stuff
#include "acsi.h"
#include "ce_commands.h"
#include "stdlib.h"

//---------------------

extern TConInfo conInfo[MAX_HANDLE];                   // this holds info about each connection

//---------------------

int16 TCP_open(uint32 rem_host, uint16 rem_port, uint16 tos, uint16 buff_size)
{
    return connection_open(1, rem_host, rem_port, tos, buff_size);
}

int16 TCP_close(int16 handle, int16 mode, int16 *result)
{
    return connection_close(1, handle, mode, result);
}

int16 TCP_send(int16 handle, void *buffer, int16 length)
{
    return connection_send(1, handle, buffer, length);
}

int16 TCP_wait_state(int16 handle, int16 wantedState, int16 timeout)
{
    if(!handle_valid(handle)) {                         // we don't have this handle? fail
        return E_BADHANDLE;
    }

    DWORD timeStart = getTicks();
    DWORD timeout2 = timeout * 200;    
    
    while(1) {
        // the connection info should be updated in VBL using update_con_info()
        
        WORD currentState = conInfo[handle].tcpConnectionState;     // get the current state
    
        // if the wanted state is CLOSED or CLOSING, and the current state is similar - success
        if(wantedState == TCLOSED || wantedState == TFIN_WAIT1 ||  wantedState == TFIN_WAIT2 ||  wantedState == TCLOSE_WAIT ||  wantedState == TCLOSING ||  wantedState == TLAST_ACK ||  wantedState == TTIME_WAIT) {
            if(currentState == TCLOSED || currentState == TFIN_WAIT1 ||  currentState == TFIN_WAIT2 ||  currentState == TCLOSE_WAIT ||  currentState == TCLOSING ||  currentState == TLAST_ACK ||  currentState == TTIME_WAIT) {
                break;
            }
        }
    
        // if the wanted state is CONNECTING, and the current state is similar - success
        if(wantedState == TSYN_SENT || wantedState == TSYN_RECV) {
            if(currentState == TSYN_SENT || currentState == TSYN_RECV) {
                break;
            }    
        }

        // for other matching cases, success
        if(wantedState == currentState) {
            break;
        }
        
        if((getTicks() - timeStart) >= timeout2) {              // if timed out, fail 
            return E_CNTIMEOUT;
        }
        
        appl_yield();                                           // keep GEM apps responding
    } 
    
    return E_NORMAL;
}

int16 TCP_ack_wait(int16 handle, int16 timeout)
{
    if(!handle_valid(handle)) {                                 // we don't have this handle? fail
        return E_BADHANDLE;
    }

    // GlueSTiK inspired -- incompatibility:  Does nothing. Linux handles this internally.
    return E_NORMAL;
}

int16 TCP_info(int16 handle, TCPIB *tcp_info)
{
    if(!handle_valid(handle)) {                                 // we don't have this handle? fail
        return E_BADHANDLE;
    }
    
    update_con_info();                                          // update the info 

    if(tcp_info == NULL) {                                      // no pointer? fail
        return E_BADHANDLE;
    }
    
    storeWord((BYTE *) tcp_info, conInfo[handle].tcpConnectionState);       // return the connection state
    return E_NORMAL;
}

