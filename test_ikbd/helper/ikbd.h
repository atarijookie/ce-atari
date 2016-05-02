#ifndef _IKBD_H_
#define _IKBD_H_

#include "../global.h"

BYTE ikbd_txready(void);
	
BYTE ikbd_put (const BYTE data);
BYTE ikbd_get (BYTE* retval);

BYTE ikbd_puts(const BYTE *ikbdData,  int len);
BYTE ikbd_gets(      BYTE *outString, int len);

void ikbd_disable_irq(void);
void ikbd_enable_irq (void);

#endif
