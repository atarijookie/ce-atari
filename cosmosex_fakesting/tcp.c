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
#include "tcp.h"
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

int16 TCP_open(uint32 rem_host, uint16 rem_port, uint16 tos, uint16 buff_size)
{
    // first store command code
    commandShort[4] = NET_CMD_TCP_OPEN;
    commandShort[5] = 0;
    
    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeDword   (pBfr, rem_host);
    pBfr = storeWord    (pBfr, rem_port);
    pBfr = storeWord    (pBfr, tos);
    pBfr = storeWord    (pBfr, buff_size);

    // send it to host
    WORD res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                        										// if failed, return FALSE 
		return 0;
	}

    // TODO: more handling here
    
    return (E_UNREACHABLE);
}

int16 TCP_close(int16 handle, int16 mode, int16 *result)
{
    if(!handles_got(handle)) {          // we don't have this handle? fail
        return E_BADHANDLE;
    }
    
    // first store command code
    commandShort[4] = NET_CMD_TCP_CLOSE;
    commandShort[5] = 0;
    
    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeWord    (pBfr, handle);
    pBfr = storeWord    (pBfr, mode);

    // send it to host
    WORD res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                        										// if failed, return FALSE 
		return 0;
	}

    // TODO: more handling here
    
    return (E_BADHANDLE);
}

int16 TCP_send(int16 handle, void *buffer, int16 length)
{
    if(!handles_got(handle)) {          // we don't have this handle? fail
        return E_BADHANDLE;
    }

    // first store command code
    commandShort[4] = NET_CMD_TCP_SEND;
    commandShort[5] = 0;
    
    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeWord    (pBfr, handle);
    pBfr = storeWord    (pBfr, length);

    // TODO: send the buffer
    
    // send it to host
    WORD res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                        										// if failed, return FALSE 
		return 0;
	}

    // TODO: more handling here

    return (E_BADHANDLE);
}

int16 TCP_wait_state(int16 handle, int16 state, int16 timeout)
{
    if(!handles_got(handle)) {          // we don't have this handle? fail
        return E_BADHANDLE;
    }

    // first store command code
    commandShort[4] = NET_CMD_TCP_WAIT_STATE;
    commandShort[5] = 0;
    
    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeWord    (pBfr, handle);
    pBfr = storeWord    (pBfr, state);
    pBfr = storeWord    (pBfr, timeout);

    // send it to host
    WORD res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                        										// if failed, return FALSE 
		return 0;
	}

    // TODO: more handling here

    return (E_BADHANDLE);
}

int16 TCP_ack_wait(int16 handle, int16 timeout)
{
    if(!handles_got(handle)) {          // we don't have this handle? fail
        return E_BADHANDLE;
    }

    // first store command code
    commandShort[4] = NET_CMD_TCP_ACK_WAIT;
    commandShort[5] = 0;
    
    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeWord    (pBfr, handle);
    pBfr = storeWord    (pBfr, timeout);

    // send it to host
    WORD res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                        										// if failed, return FALSE 
		return 0;
	}

    // TODO: more handling here
    
    return (E_BADHANDLE);
}

int16 TCP_info(int16 handle, void *tcp_info)
{
    if(!handles_got(handle)) {          // we don't have this handle? fail
        return E_BADHANDLE;
    }

    // first store command code
    commandShort[4] = NET_CMD_TCP_INFO;
    commandShort[5] = 0;
    
    // then store the params in buffer
    BYTE *pBfr = pDmaBuffer;
    pBfr = storeWord    (pBfr, handle);

    // send it to host
    WORD res = acsi_cmd(ACSI_WRITE, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);

	if(res != OK) {                        										// if failed, return FALSE 
		return 0;
	}

    // TODO: more handling here
    
    return (E_BADHANDLE);
}
