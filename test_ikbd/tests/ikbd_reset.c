#include <mint/osbind.h> 
#include "../global.h"
#include "test.h"
#include "../helper/ikbd.h"
#include "../helper/commands.h"
#include "ikbd_reset.h"

TTestIf test_ikbd_reset={&test_ikbd_reset_init,&test_ikbd_reset_run,&test_ikbd_reset_teardown};

const BYTE test_ikbd_reset_data[]={0x80,0x01};

void test_ikbd_reset_init()
{
	(void) Cconws("     test_ikbd_reset ");
	ikbd_disable_irq(); 	//disable KBD IRQ so TOS doesn't recieve IKBD return values
}

BYTE test_ikbd_reset_run()
{
	BYTE retcode=0;
	//write reset code
	ASSERT_SUCCESS( ikbd_puts(test_ikbd_reset_data,2), "Could not send reset command to IKBD" )
	//check for return code ($f1)
	ASSERT_SUCCESS( ikbd_get(&retcode), "Could not retrieve reset return value from IKBD" )
	ASSERT_EQUAL( retcode, 0xF1, "Retrieved reset return value is !=$F1" )
	return TRUE;
}

void test_ikbd_reset_teardown()
{
	ikbd_enable_irq();
}
