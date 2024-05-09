#ifndef _IKBD_RESET_H_
#define _IKBD_RESET_H_

#include "../../libacsiscsi/global.h"

void test_ikbd_reset_init();
uint8_t test_ikbd_reset_run();
void test_ikbd_reset_teardown();

extern TTestIf test_ikbd_reset;

#endif
