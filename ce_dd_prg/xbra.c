#include "xbra.h"

#include <mint/osbind.h>

LONG
unhook_xbra( WORD vecnum, LONG app_id )
{
    XBRA *rx;
    LONG vecadr, *stepadr, savessp, lret = 0L;

    vecadr = (LONG)Setexc( vecnum, VEC_INQUIRE );
    rx = (XBRA *)(vecadr - sizeof( XBRA ));

    if( (vecadr >= 0x00E00000 && vecadr <= 0x00EFFFFF)
	|| (vecadr >= 0x00FA0000 && vecadr <= 0x00FEFFFF) )
    {
	/* It's a ROM vector */
	return 0L;
    }

    /* Set supervisor mode for search just in case. */
    savessp = Super( SUP_SET );

    /* do we have an XBRA-structure? */
    if( rx->xbra_id != 'XBRA' )
    {
    	Super( savessp );
    	return 0L;
    }

    /* Special Case: Vector to remove is first in chain. */
    if( rx->xbra_id == 'XBRA' && rx->app_id == app_id )
    {
	Super( savessp );
        (void)Setexc( vecnum, rx->oldvec );
        return vecadr;
    }

    stepadr = (LONG *)&rx->oldvec;

    rx = (XBRA *)((LONG)rx->oldvec - sizeof( XBRA ));
    while( rx!=0 && rx->oldvec!=0 && rx->xbra_id == 'XBRA' )
    {
        if( rx->app_id == app_id )
        {
            *stepadr = lret = (LONG)rx->oldvec;
            break;
        }

        stepadr = (LONG *)&rx->oldvec;
        rx = (XBRA *)((LONG)rx->oldvec - sizeof( XBRA ));
    }

    Super( savessp );
    return lret;
}
