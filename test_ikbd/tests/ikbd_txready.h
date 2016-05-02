#ifndef _IKBD_TXREADY_H_
#define _IKBD_TXREADY_H_

#include "../global.h"

void test_ikbd_txready_init();
BYTE test_ikbd_txready_run();
void test_ikbd_txready_teardown();

extern TTestIf test_ikbd_txready;

#endif
