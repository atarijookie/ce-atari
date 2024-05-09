#ifndef _IKBD_TXREADY_H_
#define _IKBD_TXREADY_H_

#include "../../libacsiscsi/global.h"

void test_ikbd_txready_init();
uint8_t test_ikbd_txready_run();
void test_ikbd_txready_teardown();

extern TTestIf test_ikbd_txready;

#endif
