#include "xbra.h"

#include <mint/osbind.h>

uint32_t
unhook_xbra( uint16_t vecnum, uint32_t app_id )
{
    XBRA *rx;
    uint32_t vecadr, *stepadr, savessp, lret = 0L;

    vecadr = (uint32_t)Setexc( vecnum, VEC_INQUIRE );
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

    stepadr = (uint32_t *)&rx->oldvec;

    rx = (XBRA *)((uint32_t)rx->oldvec - sizeof( XBRA ));
    while( rx!=0 && rx->oldvec!=0 && rx->xbra_id == 'XBRA' )
    {
        if( rx->app_id == app_id )
        {
            *stepadr = lret = (uint32_t)rx->oldvec;
            break;
        }

        stepadr = (uint32_t *)&rx->oldvec;
        rx = (XBRA *)((uint32_t)rx->oldvec - sizeof( XBRA ));
    }

    Super( savessp );
    return lret;
}
