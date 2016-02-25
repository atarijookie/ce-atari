//--------------------------------------------------
#include <mint/osbind.h> 
#include <stdio.h>

#include "version.h"
#include "global.h"
#include "transprt.h"
#include "stdlib.h"
#include "out.h"

//--------------------------------------------------
extern TPL *tpl;
extern BYTE *rBuf, *wBuf;

void testInvalidHandles(void);
void testMaxTcpBuffSize(void);
int  tryTcpSend(int handle, int size);

void doTest00(void)
{
    //--------------------
    // test invalid handles
    testInvalidHandles();   

    //--------------------
    // test maximum TCP block size
    testMaxTcpBuffSize();
    
    
}

void testInvalidHandles(void)
{
    int res, ok;
    char bfr[4];
    memset(bfr, 0, 4);

    #define INVALID_HANDLE_NO       10

    //--------------------------------------
    // TCP functions
    res = TCP_close(INVALID_HANDLE_NO, 0, 0);
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0001, "TCP_close() - on invalid handle");
    out_result_error(ok, res);

    res = TCP_send(INVALID_HANDLE_NO, bfr, 4);
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0002, "TCP_send() - on invalid handle");
    out_result_error(ok, res);
    
    res = TCP_wait_state(INVALID_HANDLE_NO, TESTABLISH, 1);
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0003, "TCP_wait_state() - on invalid handle");
    out_result_error(ok, res);

    res = TCP_ack_wait(INVALID_HANDLE_NO, 0);
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0004, "TCP_ack_wait() - on invalid handle");
    out_result_error(ok, res);
    
    //--------------------------------------
    // UDP functions
    res = UDP_close(INVALID_HANDLE_NO);
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0011, "UDP_close() - on invalid handle");
    out_result_error(ok, res);

    res = UDP_send(INVALID_HANDLE_NO, bfr, 4);
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0012, "UDP_send() - on invalid handle");
    out_result_error(ok, res);
    
    //--------------------------------------
    // Connection Manager functions
    res = CNkick(INVALID_HANDLE_NO);
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0021, "CNkick() - on invalid handle");
    out_result_error(ok, res);

    res = CNbyte_count(INVALID_HANDLE_NO);
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0022, "CNbyte_count() - on invalid handle");
    out_result_error(ok, res);

    res = CNget_char(INVALID_HANDLE_NO);
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0023, "CNget_char() - on invalid handle");
    out_result_error(ok, res);

    res = (int) CNget_NDB(INVALID_HANDLE_NO);
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0024, "CNget_NDB() - on invalid handle");
    out_result_error(ok, res);

    res = CNget_block(INVALID_HANDLE_NO, bfr, 4);
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0025, "CNget_block() - on invalid handle");
    out_result_error(ok, res);

    res = (int) CNgetinfo(INVALID_HANDLE_NO);
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0026, "CNgetinfo() - on invalid handle");
    out_result_error(ok, res);

    /*
    res = CNgets(INVALID_HANDLE_NO, bfr, 4, '\n');
    ok  = (res == E_BADHANDLE) ? 1 : 0;
    out_test_header(0x0027, "CNgets() - on invalid handle");
    out_result_error(ok, res);
    */
}

void testMaxTcpBuffSize(void)
{
    int handle, res, ok;
    
    out_test_header(0x0030, "TCP open + send less than allowed");
    handle = TCP_open(SERVER_ADDR, SERVER_PORT_START, 0, 2000);
    
    if(handle < 0) {
        out_result_error_string(0, handle, "TCP_open failed");
        return;
    }
    
    res = tryTcpSend(handle, 1000);         // send less than allowed
    ok  = (res == E_NORMAL) ? 1 : 0;
    out_result_error(ok, res);
    
    //------------
    out_test_header(0x0031, "TCP open + send exactly as much as allowed");
    res = tryTcpSend(handle, 2000);         // send exactly as much as allowed
    ok  = (res == E_NORMAL) ? 1 : 0;
    out_result_error(ok, res);

    //------------
    out_test_header(0x0032, "TCP open + send more than allowed");
    res = tryTcpSend(handle, 2001);         // send more than allowed
    ok  = (res == E_OBUFFULL) ? 1 : 0;
    out_result_error(ok, res);

    //------------

    TCP_close(handle, 0, 0);
}

int tryTcpSend(int handle, int size)
{
    int res;
    DWORD now, start;
    
    start = getTicks();
    
    while(1) {
        res = TCP_send(handle, wBuf, size);
    
        now = getTicks();
        
        if((now - start) >= 200) {                  // if timeout, quit
            return -64;                             // custom timeout error code
        }

        if(res == E_OBUFFULL) {                     // buffer full? wait more
            continue;
        }
    
        if(res == E_NORMAL) {                       // good? success!
            return E_NORMAL;
        }
        
        break;                                      // if came here, it's not success and not full buffer
    }
    
    return res;                                     // fail
}
