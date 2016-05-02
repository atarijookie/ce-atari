#include <mint/osbind.h> 
#include "../global.h"
#include "../helper/ikbd.h"
#include "../helper/commands.h"
#include "../stdlib.h"
#include "test.h"

void showHexBytes(BYTE *bfr, int cnt);

void test_mouse_loadpos_init()
{
	(void) Cconws("     test_mouse_loadpos");
	ikbd_disable_irq(); 	//disable KBD IRQ so TOS doesn't recieve IKBD return values
	ikbd_reset();
}

BYTE test_mouse_loadpos_run()
{
    //------------------
    // set mouse to [0,0], read back
    ASSERT_SUCCESS( ikbd_puts((const BYTE []) {0x09,0,0,0,0}, 5), "puts 1 failed" )     // set mouse pos
    ASSERT_SUCCESS( ikbd_puts((const BYTE []) {0x89,0,0,0,0}, 5), "puts 2 failed" )     // status inquiry
    
    BYTE resp[8];
    ASSERT_SUCCESS( ikbd_gets(resp, 8), "gets 1 failed" )
    
    BYTE res = memcmp(resp, (const BYTE []) {0xf6, 9, 0, 0, 0, 0, 0, 0}, 8);            // compare with expected result

    if(res != 0) {
        TEST_FAIL_REASON( "Wrong received data 1.\r\nData: " );
        showHexBytes(resp, 8);
        return FALSE;
    }

    //------------------
    // set mouse to [111,222], read back
    ASSERT_SUCCESS( ikbd_puts((const BYTE []) {0x09,0,111,0,222}, 5),   "puts 3 failed" )   // set mouse pos
    ASSERT_SUCCESS( ikbd_puts((const BYTE []) {0x89,0,0,0,0}, 5),       "puts 4 failed" )   // status inquiry

    ASSERT_SUCCESS( ikbd_gets(resp, 8), "gets 2 failed" )
    
    res = memcmp(resp, (const BYTE []) {0xf6, 9, 0, 111, 0, 222, 0, 0}, 8);            // compare with expected result

    if(res != 0) {
        TEST_FAIL_REASON( "Wrong received data 2.\r\nData: " );
        showHexBytes(resp, 8);
        return FALSE;
    }
    
    return TRUE;
}

void test_mouse_loadpos_teardown()
{
	ikbd_enable_irq(); 	//disable KBD IRQ so TOS doesn't recieve IKBD return values
}

TTestIf test_mouse_loadpos = {&test_mouse_loadpos_init, &test_mouse_loadpos_run, &test_mouse_loadpos_teardown};

