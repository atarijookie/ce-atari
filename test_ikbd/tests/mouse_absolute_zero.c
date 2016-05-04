#include <mint/osbind.h> 
#include "../global.h"
#include "../helper/ikbd.h"
#include "../helper/commands.h"
#include "../stdlib.h"
#include "../VT52.h"
#include "test.h"
#include "mouse_absolute_zero.h"

void showHexBytes(BYTE *bfr, int cnt);

void test_mouse_absolute_zero_init()
{
	(void) Cconws("     test_mouse_absolute_zero ");
	ikbd_disable_irq(); 	//disable KBD IRQ so TOS doesn't recieve IKBD return values
	ikbd_reset();
}

BYTE test_mouse_absolute_zero_run()
{
    BYTE response[8];
    
    ASSERT_SUCCESS( ikbd_puts((const BYTE []) {0x88},   1), "E1" )              // send QUERY cmd
    ASSERT_SUCCESS( ikbd_gets(response,                 8), "E1" )              // get  QUERY response
        
    int res = memcmp((const BYTE []) {0xf6, 8, 0,0, 0,0, 0,0}, response, 8);    // should report RELATIVE

    if(res != 0) {
        TEST_FAIL_REASON(" bad inquiry data\r\n");
        showHexBytes(response, 8);
        return FALSE;
    }
    
	return TRUE;
}

void test_mouse_absolute_zero_teardown()
{
}

TTestIf test_mouse_absolute_zero={&test_mouse_absolute_zero_init, &test_mouse_absolute_zero_run, &test_mouse_absolute_zero_teardown};
