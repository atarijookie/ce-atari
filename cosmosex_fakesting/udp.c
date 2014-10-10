//----------------------------------------
// CosmosEx fake STiNG - by Jookie, 2014
// Based on sources of original STiNG
//----------------------------------------

#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>
#include <support.h>
#include <stdint.h>

#include <stdio.h>
#include <string.h>

#include "globdefs.h"
#include "udp.h"

int16  /* cdecl */  UDP_open (uint32 rem_host, uint16 rem_port)
{

   return (E_UNREACHABLE);
}

int16  /* cdecl */  UDP_close (int16 connec)
{

   return (E_BADHANDLE);
}

int16  /* cdecl */  UDP_send (int16 connec, void *buffer, int16 length)
{

   return (E_BADHANDLE);
}

