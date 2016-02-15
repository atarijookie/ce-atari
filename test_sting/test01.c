//--------------------------------------------------
#include <mint/osbind.h> 
#include <stdio.h>

#include "version.h"
#include "global.h"
#include "transprt.h"
#include "stdlib.h"
#include "out.h"

//--------------------------------------------------
extern TPL       *tpl;

void test01(WORD testNo, char *testName, BYTE tcpNotUdp, DWORD *blockSizes, WORD blockSizesCount);
void test01TcpNotUdp(int testNoOffset, int tcpNotUdp);

extern BYTE *rBuf, *wBuf;
int sendAndReceive(WORD testNo, char *testName, BYTE tcpNotUdp, DWORD blockSize, int handle);

void generateWriteBuffer(void);

void doTest01(void)
{
    generateWriteBuffer();
    
    test01TcpNotUdp(0,  1);          // offset  0, TCP
    test01TcpNotUdp(10, 0);          // offset 10, UDP
}

void generateWriteBuffer(void)
{
    int i;
    for(i=0; i<127000; i++) {
        wBuf[i] = (BYTE) i;
    }
}

void test01TcpNotUdp(int testNoOffset, int tcpNotUdp)
{
    DWORD blocks01[1] = {32};
    test01(0x0101 + testNoOffset, "TCP echo     32 B", tcpNotUdp, &blocks01[0], 1);

    DWORD blocks02[3] = {250, 250, 250};
    test01(0x0102 + testNoOffset, "TCP echo    750 B", tcpNotUdp, &blocks02[0], 3);
    
    DWORD blocks03[3] = {511, 512, 513};
    test01(0x0103 + testNoOffset, "TCP echo   1536 B", tcpNotUdp, &blocks03[0], 3);

    DWORD blocks04[3] = {1000, 250, 1000};
    test01(0x0104 + testNoOffset, "TCP echo   2250 B", tcpNotUdp, &blocks04[0], 3);

    DWORD blocks05[4] = {10, 5000, 10, 5000};
    test01(0x0105 + testNoOffset, "TCP echo  10020 B", tcpNotUdp, &blocks05[0], 4);

    DWORD blocks06[3] = {32000, 64000, 127000};
    test01(0x0106 + testNoOffset, "TCP echo 223000 B", tcpNotUdp, &blocks06[0], 3);
}

void test01(WORD testNo, char *testName, BYTE tcpNotUdp, DWORD *blockSizes, WORD blockSizesCount)
{
    DWORD start = getTicks();

    //----------
    // open socket
    int handle;
    
    if(tcpNotUdp) {
        handle = TCP_open(SERVER_ADDR, SERVER_PORT_START, 0, 1000);
    } else {
        handle = UDP_open(SERVER_ADDR, SERVER_PORT_START);
    }
    
    if(handle < 0) {
        out_tr_eb(testNo, testName, "TCP/UDP open() failed", 0);
        return;
    }

    //----------
    // if TCP (not UDP), wait for connected state
    if(tcpNotUdp) {
        // wait until connected
        int res;

        while(1) {
            res = TCP_wait_state(handle, TESTABLISH, 1);
        
            if(res == E_NORMAL) {
                break;
            }

            DWORD now = getTicks();
            if((now - start) > 5*200) {
                out_tr_eb(testNo, testName, "TCP_wait_state() timeout", 0);
                goto test01close;
            }
        }
        
        if(res != E_NORMAL) {
            out_tr_ebw(testNo, testName, "TCP_wait_state() failed", 0, res);
            goto test01close;
        }
    }
    
    //---------------------
    int i, res;
    
    for(i=0; i<blockSizesCount; i++) {
        res = sendAndReceive(testNo, testName, tcpNotUdp, blockSizes[i], handle);
    
        if(!res) {                              // if single block-send-and-receive operation failed, quit and close
            goto test01close;
        }
    }
    
    //---------------------
    
    out_tr_eb(testNo, testName, " ", 1);        // success!
    
test01close:
    if(tcpNotUdp) {
        res = TCP_close(handle, 0, 0);          // close
    } else {
        res = UDP_close(handle);                // close
    }
    
    if(res != E_NORMAL) {
        out_tr_ebw(testNo, testName, "TCP/UDP close() failed", 0, res);
    }
}

int sendAndReceive(WORD testNo, char *testName, BYTE tcpNotUdp, DWORD blockSize, int handle)
{
    //----------
    // send
    int res;
    DWORD now, start;
    
    start = getTicks();
    
    while(1) {              // try to send
        if(tcpNotUdp) {
            res = TCP_send(handle, wBuf, blockSize);
        } else {
            res = UDP_send(handle, wBuf, blockSize);
        }

        if(res != E_OBUFFULL) {
            break;
        }
        
        now = getTicks();
        if((now - start) > (5 * 200)) {     // timeout? 
            out_tr_eb(testNo, testName, "TCP / UDP send() timeout", 0);
            return 0;
        }
    }
    
    if(res != E_NORMAL) { 
        out_tr_ebw(testNo, testName, "TCP / UDP send() failed", 0, res);
        return 0;
    }
    
    //----------
    // wait
    start = getTicks();
    
    while(1) {
        res = CNbyte_count(handle);
        
        if(res >= blockSize) {
            break;
        }

        now = getTicks();
        if((now - start) > (5 * 200)) {     // timeout? 
            out_tr_eb(testNo, testName, "CNbyte_count() timeout", 0);
            return 0;
        }
    }
    
    if(res < blockSize) {                   // not enough data?
        out_tr_ebw(testNo, testName, "CNbyte_count() failed", 0, res);
        return 0;
    }
    
    //----------
    // receive
    memset(rBuf, 0, blockSize);
    
    res = CNget_block(handle, rBuf, blockSize);
    
    if(res != blockSize) { 
        out_tr_ebw(testNo, testName, "CNget_block() failed", 0, res);
        return 0;
    }
    
    //----------
    // data are valid? 
    res = memcmp(rBuf, wBuf, blockSize);
    
    if(res != 0) {
        out_tr_eb(testNo, testName, "Received data mismatch", 0);
        return 0;
    }

    //-------
    // if came here, everything is OK
    return 1;
}
