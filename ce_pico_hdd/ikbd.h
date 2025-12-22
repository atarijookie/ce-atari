#ifndef __IKBD_H__
#define __IKBD_H__

#include <arduino.h>

#define IKBD_BAUD_RATE  7812

// tags to distinguish keyboard data from ST commands
#define UARTMARK_STCMD      0xAA
#define UARTMARK_KEYBDATA   0xBB
#define UARTMARK_ALIVE      0xEE

void ikbdInit(void);
void ikbdProcessing(void);

#endif
