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

void doTest0200(BYTE tcpNotUdp);
void doTest0202(BYTE tcpNotUdp, BYTE test0202not0204);

int trySend0202   (int handle, int tcpNotUdp, int size, int timeoutSecs);
int tryReceive0202(int handle, int size, int timeoutSecs);
int tryReceive0204(int handle, int size, int timeoutSecs);

void doTest0206    (BYTE tcpNotUdp);
int  tryReceive0206(int handle);

void doTest0208    (BYTE tcpNotUdp);

void doTest02(void)
{
    // CNbyte_count + CNget_char
    doTest0200(1);      // TCP
    doTest0200(0);      // UDP

    // CNget_NDB
    doTest0202(1, 1);   // TCP
    doTest0202(0, 1);   // UDP
    
    // CNget_block
    doTest0202(1, 0);   // TCP
    doTest0202(0, 0);   // UDP
    
    // CNgets
    doTest0206(1);      // TCP
    doTest0206(0);      // UDP
    
    // CNgetinfo
    doTest0208(1);      // TCP
    doTest0208(0);      // UDP
}

void doTest0200(BYTE tcpNotUdp)
{
    int handle, res;
    
    if(tcpNotUdp) {
        out_test_header(0x0200, "TCP CNbyte_count + CNget_char");
        handle = TCP_open(SERVER_ADDR, SERVER_PORT_START, 0, 2000);
    } else {
        out_test_header(0x0201, "UDP CNbyte_count + CNget_char");
        handle = UDP_open(SERVER_ADDR, SERVER_PORT_START + 4);
    }
    
    if(handle < 0) {
        out_result_error_string(0, handle, "open failed");
        return;
    }
    
    if(tcpNotUdp) {
        res = TCP_send(handle, wBuf, 1000);
    } else {
        res = UDP_send(handle, wBuf, 1000);
    }
    
    if(res != E_NORMAL) {
        out_result_error_string(0, res, "send failed");
        goto test0200end;
    }
    
    DWORD now, start;
    start = getTicks();
    
    int toReceive = 1000;
    int idx = 0;
    int mismatch = 0;
    
    while(1) {
        // nothing more to receive? quit
        if(toReceive <= 0) {

            if(!mismatch) {         // no mismatch?   good
                out_result_error(1, 0);
            } else {                // data mismatch? fail
                out_result_string(0, "received enough data, but mismatch");
            }
             
            goto test0200end;
        }
    
        //----------
        // something to receive?
        int gotBytes = CNbyte_count(handle);    // how much data we have?
        if(gotBytes > 0) {
            int i, a;
            
            for(i=0; i<gotBytes; i++) {         // try to receive it all
                a = CNget_char(handle);
                
                if(a != wBuf[idx]) {            // check if the data is matching the sent data
                    mismatch = 1;
                }
                idx++;
            }
            
            toReceive -= gotBytes;              // now we need to receive less
        }    
        
        //----------
        // check for timeout
        now = getTicks();
        
        if((now - start) > 600) {
            out_result_string(0, "timeout while receiving");
            goto test0200end;
        }
    }
    
test0200end:
    if(tcpNotUdp) {
        TCP_close(handle, 0, 0);
    } else {
        UDP_close(handle);
    }
}

void doTest0202(BYTE tcpNotUdp, BYTE test0202not0204)
{
    int handle, res;
    
    // open connection
    if(tcpNotUdp) {
        if(test0202not0204) {
            out_test_header(0x0202, "TCP CNget_NDB");
        } else {
            out_test_header(0x0204, "TCP CNget_block");
        }
        
        handle = TCP_open(SERVER_ADDR, SERVER_PORT_START, 0, 2000);
    } else {
        if(test0202not0204) {
            out_test_header(0x0203, "UDP CNget_NDB");
        } else {
            out_test_header(0x0205, "UDP CNget_block");
        }
        
        handle = UDP_open(SERVER_ADDR, SERVER_PORT_START + 4);
    }
    
    if(handle < 0) {
        out_result_error_string(0, handle, "open failed");
        return;
    }
    
    int j;
    for(j=0; j<10; j++) {
        // send data
        res = trySend0202(handle, tcpNotUdp, 1000, 3);

        if(!res) {
            out_result_string(0, "timeout on send");
            goto test0202end;
        }
    
        // get data
        if(test0202not0204) {
            res = tryReceive0202(handle, 1000, 3);
        } else {
            res = tryReceive0204(handle, 1000, 3);
        }

        if(res) {       // if not 0 (not good)
            goto test0202end;
        }
    }
    
    out_result_error(1, 0);                 // everything OK
    
test0202end:
    // close connection
    if(tcpNotUdp) {
        TCP_close(handle, 0, 0);
    } else {
        UDP_close(handle);
    }
}

