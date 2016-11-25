#include <mint/osbind.h> 
#include "../global.h"
#include "../helper/ikbd.h"
#include "../helper/commands.h"
#include "../stdlib.h"
#include "../VT52.h"
#include "test.h"

void showHexByte (BYTE bfr);
void showHexBytes(BYTE *bfr, int cnt);

#define ABSMOUSEBTN_LEFTUP  0x08
#define ABSMOUSEBTN_LEFTDN  0x04
#define ABSMOUSEBTN_RIGHTUP 0x02
#define ABSMOUSEBTN_RIGHTDN 0x01

#define PRESS_ANY_KEY       { ikbd_enable_irq(); Cnecin(); ikbd_disable_irq(); }

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

int testCorner(const char *subTestName, const char *message, int upLeftNotBtmRight)
{
    // show message
    VT52_Goto_pos(0, 20);
    (void) Cconws(message);

    // wait in loop for user to move to the corner
    BYTE response[6];
    while(1) {
        ASSERT_SUCCESS( ikbd_puts((const BYTE []) {0x0d},   1), subTestName )   // send QUERY cmd
        ASSERT_SUCCESS( ikbd_gets(response,                 6), subTestName )   // get  QUERY response
    
        if((response[1] & ABSMOUSEBTN_LEFTDN) != 0) {                           // left mouse down? quit loop
            break;
        }
        
        VT52_Goto_pos(0, 21);
        (void) Cconws("Mouse pos: [");
        showHexBytes(response + 2, 2);
        (void) Cconws(",");
        showHexBytes(response + 4, 2);
        (void) Cconws("]");
    }
    
    // clear the message 
    VT52_Goto_pos(0, 20);
    VT52_Clear_line();
    VT52_Goto_pos(0, 21);
    VT52_Clear_line();
    
    // wait for mouse up
    while(1) {
        ASSERT_SUCCESS( ikbd_puts((const BYTE []) {0x0d},   1), subTestName )   // send QUERY cmd
        ASSERT_SUCCESS( ikbd_gets(response,                 6), subTestName )   // get  QUERY response
        
        if((response[1] & ABSMOUSEBTN_LEFTUP) != 0) {                           // left mouse up? quit loop
            break;
        }
    }
    
    // check the coordinates
    const BYTE leftUp[4]    = {0, 0, 0, 0};
    const BYTE rightDown[4] = {3, 0, 3, 0};
    const BYTE *coords      = upLeftNotBtmRight ? leftUp : rightDown;           // choose with what it should be compared
    
    int res = memcmp(coords, response + 2, 4);                                  // check if the coordinates match
    if(res != 0) {
        TEST_FAIL_REASON(subTestName);
        return FALSE;
    }

    return TRUE;
}

int testClick(const char *subTestName, const char *message, int upLeftNotBtmRight)
{
    BYTE response[6];

    // show message
    VT52_Goto_pos(0, 20);
    (void) Cconws(message);

    // prepare mask for up and down button events
    WORD maskUp     = upLeftNotBtmRight ? ABSMOUSEBTN_LEFTUP : ABSMOUSEBTN_RIGHTUP;
    WORD maskDown   = upLeftNotBtmRight ? ABSMOUSEBTN_LEFTDN : ABSMOUSEBTN_RIGHTDN;
    
    // wait for down event
    while(1) {
        ASSERT_SUCCESS( ikbd_puts((const BYTE []) {0x0d},   1), subTestName )   // send QUERY cmd
        ASSERT_SUCCESS( ikbd_gets(response,                 6), subTestName )   // get  QUERY response
    
        if(response[1] & maskDown) {            // if the propper button is down, quit the loop
            break;
        }
        
        if(response[1] & (~maskDown)) {         // if something else, fail
            TEST_FAIL_REASON("down: wrong button event: ");
            showHexByte(response[1]);
            return FALSE;
        }    
    }

    // wait for up event
    while(1) {
        ASSERT_SUCCESS( ikbd_puts((const BYTE []) {0x0d},   1), subTestName )   // send QUERY cmd
        ASSERT_SUCCESS( ikbd_gets(response,                 6), subTestName )   // get  QUERY response
    
        if(response[1] & maskUp) {              // if the propper button is up, quit the loop
            break;
        }
        
        if(response[1] & (~maskUp)) {           // if something else, fail
            TEST_FAIL_REASON("up: wrong button event: ");
            showHexByte(response[1]);
            return FALSE;
        }    
    }
    
    // clear the message 
    VT52_Goto_pos(0, 20);
    VT52_Clear_line();

    // success
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
    
    res = testCorner("E4", "Go to upper left corner and click", 1);
    if(!res) return FALSE;

    res = testCorner("E5", "Go to bottom right corner and click", 0);
    if(!res) return FALSE;

    res = testClick("E6", "Do a left mouse click", 1);
    if(!res) return FALSE;

    res = testClick("E7", "Do a right mouse click", 0);
    if(!res) return FALSE;
    
    return TRUE;
}

void test_mouse_loadpos_teardown()
{
	ikbd_enable_irq(); 	//disable KBD IRQ so TOS doesn't recieve IKBD return values
}

TTestIf test_mouse_loadpos = {&test_mouse_loadpos_init, &test_mouse_loadpos_run, &test_mouse_loadpos_teardown};

