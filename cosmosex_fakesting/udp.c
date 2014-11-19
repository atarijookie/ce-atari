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

//---------------------

int16 UDP_open (uint32 rem_host, uint16 rem_port)
{
    return connection_open(0, rem_host, rem_port, 0, 0);
}

int16 UDP_close (int16 handle)
{
    return connection_close(0, handle, 0, NULL);
}

int16 UDP_send(int16 handle, void *buffer, int16 length)
{
    return connection_send(0, handle, buffer, length);
}

