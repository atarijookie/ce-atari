#ifndef _IKBD_SEND_H_
#define _IKBD_SEND_H_

#include "../../libacsiscsi/global.h"

void test_ikbd_send_init();
uint8_t test_ikbd_send_run();
void test_ikbd_send_teardown();

extern TTestIf test_ikbd_send;

#endif
