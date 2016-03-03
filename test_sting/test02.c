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
int  tryReceive0206(int handle, int line);

void doTest0208    (BYTE tcpNotUdp);

void doTest0210    (WORD testNumber);
int  sendAndReceive(BYTE tcpNotUdp, DWORD blockSize, int handle, BYTE getBlockNotNdb);
int  getCab        (int handle, CAB *cab);
int  tellServerToConnectToOutPassiveConnection(int handlePassive);

int verifyCab(  int handle, 
                DWORD lhost, char op1, WORD lport, char op2, 
                DWORD rhost, char op3, WORD rport, char op4 );
                
void doTest0230    (void);
void doTest0240    (void);

int testCanaries(int handle, int blockSize, int offset);

#define LOCAL_PASSIVE_PORT 10000

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
    
    //----------------------------
    /*
    TCP_ACTIVE : remote host and port must be specified.
                 Local host is local IP.
    
    TCP_PASSIVE: remote host and port must be 0, otherwise socket won't listen.
                 Local host is 0.
    */

    // TCP_open() and UDP_open - addressing modes tests
    int i;
    for(i = 0x0210; i <= 0x0222; i++) {
        doTest0210(i);
    }
    //----------------------------
    
    // TCP waiting for listening socket to connect
    doTest0230();
    
    // test canaries on CNget_block    
    doTest0240();
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
                out_result(1);
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
    BYTE getBlockNotNdb = test0202not0204 ? 0 : 1;  // test 0202 goes through CNget_NDB, test 0204 uses CNget_block
    
    for(j=0; j<10; j++) {
        res = sendAndReceive(tcpNotUdp, 1000, handle,  getBlockNotNdb);
        
        if(!res) {                                  // if 0 (not good), fail
            goto test0202end;
        }
    }
    
    out_result(1);                                  // everything OK
    
test0202end:
    // close connection
    if(tcpNotUdp) {
        TCP_close(handle, 0, 0);
    } else {
        UDP_close(handle);
    }
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

    //----------
    // try when there is no data to get
    res = CNgets(handle, rBuf, 9, '\n');        // try to receive to small buffer
    
    if(res != E_NODATA) {                       // if not this error, fail
        out_result_error_string(0, res, "didn't get E_NODATA on empty socket");
        goto test0206end;
    }
    //----------
    
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

    //-----------
    // try too small buffer
    DWORD end = getTicks() + 600;
    while(1) {                                  // wait until there is some data
        if(getTicks() >= end) {
            out_result_string(0, "timeout on wait for data");
            goto test0206end;
        }
        
        res = CNbyte_count(handle);
        if(res >= 10) {
            break;
        }
    }
    
    res = CNgets(handle, rBuf, 9, '\n');        // try to receive to small buffer
    
    if(res != E_BIGBUF) {                       // if not this error, fail
        out_result_error_string(0, res, "didn't get E_BIGBUF on small buffer");
        goto test0206end;
    }
    //-----------
    
    int j;
    for(j=0; j<GETS_LINES; j++) {
        res = tryReceive0206(handle, j);

        if(res) {       // if not 0 (not good)
            goto test0206end;
        }
    }
    
    out_result(1);                      // everything OK
    
test0206end:
    // close connection
    if(tcpNotUdp) {
        TCP_close(handle, 0, 0);
    } else {
        UDP_close(handle);
    }
}

