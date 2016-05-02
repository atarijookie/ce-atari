#ifndef _IKBD_RESET_H_
#define _IKBD_RESET_H_

#include "../global.h"

void test_ikbd_reset_init();
BYTE test_ikbd_reset_run();
void test_ikbd_reset_teardown();

extern TTestIf test_ikbd_reset;

#endif
