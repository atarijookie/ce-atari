#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "acsi.h"
#include "translated.h"
#include "gemdos.h"
#include "main.h"

int32_t custom_mediach( void *sp )
{
/*
DEFINE_PARAMS( trap_w );
CALL( Mediach, params->param1 );
*/

	return 0;
}

int32_t custom_drvmap( void *sp )
{
//	CALL( Drvmap );

	return 0;
}