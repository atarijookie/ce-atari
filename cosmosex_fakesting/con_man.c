#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>
#include <stdint.h>
#include <stdio.h>

#include "globdefs.h"
#include "con_man.h"

int16  /* cdecl */  CNkick (int16 connec)
{

	return (E_BADHANDLE);
}

int16  /* cdecl */  CNbyte_count (int16 connec)
{

   return 0;
}

int16  /* cdecl */  CNget_char (int16 connec)
{

   return 0;
}


NDB *  /* cdecl */  CNget_NDB (int16 connec)
{

   return 0;
}

int16  /* cdecl */  CNget_block (int16 connec, void *buffer, int16 length)
{

	return 0;
}

CIB *  /* cdecl */  CNgetinfo (int16 connec)
{

	return 0;
}

int16  /* cdecl */  CNgets (int16 connec, char *buffer, int16 length, char delimiter)
{

	return 0;
}