int tryReceive0206(int handle, int line)
{
    int   timeoutTics = 3 * 200;
    DWORD timeout     = getTicks() + timeoutTics;

    memset(rBuf, 0, 200);                   // clear the buffer
    
    int res;
    while(1) {
        res = CNgets(handle, rBuf, 200, '\n');
    
        if(res > 0) {                       // if good, quit
            break;
        }

        if(getTicks() >= timeout) {         // if timeout, fail
            out_result_error_string(0, line, "timeout on CNgets");
            return res;
        }
    }

    int lenFromString = getIntFromStr((char *) rBuf, 4);
    int lenFromStrlen = strlen       ((char *) rBuf);
    
    // If the lenght from sting doesn't match the length from strlen(), fail
    // Valid difference between those lengths is 3, because strlen() on server also count '\n', and strlen() on client also counts integer on start, but there's no '\n'.
    if((lenFromString + 3) != lenFromStrlen) {      
        out_result_error_string(0, line, "too short received string");
        out_result_string_dw_w(0, (char *) rBuf, lenFromString, lenFromStrlen);
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
    DWORD start = getTicks();
    if(tcpNotUdp) {
        // wait for connection to close
        res = TCP_wait_state(handle, TCLOSE_WAIT, 3);
        
        if(res != E_NORMAL) {                       // if didn't get to TCLOSE_WAIT state, fail
            out_result_error_string(0, res, "waiting for close failed");
            goto test0208end;
        }
    }
    DWORD end   = getTicks();
    DWORD ms    = (end - start) * 5;                // calculate how long it took to find out, that the socket has closed on the other side
    
    out_result_error(1, ms);                        // everything OK
    
test0208end:
    // close connection
    if(tcpNotUdp) {
        TCP_close(handle, 0, 0);
    } else {
        UDP_close(handle);
    }    
}

void doTest0210(WORD testNumber)
{
    int handle, res;
    
    CAB cab;
    cab.lhost = 0;                      // local  host
    cab.lport = LOCAL_PASSIVE_PORT;     // local  port
    cab.rhost = SERVER_ADDR;            // remote host
    cab.rport = SERVER_PORT_START;      // remote port
    
    int tcpNotUdp           = 1;        // by default - TCP
    int activeNotPassive    = 1;        // by default - active connection
    
    CAB cab2;
    memset(&cab2, 0, sizeof(CAB));
    
    static uint32 myIp = 0;
    
    #define TCP_FIRST_PORT      1024
    #define UDP_FIRST_PORT      1024
    
    switch(testNumber) {
        /////////////////
        // remote host is normal IP
        case 0x0210:                // TCP, rem_host = IP addr, rem_port = port number (not TCP_ACTIVE, not TCP_PASSIVE) --> ACTIVE
            out_test_header(testNumber, "TCP_open - IP specified, port specified");
            handle = TCP_open(SERVER_ADDR, SERVER_PORT_START, 0, 2000);
            
            res = getCab(handle, &cab2);
            myIp = cab2.lhost;
            
            if(!verifyCab(handle, myIp, '!', 0, ' ', SERVER_ADDR, '!', SERVER_PORT_START, '!')) {
                goto test0210end;
            }
            
            break;
            
        /////////////////
        // remote host is CAB *, TCP_ACTIVE
        case 0x0211:                // TCP, rem_host = CAB *,   rem_port = TCP_ACTIVE, rport & rhost specify remote address, cab->lport is local port, cab->lhost may be 0 (in that case it's filled with local ip)
            out_test_header(testNumber, "TCP_open - TCP_ACTIVE, got remote & local");
            handle = TCP_open(&cab, TCP_ACTIVE, 0, 2000);

            if(!verifyCab(handle, myIp, '!', cab.lport, '!', cab.rhost, '!', cab.rport, '!')) {
                goto test0210end;
            }
            
            break;

        case 0x0212:                // TCP, rem_host = CAB *,   rem_port = TCP_ACTIVE, rport & rhost specify remote address, cab->lport is 0, next free port will be used (use CNgetinfo() to find out used port #), cab->lhost may be 0 (in that case it's filled with local ip)
            out_test_header(testNumber, "TCP_open - TCP_ACTIVE, got only remote");
            cab.lhost = 0;
            cab.lport = 0;
            handle = TCP_open(&cab, TCP_ACTIVE, 0, 2000);

            if(!verifyCab(handle, myIp, '!', TCP_FIRST_PORT, '<', cab.rhost, '!', cab.rport, '!')) {
                goto test0210end;
            }

            break;
        
        case 0x0213:                // TCP, rem_host = 0,       rem_port = TCP_ACTIVE --> becomes PASSIVE connection, local port is next free port
            out_test_header(testNumber, "TCP_open - TCP_ACTIVE, rem_host is NULL");
            handle = TCP_open(NULL,        TCP_ACTIVE, 0, 2000);
            
            activeNotPassive = 0;   // passive connection, because nothing specified as rem_host
            
            // when listening, local host is 0
            if(!verifyCab(handle, 0, '!', TCP_FIRST_PORT, '<', 0, '!', 0, '!')) {
                goto test0210end;
            }
            
            break;

        /////////////////
        // remote host is CAB *, TCP_PASSIVE
        case 0x0214:                // TCP, rem_host = CAB *,   rem_port = TCP_PASSIVE, cab->lport is local port
            activeNotPassive = 0;   // passive connection
            out_test_header(testNumber, "TCP_open - TCP_PASSIVE, got local port");
            
            // remote host and port must be 0, otherwise socket won't listen
            cab.rhost = 0;          
            cab.rport = 0;
            
            handle = TCP_open(&cab, TCP_PASSIVE, 0, 2000);
            
            // when listening, local host is 0
            if(!verifyCab(handle, 0, '!', cab.lport, '!', 0, ' ', 0, ' ')) {
                goto test0210end;
            }

            break;

        case 0x0215:                // TCP, rem_host = CAB *,   rem_port = TCP_PASSIVE, cab->lport is 0, next free port will be used (use CNgetinfo() to find out used port #)
            activeNotPassive = 0;   // passive connection
            out_test_header(testNumber, "TCP_open - TCP_PASSIVE, local port is 0");
            
            // remote host and port must be 0, otherwise socket won't listen
            cab.rhost = 0;          
            cab.rport = 0;

            cab.lport = 0;
            handle = TCP_open(&cab, TCP_PASSIVE, 0, 2000);
            
            // when listening, local host is 0
            if(!verifyCab(handle, 0, '!', TCP_FIRST_PORT, '<', 0, ' ', 0, ' ')) {
                goto test0210end;
            }
            
            break;
        
        case 0x0216:                // TCP, rem_host = 0,       rem_port = TCP_PASSIVE --> either will crash, or will open PASSIVE connection on next free port
            activeNotPassive = 0;   // passive connection
            out_test_header(testNumber, "TCP_open - TCP_PASSIVE, rem_host is NULL");
            
            // remote host and port must be 0, otherwise socket won't listen
            cab.rhost = 0;          
            cab.rport = 0;
            
            handle = TCP_open(NULL, TCP_PASSIVE, 0, 2000);
            
            // when listening, local host is 0
            if(!verifyCab(handle, 0, '!', TCP_FIRST_PORT, '<', 0, ' ', 0, ' ')) {
                goto test0210end;
            }
            
            break;
            
        ////////////////////////////////////////////////////////
        case 0x0220:                // UDP, rem_host is IP,    rem_port is port # (not 0 / UDP_EXTEND) -> remote is to IP:port, local port is next free port
            tcpNotUdp = 0;          // it's UDP
            out_test_header(testNumber, "UDP_open - IP specified, port specified");
            handle = UDP_open(SERVER_ADDR, SERVER_PORT_START + 4);

            if(!verifyCab(handle, myIp, '!', UDP_FIRST_PORT, '<', SERVER_ADDR, '!', SERVER_PORT_START + 4, '!')) {
                goto test0210end;
            }
            
            break;

        case 0x0221:                // UDP, rem_host is CAB *, rem_port is 0 / UDP_EXTEND, cab->lport is port # (not 0) -> open UDP on this local port
            tcpNotUdp = 0;          // it's UDP
            out_test_header(testNumber, "UDP_open - UDP_EXTEND, got local port");
            
            cab.rport += 4;
            
            handle = UDP_open(&cab, UDP_EXTEND);
            
            if(!verifyCab(handle, myIp, '!', cab.lport, '!', cab.rhost, '!', cab.rport, '!')) {
                goto test0210end;
            }
            
            break;
        
        case 0x0222:                // UDP, rem_host is CAB *, rem_port is 0 / UDP_EXTEND, cab->lport is 0              -> open UDP on first free local port
            tcpNotUdp = 0;          // it's UDP
            out_test_header(testNumber, "UDP_open - UDP_EXTEND, local port is 0");
            cab.lport  = 0;
            cab.rport += 4;

            handle = UDP_open(&cab, UDP_EXTEND);
            
            if(!verifyCab(handle, myIp, '!', UDP_FIRST_PORT, '<', cab.rhost, '!', cab.rport, '!')) {
                goto test0210end;
            }
            
            break;
            
        ////////////////////////////////////////////////////////
        // bad test case number? quit
        default:        return;
    }
    
    if(handle < 0) {
        out_result_error_string(0, handle, "open failed");
        return;
    }
    
    //---------------------
    // for passive connection, send local port to server, wait for connection
    if(!activeNotPassive) {
        res = tellServerToConnectToOutPassiveConnection(handle);

        if(!res) {              // failed to tell server? fail
            goto test0210end;
        }
        
        //---------
        // wait until server connects back
        res = TCP_wait_state(handle, TESTABLISH, 5);
        
        if(res != E_NORMAL) {
            out_result_error_string(0, res, "server didn't connect back");
            goto test0210end;
        }
    }
    
    //---------------------
    // send & receive data
    res = sendAndReceive(tcpNotUdp, 1000, handle, 1);
    
    if(!res) {                              // if single block-send-and-receive operation failed, quit and close
        goto test0210end;
    }
    
    //---------------------
    // success flows through here
    out_result(1);                          // success!    
    
test0210end:
    if(handle <= 0) {                       // no handle? just quit
        return;
    }

    if(tcpNotUdp) {
        TCP_close(handle, 0, 0);
    } else {
        UDP_close(handle);
    }
}

int getCab(int handle, CAB *cab)
{
    if(handle < 0) {
        return 0;
    }
    
    CIB *cib = CNgetinfo(handle);
    
    if(cib == 0) {
        out_result_string(0, "verify port - CNgetinfo() failed");
        return 0;
    }

    *cab = cib->address;
    return 1;
}

int compare(DWORD a, DWORD b, char op)
{
    switch(op) {
        case '!':   return (a != b);
        case '=':   return (a == b);
        case '<':   return (a < b);
        case '>':   return (a > b);
        case ' ':   return 0;               // return false == ignore this comparing
    }
    
    return 1;                               // unknown op? fail
}

int verifyCab(  int handle, 
                DWORD lhost, char op1, WORD lport, char op2, 
                DWORD rhost, char op3, WORD rport, char op4 )
{
    CAB cab2;
    int res = getCab(handle, &cab2);
    
    if(res) {
        if( compare(cab2.lhost, lhost, op1) || 
            compare(cab2.lport, lport, op2) ) {
            out_result_string_dw_w(0, "CNgetinfo() local info bad", cab2.lhost, cab2.lport);
            return 0;
        }

        if( compare(cab2.rhost, rhost, op3) || 
            compare(cab2.rport, rport, op4) ) {
            out_result_string_dw_w(0, "CNgetinfo() remote info bad", cab2.rhost, cab2.rport);
            return 0;
        }
    }

    return 1;
}

int tellServerToConnectToOutPassiveConnection(int handlePassive)
{
    int res;

    //---------
    // find out local port
    CIB *cib = CNgetinfo(handlePassive);
    
    if(!cib) {
        out_result_string(0, "CNgetinfo() failed");
        return 0;
    }
    
    //---------
    // connect to server
    int handle2;
    
    handle2 = TCP_open(SERVER_ADDR, SERVER_PORT_START + 20, 0, 1000);
    
    if(handle2 < 0) {
        out_result_error_string(0, handle2, "tell port TCP_open() failed");
        return 0;
    }
    
    res = TCP_wait_state(handle2, TESTABLISH, 3);
    
    if(res != E_NORMAL) {
        out_result_error_string(0, res, "tell port TCP_wait_state() failed");
        TCP_close(handle2, 0, 0);
        return 0;
    }

    //---------
    // send local port to server
    CAB         cab;
    res         = getCab(handle2, &cab);
    DWORD myIp  = cab.lhost;

    BYTE tmp[6];
    
    tmp[0] = (myIp   >> 24) & 0xff;
    tmp[1] = (myIp   >> 16) & 0xff;
    tmp[2] = (myIp   >>  8) & 0xff;
    tmp[3] = (myIp        ) & 0xff;

    tmp[4] = (cib->address.lport >> 8) & 0xff;
    tmp[5] = (cib->address.lport     ) & 0xff;
    
    res = TCP_send(handle2, tmp, 6);
    
    if(res != E_NORMAL) {
        out_result_error_string(0, res, "tell port TCP_send() failed");
        TCP_close(handle2, 0, 0);
        return 0;
    }
    
    TCP_close(handle2, 0, 0);
    return 1;
}

int createPassiveConnection(int localPort)
{
    int handle;
    
    CAB cab;
    cab.lhost = 0;                      // local  host
    cab.lport = localPort;              // local  port
    
    // remote host and port must be 0, otherwise socket won't listen
    cab.rhost = 0;                      // remote host
    cab.rport = 0;                      // remote port
    
    handle = TCP_open(&cab, TCP_PASSIVE, 0, 2000);

    return handle;
}

void doTest0230(void)
{
    int handle, res, ok;

    //--------------------------
    // wait using TCP_wait_state
    out_test_header(0x0230, "PASSIVE waiting using TCP_wait_state");
    handle  = createPassiveConnection(LOCAL_PASSIVE_PORT);
    res     = tellServerToConnectToOutPassiveConnection(handle);
    if(res) {
        res = TCP_wait_state(handle, TESTABLISH, 3);    // wait 3 seconds for connection
        ok = (res == E_NORMAL) ? 1 : 0;
    
        out_result_error(ok, res);
    }
    TCP_close(handle, 0, 0);
    
    //--------------------------    
    // wait using CNbyte_count
    out_test_header(0x0231, "PASSIVE waiting using CNbyte_count");
    handle  = createPassiveConnection(LOCAL_PASSIVE_PORT);
    res     = tellServerToConnectToOutPassiveConnection(handle);
    if(res) {
        DWORD end = getTicks() + 600;           // wait 3 seconds for connection

        ok = 0;
        while(1) {
            if(getTicks() >= end) {             // if timeout, fail
                ok = 0;
                break;
            }
        
            res = CNbyte_count(handle);         // try to get byte count, which will return E_LISTEN if still listening
            
            if(res >= 0 || res == E_NODATA) {   // can finally get amount waiting, or E_NODATA? it's better than E_LISTEN, good
                ok = 1;
                break;
            }
        }
        out_result_error(ok, res);
    }
    TCP_close(handle, 0, 0);
    
    //--------------------------    
    // wait by check out the info that is pointed to by the pointer returned by CNgetinfo
    out_test_header(0x0232, "PASSIVE waiting using CNgetinfo");
    handle  = createPassiveConnection(LOCAL_PASSIVE_PORT);
    res     = tellServerToConnectToOutPassiveConnection(handle);
    
    CIB *cib = CNgetinfo(handle);
    
    if(cib != NULL) {
        DWORD end = getTicks() + 600;           // wait 3 seconds for connection

        ok = 0;
        while(1) {
            if(getTicks() >= end) {             // if timeout, fail
                ok = 0;
                break;
            }

            if(cib->address.rhost != 0) {       // did remote host change from 0 to something? We're connected, good
                ok = 1;
                break;
            }
        }
        out_result_error(ok, res);
    } else {
        out_result_string(0, "CNgetinfo failed");
    }
    TCP_close(handle, 0, 0);
}

void doTest0240(void)
{
    int handle;

    // open connection
    handle = TCP_open(SERVER_ADDR, SERVER_PORT_START, 0, 2000);

    if(handle < 0) {
        out_test_header(0x0240, "CNget_block canaries");
        out_result_string(0, "TCP_open failed");
        return;
    }
        
    // test canaries on CNget_block
    WORD testNo = 0x0240;
    
    out_test_header(testNo++, "CNget_block canaries  511 0");
    testCanaries(handle, 511, 0);
    
    out_test_header(testNo++, "CNget_block canaries  511 1");
    testCanaries(handle, 511, 1);
    
    out_test_header(testNo++, "CNget_block canaries  512 0");
    testCanaries(handle, 512, 0);
    
    out_test_header(testNo++, "CNget_block canaries  512 1");
    testCanaries(handle, 512, 1);

    out_test_header(testNo++, "CNget_block canaries  513 0");
    testCanaries(handle, 513, 0);
    
    out_test_header(testNo++, "CNget_block canaries  513 1");
    testCanaries(handle, 513, 1);
    
    out_test_header(testNo++, "CNget_block canaries 1023 0");
    testCanaries(handle, 1023, 0);
    
    out_test_header(testNo++, "CNget_block canaries 1023 1");
    testCanaries(handle, 1023, 1);
    
    out_test_header(testNo++, "CNget_block canaries 1024 0");
    testCanaries(handle, 1024, 0);
    
    out_test_header(testNo++, "CNget_block canaries 1024 1");
    testCanaries(handle, 1024, 1);
    
    out_test_header(testNo++, "CNget_block canaries 1025 0");
    testCanaries(handle, 1025, 0);
    
    out_test_header(testNo++, "CNget_block canaries 1025 1");
    testCanaries(handle, 1025, 1);
    
    out_test_header(testNo++, "CNget_block canaries 1535 0");
    testCanaries(handle, 1535, 0);
    
    out_test_header(testNo++, "CNget_block canaries 1535 1");
    testCanaries(handle, 1535, 1);
    
    out_test_header(testNo++, "CNget_block canaries 1536 0");
    testCanaries(handle, 1536, 0);
    
    out_test_header(testNo++, "CNget_block canaries 1536 1");
    testCanaries(handle, 1536, 1);
    
    out_test_header(testNo++, "CNget_block canaries 1537 0");
    testCanaries(handle, 1537, 0);
    
    out_test_header(testNo++, "CNget_block canaries 1537 1");
    testCanaries(handle, 1537, 1);
    
    TCP_close(handle, 0, 0);
}

int testCanaries(int handle, int blockSize, int offset)
{
    int res;
    
    //------------
    // send data
    res = TCP_send(handle, wBuf, blockSize);
    
    if(res != E_NORMAL) {
        out_result_error_string(0, res, "send failed");
        return 0;
    }
    
    //------------
    // wait for enough data to arrive
    DWORD end = getTicks() + 600;
    while(1) {
        if(getTicks() >= end) {                     // if timeout, fail
            out_result_error_string(0, res, "waiting for data failed");
            return 0;
        }
        
        res = CNbyte_count(handle);
        
        if(res >= blockSize) {                      // if enough data arrived, stop waiting
            break;
        }
    }
    
    //------------
    // prepare read buffer and receive data
    memcpy(rBuf                 + offset, "CANARIES", 8);    // put canaries before buffer
    memset(rBuf + 8             + offset, 0, blockSize);     // clear buffer
    memcpy(rBuf + 8 + blockSize + offset, "HMNGBRDS", 8);    // put canaries after buffer
    
    res = CNget_block(handle, rBuf + 8 + offset, blockSize); // read data

    if(res < blockSize) {
        out_result_error_string(0, res, "CNget_block not enough data");
        return 0;
    }
    
    // verify received data
    int mismatch = memcmp(wBuf, rBuf + 8 + offset, blockSize);
    
    if(mismatch) {                                              // if mismatch
        out_result_error_string(0, res, "received data mismatch");
        return 0;
    }
    
    // verify canaries before received buffer
    mismatch = memcmp(rBuf + offset, "CANARIES", 8);
    
    if(mismatch) {                                              // if mismatch
        out_result_error_string(0, res, "canaries before destroyed");
        return 0;
    }
    
    // verify canaries after received buffer
    mismatch = memcmp(rBuf + 8 + blockSize + offset, "HMNGBRDS", 8);
    
    if(mismatch) {                                              // if mismatch
        out_result_error_string(0, res, "canaries after destroyed");
        return 0;
    }
    
    // everything OK
    out_result(1);
    return 1;
}
