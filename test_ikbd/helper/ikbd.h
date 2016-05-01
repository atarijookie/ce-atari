#include "../global.h"
	
BYTE ikbd_ws( const BYTE* ikbdData, int len );
BYTE ikbd_get( BYTE* result );
BYTE ikbd_disable_irq();
BYTE ikbd_enable_irq();
