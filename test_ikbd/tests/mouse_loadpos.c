#include <mint/osbind.h> 
#include "../global.h"
#include "../helper/ikbd.h"
#include "../helper/commands.h"
#include "../stdlib.h"
#include "test.h"

void showHexBytes(BYTE *bfr, int cnt);

int setAndQuery(const char *subTestName, const BYTE *setCmd, int setCmdLen, const BYTE *queryCmd, int queryCmdLen, const BYTE *expectedResponse, int expectedResponseLength)
{
    BYTE response[8];
    
    ASSERT_SUCCESS( ikbd_puts(setCmd, setCmdLen),                   subTestName )   // send SET cmd

    ASSERT_SUCCESS( ikbd_puts(queryCmd, queryCmdLen),               subTestName )   // send QUERY cmd
    ASSERT_SUCCESS( ikbd_gets(response, expectedResponseLength),    subTestName )   // get  QUERY response
    
    int res = memcmp(response, expectedResponse, expectedResponseLength);           // compare with expected result
    
    if(res != 0) {
        (void) Cconws(" - ");
        TEST_FAIL_REASON( subTestName );
        (void) Cconws(": ");
        showHexBytes(response, expectedResponseLength);
        return FALSE;
    }
    
    return TRUE;
}

void test_mouse_loadpos_init()
{
	(void) Cconws("     test_mouse_loadpos");
	ikbd_disable_irq(); 	//disable KBD IRQ so TOS doesn't recieve IKBD return values
	ikbd_reset();
}

BYTE test_mouse_loadpos_run()
{
    int res;
    //------------------
    // set mouse to ABS mode
    res = setAndQuery("E1", (const BYTE []) {0x09,3,0,3,0}, 5,              // set ABS mouse with max [0x300, 0x300]
                            (const BYTE []) {0x89},         1,              // inquiry mode
                            (const BYTE []) {0xf6, 9, 3,0, 3,0, 0,0}, 8);   // should report ABS, with max [0x300, 0x300]
    if(!res) return FALSE;

    //------------------
    // set mouse to [0,0], read back
    res = setAndQuery("E2", (const BYTE []) {0x0e,0,0,0,0,0},    6,         // load mouse pos [0,0]
                            (const BYTE []) {0x0d},              1,         // interrogate mouse
                            (const BYTE []) {0xf7, 0, 0,0, 0,0}, 6);        // test the mouse pos [0,0]
    if(!res) return FALSE;

    //------------------
    // set mouse to [0x300,0x300], read back
    res = setAndQuery("E3", (const BYTE []) {0x0e,0, 3,0, 3,0},  6,         // load mouse pos [0x300,0x300]
                            (const BYTE []) {0x0d},              1,         // interrogate mouse
                            (const BYTE []) {0xf7, 0, 3,0, 3,0}, 6);        // test the mouse pos [0x300,0x300]
    if(!res) return FALSE;
    
    return TRUE;
}

void test_mouse_loadpos_teardown()
{
	ikbd_enable_irq(); 	//disable KBD IRQ so TOS doesn't recieve IKBD return values
}

TTestIf test_mouse_loadpos = {&test_mouse_loadpos_init, &test_mouse_loadpos_run, &test_mouse_loadpos_teardown};

