//--------------------------------------------------
#include <mint/osbind.h> 
#include <stdio.h>

#include "version.h"
#include "transprt.h"
#include "global.h"
#include "stdlib.h"
#include "out.h"

//--------------------------------------------------
extern TPL  *tpl;
extern BYTE *rBuf, *wBuf;

void doTest0300(void);
int16 cdecl receive_echo(IP_DGRAM  *datagram);
void        send_echo(void);

#define  ICMP_ECHO_REPLY      0
#define  ICMP_ECHO            8

WORD received = 0;

void doTest03(void)
{
    doTest0300();
}

void doTest0300(void)
{
    int res, ok;

    //------------
    // test NULL as handler pointer
    out_test_header(0x0301, "ICMP_handler(NULL, HNDLR_SET)");
    res = ICMP_handler(0, HNDLR_SET);
    ok  = (res == TRUE) ? 1 : 0;        // it seems that you can set NULL as a handler
    out_result_error(ok, res);
    
    out_test_header(0x0302, "ICMP_handler(NULL, HNDLR_FORCE)");
    res = ICMP_handler(0, HNDLR_FORCE);
    ok  = (res == FALSE) ? 1 : 0;       // this is the same as HNDLR_SET for ICMP, so it will fail, as the NULL handler already exists
    out_result_error(ok, res);

    out_test_header(0x0303, "ICMP_handler(NULL, HNDLR_QUERY)");
    res = ICMP_handler(0, HNDLR_QUERY);
    ok  = (res == TRUE) ? 1 : 0;        // query will succeed, NULL handler exists
    out_result_error(ok, res);
    
    out_test_header(0x0304, "ICMP_handler(NULL, HNDLR_REMOVE)");
    res = ICMP_handler(0, HNDLR_REMOVE);
    ok  = (res == TRUE) ? 1 : 0;        // as NULL was set by ICMP_handler() call, this will succeed and remove it from list of handlers
    out_result_error(ok, res);
    
    //------------
    // test real function pointer
    ICMP_handler (receive_echo, HNDLR_REMOVE);

    out_test_header(0x0305, "ICMP_handler - HNDLR_REMOVE not registered");
    res = ICMP_handler (receive_echo, HNDLR_REMOVE);
    ok  = (res == FALSE) ? 1 : 0;
    out_result_error(ok, res);

    out_test_header(0x0306, "ICMP_handler - HNDLR_QUERY not registered");
    res = ICMP_handler(receive_echo, HNDLR_QUERY);
    ok  = (res == FALSE) ? 1 : 0;
    out_result_error(ok, res);
    
    //------------    
    // now do the real ICMP test
    out_test_header(0x0307, "ICMP_handler - HNDLR_SET real");
    res = ICMP_handler(receive_echo, HNDLR_SET);
    ok  = (res == TRUE) ? 1 : 0;
    out_result_error(ok, res);

    out_test_header(0x0308, "ICMP_handler - HNDLR_SET real, again");
    res = ICMP_handler(receive_echo, HNDLR_SET);
    ok  = (res == FALSE) ? 1 : 0;
    out_result_error(ok, res);

    out_test_header(0x0309, "ICMP_handler - HNDLR_QUERY of real");
    res = ICMP_handler(receive_echo, HNDLR_QUERY);
    ok  = (res == TRUE) ? 1 : 0;
    out_result_error(ok, res);
    
    //------------
    // test the ping
    received = 0;
    out_test_header(0x0310, "ICMP_send - 5 pings");
    
    int i;
    for(i=0; i<5; i++) {
        send_echo();
        sleepMs(200);
    }
    
    sleep(1);
    ok = (received == 5) ? 1 : 0;
    out_result_error(ok, received);
    
    //------------
    // now remove the handler and see if it is removed
    out_test_header(0x0311, "ICMP_handler - HNDLR_REMOVE existing");
    res = ICMP_handler(receive_echo, HNDLR_REMOVE);
    ok  = (res == TRUE) ? 1 : 0;
    out_result_error(ok, res);

    out_test_header(0x0312, "ICMP_handler - HNDLR_QUERY after REMOVE");
    res = ICMP_handler(receive_echo, HNDLR_QUERY);
    ok  = (res == FALSE) ? 1 : 0;
    out_result_error(ok, res);

    //------------
    out_test_header(0x0313, "ICMP_send - calling handler after REMOVE");
    received = 0;

    for(i=0; i<5; i++) {
        send_echo();
        sleepMs(200);
    }
    
    sleep(1);
    ok = (received == 0) ? 1 : 0;
    out_result_error(ok, received);
}

int16 cdecl receive_echo(IP_DGRAM  *datagram)
{
    uint16  *data;
    data = datagram->pkt_data;

    if (data[0] != (ICMP_ECHO_REPLY << 8) || data[2] != 0xaffeu) {
        return FALSE;
    }

   received++;

   ICMP_discard(datagram);
   return TRUE;
}

void send_echo(void)
{
           uint16  buffer[16];
   static  uint16  sequence = 0;

   buffer[0] = 0xaffeu;   buffer[1] = sequence++;
   * (long *) &buffer[2] = getTicks();

   buffer[4] = 0xa5a5u;   buffer[5] = 0x5a5au;
   buffer[6] = 0x0f0fu;   buffer[7] = 0xf0f0u;

   ICMP_send(SERVER_ADDR, ICMP_ECHO, 0, buffer, 32);
}
