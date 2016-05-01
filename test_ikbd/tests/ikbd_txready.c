#include <mint/osbind.h> 
#include "../global.h"
#include "test.h"
#include "../helper/ikbd.h"
#include "ikbd_txready.h"

TTestIf test_ikbd_txready={&test_ikbd_txready_init,&test_ikbd_txready_run,&test_ikbd_txready_teardown};

void test_ikbd_txready_init()
{
	(void) Cconws("     test_ikbd_txready ");
}

BYTE test_ikbd_txready_run()
{
	//check if TX is ready
	ASSERT_SUCCESS( ikbd_txready(), "IKBD TX didn't get ready" )
	return TRUE;
}

void test_ikbd_txready_teardown()
{
}