#ifndef _IKBD_H_
#define _IKBD_H_

#include "../global.h"
	
BYTE ikbd_ws (const BYTE* ikbdData, int len);
BYTE ikbd_put(BYTE data);
BYTE ikbd_get(BYTE* retval);
BYTE ikbd_txready(void);
void ikbd_disable_irq();
void ikbd_enable_irq ();

#endif
