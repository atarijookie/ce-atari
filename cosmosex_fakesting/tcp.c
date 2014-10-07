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
#include "tcp.h"

int16  /* cdecl */  TCP_open (uint32 rem_host, uint16 rem_port, uint16 tos, uint16 buff_size)
{
   return (E_UNREACHABLE);
}


int16  /* cdecl */  TCP_close (connec, mode, result)

int16  connec, mode, *result;

{
   return (E_BADHANDLE);
 }


int16  /* cdecl */  TCP_send (connec, buffer, length)

int16  connec, length;
void   *buffer;

{
   return (E_BADHANDLE);
 }


int16  /* cdecl */  TCP_wait_state (connec, state, timeout)

int16  connec, state, timeout;

{
   return (E_BADHANDLE);
 }


int16  /* cdecl */  TCP_ack_wait (connec, timeout)

int16  connec, timeout;

{
   return (E_BADHANDLE);
 }


int16  /* cdecl */  TCP_info (connec, tcp_info)

int16  connec;
void   *tcp_info;

{
   return (E_BADHANDLE);
}
