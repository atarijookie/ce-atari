#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>
#include <stdint.h>
#include <stdio.h>

#include "globdefs.h"
#include "con_man.h"

//--------------------------------------

int16 handles[MAX_HANDLE];

//--------------------------------------
// client API functions
int16 CNkick (int16 handle)
{
    // will do nothing...
    
    if(!handles_got(handle)) {          // we don't have this handle? fail
        return E_BADHANDLE;
    }
    
    
	return E_NORMAL;
}

int16 CNbyte_count (int16 handle)
{
    if(!handles_got(handle)) {          // we don't have this handle? fail
        return E_BADHANDLE;
    }

    
    return 0;
}

int16 CNget_char (int16 handle)
{
    if(!handles_got(handle)) {          // we don't have this handle? fail
        return E_BADHANDLE;
    }


    return 0;
}

NDB *CNget_NDB (int16 handle)
{
    if(!handles_got(handle)) {          // we don't have this handle? fail
        return (NDB *) NULL;
    }

    
    return 0;
}

int16 CNget_block (int16 handle, void *buffer, int16 length)
{
    if(!handles_got(handle)) {          // we don't have this handle? fail
        return E_BADHANDLE;
    }

    
    return 0;
}

CIB *CNgetinfo (int16 handle)
{
    if(!handles_got(handle)) {          // we don't have this handle? fail
        return (CIB *) NULL;
    }

    
	return 0;
}

int16 CNgets (int16 handle, char *buffer, int16 length, char delimiter)
{
    if(!handles_got(handle)) {          // we don't have this handle? fail
        return E_BADHANDLE;
    }

    
	return 0;
}

//--------------------------------------
// helper functions

void handles_init(void)
{
    int i;
    
    for(i=0; i<MAX_HANDLE; i++) {
        handles[i] = 0;
    }
}

int handles_got(int16 h)
{
    int i;
    
    for(i=0; i<MAX_HANDLE; i++) {
        if(handles[i] == h) {
            return TRUE;
        }
    }

    return FALSE;
}

//--------------------------------------

