#include <mint/osbind.h> 
#include "../global.h"
#include "test.h"
#include "../helper/ikbd.h"
#include "ikbd_send.h"

void test_ikbd_send_init()
{
	(void) Cconws("     test_ikbd_send ");
}

BYTE test_ikbd_send_run()
{
	//send a byte
	ASSERT_SUCCESS( ikbd_put(0), "Could not send byte to IKBD" )
	return TRUE;
}

TTestIf test_ikbd_send={&test_ikbd_send_init, &test_ikbd_send_run, 0};
