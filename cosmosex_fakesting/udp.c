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

#include "globdefs.h"
#include "udp.h"
#include "con_man.h"

//---------------------
// ACSI / CosmosEx stuff
#include "acsi.h"
#include "ce_commands.h"
#include "stdlib.h"

extern BYTE deviceID;
extern BYTE commandShort[CMD_LENGTH_SHORT];
extern BYTE commandLong[CMD_LENGTH_LONG];
extern BYTE *pDmaBuffer;
//---------------------

extern CIB      cibs[MAX_HANDLE];
extern uint32   localIP;

//---------------------

int16 UDP_open (uint32 rem_host, uint16 rem_port)
{
    // first store command code
    commandShort[4] = NET_CMD_UDP_OPEN;
    commandShort[5] = 0;
    
    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeDword   (pBfr, rem_host);
    pBfr = storeWord    (pBfr, rem_port);

    // send it to host
    BYTE res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                                 // if failed, return FALSE 
		return E_UNREACHABLE;
	}

    if(handleIsFromCE(res)) {                       // if it's CE handle
        int stHandle = handleCEtoAtari(res);        // convert it to ST handle

        // store info to CIB and CAB structures
        cibs[stHandle].protocol     = TCP;
        cibs[stHandle].status       = 0;            // 0 means normal
        cibs[stHandle].address.rport    = rem_port; // Remote machine port
        cibs[stHandle].address.rhost    = rem_host; // Remote machine IP address
        cibs[stHandle].address.lport    = 0;        // Local  machine port
        cibs[stHandle].address.lhost    = localIP;  // Local  machine IP address
        
        return stHandle;                            // return the new handle
    } 

    // it's not a CE handle
    return extendByteToWord(res);                   // extend the BYTE error code to WORD
}

int16 UDP_close (int16 handle)
{
    int index;
        
    if(!handles_got(handle, &index)) {          // we don't have this handle? fail
        return E_BADHANDLE;
    }

    // first store command code
    commandShort[4] = NET_CMD_UDP_CLOSE;
    commandShort[5] = 0;
    
    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeWord    (pBfr, handle);

    // send it to host
    WORD res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                             // if failed, return FALSE 
		return 0;
	}

    // TODO: more handling here

    return (E_BADHANDLE);
}

int16 UDP_send(int16 handle, void *buffer, int16 length)
{
    int index;

    if(!handles_got(handle, &index)) {          // we don't have this handle? fail
        return E_BADHANDLE;
    }

    // first store command code
    commandShort[4] = NET_CMD_UDP_SEND;
    commandShort[5] = 0;
    
    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeWord    (pBfr, handle);
    pBfr = storeWord    (pBfr, length);

    // TODO: send the actual buffer
    
    // send it to host
    WORD res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                             // if failed, return FALSE 
		return 0;
	}

    // TODO: more handling here
    
    return (E_BADHANDLE);
}