int trySend0202(int handle, int tcpNotUdp, int size, int timeoutSecs)
{
    int   res;
    int   timeoutTics = timeoutSecs * 200;
    DWORD timeout     = getTicks() + timeoutTics;
    
    while(1) {
        if(tcpNotUdp) {
            res = TCP_send(handle, wBuf, size);
        } else {
            res = UDP_send(handle, wBuf, size);
        }

        if(res == E_NORMAL) {                       // if good, success
            return 1;
        }
        
        if(getTicks() >= timeout) {                 // if timeout, fail
            return 0;
        }
    }
}

int tryReceive0202(int handle, int size, int timeoutSecs)
{
    int   timeoutTics = timeoutSecs * 200;
    DWORD timeout     = getTicks() + timeoutTics;
    
    NDB *ndb;
    
    while(1) {
        ndb = CNget_NDB(handle);

        if(getTicks() >= timeout) {         // if timeout, fail
            out_result_string(0, "timeout on CNget_NDB");
            return -1;
        }
        
        if(ndb) {                           // some received? process it
            break;
        }
    }
        
    int res = 0;                            // good (for now)
   
    if(ndb->len != size) {                  // block size mismatch, fail
        res = -2;
        out_result_string(0, "CNget_NDB size mismatch");
    }
    
    if(res == 0) {                          // only if size OK
        int i;
        for(i=0; i<size; i++) {
            if(ndb->ndata[i] != wBuf[i]) {  // received data mismatch? fail
                res = -3;
                out_result_error_string(0, 0, "CNget_NDB data mismatch");
                break;
            }
        }
    }
    
    KRfree (ndb->ptr);                      // free the ram
    KRfree (ndb);

    return res;                             // return error code
}

int tryReceive0204(int handle, int size, int timeoutSecs)
{
    int   timeoutTics = timeoutSecs * 200;
    DWORD timeout     = getTicks() + timeoutTics;
    
    int res;
    
    while(1) {
        res = CNget_block(handle, rBuf, size);
    
        if(getTicks() >= timeout) {         // if timeout, fail
            out_result_string(0, "timeout on CNget_block");
            return -1;
        }

        if(res != E_NODATA) {
            break;
        }
    }
        
    if(res != size) { 
        out_result_error_string(0, res, "CNget_block failed");
        return res;
    }        

    res = memcmp(wBuf, rBuf, size);
    
    if(res != 0) {
        out_result_string(0, "data mismatch");
        return -2;
    }
    
    return 0;           // good
}

void doTest0206(BYTE tcpNotUdp)
{
    int handle, res;
    
    // open connection
    if(tcpNotUdp) {
        out_test_header(0x0206, "TCP CNgets");
        handle = TCP_open(SERVER_ADDR, SERVER_PORT_START + 2, 0, 2000);
    } else {
        out_test_header(0x0207, "UDP CNgets");
        handle = UDP_open(SERVER_ADDR, SERVER_PORT_START + 2 + 4);
    }
    
    if(handle < 0) {
        out_result_error_string(0, handle, "open failed");
        return;
    }
    
    #define GETS_LINES  32
    
    char txBuf[2];
    txBuf[0] = 0;
    txBuf[1] = GETS_LINES;
        
    if(tcpNotUdp) {
        res = TCP_send(handle, txBuf, 2);
    } else {
        res = UDP_send(handle, txBuf, 2);
    }

    if(res != E_NORMAL) {
        out_result_string(0, "timeout on send");
        goto test0206end;
    }
    
    int j;
    for(j=0; j<GETS_LINES; j++) {
        res = tryReceive0206(handle);

        if(res) {       // if not 0 (not good)
            goto test0206end;
        }
    }
    
    out_result_error(1, 0);                 // everything OK
    
test0206end:
    // close connection
    if(tcpNotUdp) {
        TCP_close(handle, 0, 0);
    } else {
        UDP_close(handle);
    }
}

