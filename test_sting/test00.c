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

void doTest00(void)
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

