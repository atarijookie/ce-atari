#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>
#include <stdint.h>
#include <stdio.h>

#include "globdefs.h"
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

//--------------------------------------

CIB cibs[MAX_HANDLE];

//--------------------------------------
// connection info function

int16 CNkick (int16 handle)
{
    update_con_info();                  // update connections info structs (max once per 100 ms)

    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return E_BADHANDLE;
    }

	return E_NORMAL;
}

CIB *CNgetinfo (int16 handle)
{
    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return (CIB *) NULL;
    }

    update_con_info();                  // update connections info structs (max once per 100 ms)

	return &cibs[handle];               // return pointer to correct CIB
}

int16 CNbyte_count (int16 handle)
{
    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return E_BADHANDLE;
    }

    update_con_info();                  // update connections info structs (max once per 100 ms)


    
    
    return 0;
}

//-------------------------------------
// data retrieval functions
int16 CNget_char (int16 handle)
{   
    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return E_BADHANDLE;
    }


    return 0;
}

NDB *CNget_NDB (int16 handle)
{
    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return (NDB *) NULL;
    }

    
    return 0;
}

int16 CNget_block (int16 handle, void *buffer, int16 length)
{
    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return E_BADHANDLE;
    }

    
    return 0;
}

int16 CNgets (int16 handle, char *buffer, int16 length, char delimiter)
{
    if(!handle_valid(handle)) {         // we don't have this handle? fail
        return E_BADHANDLE;
    }

    
	return 0;
}

//--------------------------------------
// helper functions

void structs_init(void)
{
    int i;
    
    for(i=0; i<MAX_HANDLE; i++) {
        memset(&cibs[i], 0, sizeof(CIB));
    }
}

int handle_valid(int16 h)
{
    if(h >=0 && h <= MAX_HANDLE) {
        return TRUE;
    }
    
    return FALSE;
}

void update_con_info(void)
{
	DWORD res;
	static DWORD lastUpdate = 0;
	DWORD now = *HZ_200;

	if((now - lastUpdate) < 20) {								            // if the last update was less than 100 ms ago, don't update
		return;
	}
	
	lastUpdate = now;											            // mark that we've just updated the ceDrives 
	
	// now do the real update 
	commandShort[4] = NET_CMD_CN_UPDATE_INFO;								// store function number 
	commandShort[5] = 0;										
	
	res = acsi_cmd(ACSI_READ, commandShort, CMD_LENGTH_SHORT, pDmaBuffer, 1);   // send command to host over ACSI 
	
    if(res == OK) {							                                // error? 
		return;														
	}



        // TODO: move the data from buffer to structs
}

//--------------------------------------

