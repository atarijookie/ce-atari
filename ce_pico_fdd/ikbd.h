#ifndef __IKBD_H__
#define __IKBD_H__

// tags to distinguish keyboard data from ST commands
#define UARTMARK_STCMD      0xAA
#define UARTMARK_KEYBDATA   0xBB
#define UARTMARK_ALIVE      0xEE

void ikbdHandling(void);

#endif
