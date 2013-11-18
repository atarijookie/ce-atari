#include <mint/osbind.h>
#include <mint/basepage.h>

#include <stdint.h>
#include <stdio.h>

#include "xbra.h"

/*
 * Simple GEMDOS handler.
 * 14/11/2013 Miro Kropacek
 * miro.kropacek@gmail.com
 */

typedef void  (*TrapHandlerPointer)( void );

extern void gemdos_handler( void );
extern TrapHandlerPointer old_gemdos_handler;
int32_t (*gemdos_table[256])( void* sp ) = { 0 };
int16_t gemdos_on = 0;	/* 0: use new handlers, 1: use old handlers */

static int32_t custom_dgetdrv( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_dsetdrv( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fsetdta( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fgetdta( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_dfree( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_dcreate( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_ddelete( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_dsetpath( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fcreate( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fopen( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fclose( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fread( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fwrite( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fdelete( void *sp )
{
	static char buff[256];
	/* pretend the params are array of strings */
	char** args = (char**)sp;
	
	gemdos_on = 1;
	Fdelete( args[0] );
	gemdos_on = 0;

#ifndef SLIM
	/* be nice on supervisor stack */
	sprintf( buff, "arg: %s\n", args[0] );
	(void)Cconws(buff);
#endif

	/* not handled */
	return 0;
}

static int32_t custom_fseek( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fattrib( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_dgetpath( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fsfirst( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fsnext( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_frename( void *sp )
{
	/* not handled */
	return 0;
}

static int32_t custom_fdatime( void *sp )
{
	/* not handled */
	return 0;
}

int main( int argc, char* argv[] )
{
	gemdos_table[0x0e] = custom_dsetdrv;
	gemdos_table[0x1a] = custom_fsetdta;
	gemdos_table[0x19] = custom_dgetdrv;
	gemdos_table[0x1a] = custom_fsetdta;
	gemdos_table[0x2f] = custom_fgetdta;
	gemdos_table[0x36] = custom_dfree;
	gemdos_table[0x39] = custom_dcreate;
	gemdos_table[0x3a] = custom_ddelete;
	gemdos_table[0x3b] = custom_dsetpath;
	gemdos_table[0x3c] = custom_fcreate;
	gemdos_table[0x3d] = custom_fopen;
	gemdos_table[0x3e] = custom_fclose;
	gemdos_table[0x3f] = custom_fread;
	gemdos_table[0x40] = custom_fwrite;
	gemdos_table[0x41] = custom_fdelete;
	gemdos_table[0x42] = custom_fseek;
	gemdos_table[0x43] = custom_fattrib;
	gemdos_table[0x47] = custom_dgetpath;
	gemdos_table[0x4e] = custom_fsfirst;
	gemdos_table[0x4f] = custom_fsnext;
	gemdos_table[0x56] = custom_frename;
	gemdos_table[0x57] = custom_fdatime;

	/* either remove the old one or do nothing
	 * NOTE: memory occupied by the old code isn't released!
	 */
	if( unhook_xbra( VEC_GEMDOS, 'MKRO' ) == 0L )
	{
		(void)Cconws( "New GEMDOS handler installed.\r\n" );
	}
	else
	{
		(void)Cconws( "New GEMDOS handler has replaced the old one.\r\n" );
	}

	old_gemdos_handler = Setexc( VEC_GEMDOS, gemdos_handler );
	
	Ptermres( 0x100 + _base->p_tlen + _base->p_dlen + _base->p_blen, 0 );

	/* make compiler happy, we wont return */
	return 0;
}