int tryReceive0206(int handle)
{
    int   timeoutTics = 3 * 200;
    DWORD timeout     = getTicks() + timeoutTics;

    memset(rBuf, 0, 200);                   // clear the buffer
    
    int res;
    while(1) {
        res = CNgets(handle, rBuf, 200, '\n');
    
        if(res == E_NORMAL) {               // if good, quit
            break;
        }

        if(getTicks() >= timeout) {         // if timeout, fail
            out_result_string(0, "timeout on CNgets");
            return res;
        }
    }

    int lenFromString = getIntFromStr((char *) rBuf, 4);
    int lenFromStrlen = strlen       ((char *) rBuf);
    
    if((lenFromString + 4) != lenFromStrlen) {
        out_result_string(0, "too short received string");
        return -2;
    }
    
    return 0;           // good
}

void doTest0208(BYTE tcpNotUdp)
{
   int handle, res;
   int remotePort;
    
    // open connection
    if(tcpNotUdp) {
        out_test_header(0x0208, "TCP CNgetinfo");
        remotePort = SERVER_PORT_START + 3;
        handle = TCP_open(SERVER_ADDR, remotePort, 0, 2000);
    } else {
        out_test_header(0x0209, "UDP CNgetinfo");
        remotePort = SERVER_PORT_START + 3 + 4;
        handle = UDP_open(SERVER_ADDR, remotePort);
    }
    
    if(handle < 0) {
        out_result_error_string(0, handle, "open failed");
        return;
    }
    
    CIB *cib = CNgetinfo(handle);
    
    if(!cib) {
        out_result_string(0, "CNgetinfo failed");
        goto test0208end;
    }
    
    //---------------------------
    // verify protocol
    if(tcpNotUdp) {
        if(cib->protocol != P_TCP) {
            out_result_string(0, "protocol not TCP");
            goto test0208end;
        }
    } else {
        if(cib->protocol != P_UDP) {
            out_result_string(0, "protocol not UDP");
            goto test0208end;
        }
    }
    
    //---------------------------
    // verify remote port
    if(cib->address.rport != remotePort) {
        out_result_string(0, "remote port bad");
        goto test0208end;
    }
    
    // verify remote address
    if(cib->address.rhost != SERVER_ADDR) {
        out_result_string(0, "remote address bad");
        goto test0208end;
    }

    // verify status
    if(cib->status != 0) {
        out_result_error_string(0, cib->status, "status not 0");
        goto test0208end;
    }

    //---------------------------
    // send closing time
    #define CLOSING_TIME_MS     1000
    
    char txBuf[2];
    txBuf[0] = (BYTE) (CLOSING_TIME_MS >> 8);       // upper byte
    txBuf[1] = (BYTE) (CLOSING_TIME_MS     );       // lower byte
        
    if(tcpNotUdp) {
        res = TCP_send(handle, txBuf, 2);
    } else {
        res = UDP_send(handle, txBuf, 2);
    }
    
    if(res != E_NORMAL) {
        out_result_error_string(0, res, "send failed");
        goto test0208end;
    }
    
    //---------------------------
    // if it's TCP, wait for closing and check status
    if(tcpNotUdp) {
        // wait for connection to close
        DWORD endTime = getTicks() + (CLOSING_TIME_MS / 5) + 600;

        WORD statusPrev = 0;
        
        while(1) {
            if(getTicks() >= endTime) {
                out_result_error_string(0, res, "waiting for close failed");
                goto test0208end;
            }
            
            if(statusPrev != cib->status) {     // if CIB->status changed
                out_result_error_string(0, cib->status, "cib->status");
                statusPrev = cib->status;
            }
        }
    }
    
    out_result_error(1, 0);                 // everything OK
    
test0208end:
    // close connection
    if(tcpNotUdp) {
        TCP_close(handle, 0, 0);
    } else {
        UDP_close(handle);
    }    
}
