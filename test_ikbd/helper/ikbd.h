#ifndef _IKBD_H_
#define _IKBD_H_

#include "../../libacsiscsi/global.h"

uint8_t ikbd_txready(void);
	
uint8_t ikbd_put (const uint8_t data);
uint8_t ikbd_get (uint8_t* retval);

uint8_t ikbd_puts(const uint8_t *ikbdData,  int len);
uint8_t ikbd_gets(      uint8_t *outString, int len);

void ikbd_disable_irq(void);
void ikbd_enable_irq (void);

#endif
