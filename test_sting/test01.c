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

void test01(WORD testNo, char *testName, BYTE tcpNotUdp, DWORD *blockSizes, WORD blockSizesCount);
void test01TcpNotUdp(int testNoOffset, int tcpNotUdp);

extern BYTE *rBuf, *wBuf;
int sendAndReceive(BYTE tcpNotUdp, DWORD blockSize, int handle, BYTE getBlockNotNdb);

void generateWriteBuffer(void);

void test0130(void);

void doTest01(void)
{
    generateWriteBuffer();
    
    test01TcpNotUdp(0,    1);           // offset    0, TCP
    test01TcpNotUdp(0x10, 0);           // offset 0x10, UDP
    
    test0130();                         // test UDP cutting of received data to separate datagrams
}

void generateWriteBuffer(void)
{
    int i;
    for(i=0; i<127000; i++) {
        wBuf[i] = (BYTE) i;
    }
}

void generateTestName(int tcpNotUdp, char *partialName, char *fullName)
{
    if(tcpNotUdp) {
        strcpy(fullName, "TCP ");
    } else {
        strcpy(fullName, "UDP ");
    }
    
    strcat(fullName, partialName);
}

void test01TcpNotUdp(int testNoOffset, int tcpNotUdp)
{
    char testName[64];

    DWORD blocks01[1] = {32};
    generateTestName(tcpNotUdp, "echo    32 B", testName);
    test01(0x0101 + testNoOffset, testName, tcpNotUdp, &blocks01[0], 1);

    DWORD blocks02[3] = {250, 250, 250};
    generateTestName(tcpNotUdp, "echo   750 B", testName);
    test01(0x0102 + testNoOffset, testName, tcpNotUdp, &blocks02[0], 3);
    
    DWORD blocks03[3] = {511, 512, 513};
    generateTestName(tcpNotUdp, "echo  1536 B", testName);
    test01(0x0103 + testNoOffset, testName, tcpNotUdp, &blocks03[0], 3);

    DWORD blocks04[3] = {1000, 250, 1000};
    generateTestName(tcpNotUdp, "echo  2250 B", testName);
    test01(0x0104 + testNoOffset, testName, tcpNotUdp, &blocks04[0], 3);

    if(tcpNotUdp) {
        DWORD blocks05[4] = {500, 1000, 1500, 2000};
        generateTestName(tcpNotUdp, "echo 15000 B", testName);
        test01(0x0105 + testNoOffset, testName, tcpNotUdp, &blocks05[0], 4);

        DWORD blocks06[3] = {5000, 10000, 16000};
        generateTestName(tcpNotUdp, "echo 31000 B", testName);
        test01(0x0106 + testNoOffset, testName, tcpNotUdp, &blocks06[0], 3);
    }
}

void test01(WORD testNo, char *testName, BYTE tcpNotUdp, DWORD *blockSizes, WORD blockSizesCount)
{
    DWORD start = getTicks();

    //----------
    out_test_header(testNo, testName);          // show test header
    
    //----------
    // find out the largest block size
    int i;
    int maxBlockSize = 0;

    for(i=0; i<blockSizesCount; i++) {
        if(maxBlockSize < blockSizes[i]) {      // if current max block size is smaller than this block size, store it
            maxBlockSize = blockSizes[i];
        }
    }
    //----------
    // open socket
    int handle;
    
    if(tcpNotUdp) {
        handle = TCP_open(SERVER_ADDR, SERVER_PORT_START, 0, maxBlockSize);
    } else {
        handle = UDP_open(SERVER_ADDR, SERVER_PORT_START + 4);
    }
    
    if(handle < 0) {
        out_result_string(0, "TCP/UDP open() failed");
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
                out_result_string(0, "TCP_wait_state() timeout");
                goto test01close;
            }
        }
        
        if(res != E_NORMAL) {
            out_result_error_string(0, res, "TCP_wait_state() failed");
            goto test01close;
        }
    }
    
    //---------------------
    int res;
    
    for(i=0; i<blockSizesCount; i++) {
        res = sendAndReceive(tcpNotUdp, blockSizes[i], handle, 1);
    
        if(!res) {                              // if single block-send-and-receive operation failed, quit and close
            goto test01close;
        }
    }
    
    //---------------------
    
    out_result(1);                              // success!
    
test01close:
    if(tcpNotUdp) {
        res = TCP_close(handle, 0, 0);          // close
    } else {
        res = UDP_close(handle);                // close
    }
    
    if(res != E_NORMAL) {
        out_result_error_string(0, res, "TCP/UDP close() failed");
    }
}

int sendAndReceive(BYTE tcpNotUdp, DWORD blockSize, int handle, BYTE getBlockNotNdb)
{
    //----------
    // send
    int res;
    DWORD now, endTime;
    DWORD kbs = 1 + (blockSize / 1024);
    
    endTime = getTicks() + (kbs * 200);         // for each kB of data give 1 second to transfer
    
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
        if(now >= endTime) {        // timeout? 
            out_result_error_string(0, blockSize, "send() timeout");
            return 0;
        }
    }
    
    if(res != E_NORMAL) { 
        out_result_error_string(0, res, "send() failed");
        return 0;
    }
    
    //----------
    // wait
    endTime = getTicks() + (kbs * 200);         // for each kB of data give 1 second to transfer
    
    memset(rBuf, 0, blockSize);
    BYTE *pBuf  = rBuf;
    DWORD toGet = blockSize;
    
    while(1) {
        if(toGet <= 0) {                                // nothing to get? quit
            break;
        }
    
        res = CNbyte_count(handle);                     // find out how many bytes are waiting

        if(res > 0) {                                   // something waiting? read it
            if(getBlockNotNdb) {                        // retrieve using CNget_block?
                res = CNget_block(handle, pBuf, res);
                
                if(res != E_NODATA && res < 0) {        // if it's some error, and that error is not E_NODATA, fail
                    out_result_error_string(0, blockSize, "CNget_block() failed");
                    return 0;
                }
            } else {                                    // retrieve using CNget_NDB
                NDB *ndb = CNget_NDB(handle);
    
                if(ndb) {                               // if something retrieved
                    memcpy(pBuf, ndb->ndata, ndb->len); // copy in the data
                    
                    KRfree (ndb->ptr);                  // free the ram
                    KRfree (ndb);
                    
                    res = ndb->len;                     // it was this many bytes
                } else {                                // if nothing retrieved
                    res = 0;
                }
            }
            
            pBuf  += res;
            toGet -= res;
        }

        now = getTicks();
        if(now >= endTime) {                    // timeout? 
            out_result_error_string(0, blockSize, "CNbyte_count() timeout");
            return 0;
        }
    }
    
    //----------
    // data are valid? 
    res = memcmp(rBuf, wBuf, blockSize);
    
    if(res != 0) {
        out_result_string(0, "Received data mismatch");
        return 0;
    }

    //-------
    // if came here, everything is OK
    return 1;
}

void test0130(void)
{
    int handle, res, ok;
    int a,b,c;
    
    out_test_header(0x0130, "UDP - CNbyte_count() on 3 datagrams");
    handle = UDP_open(SERVER_ADDR, SERVER_PORT_START + 4);
    if(handle >= 0) {
        // send 3 x 100 bytes
        (void) UDP_send(handle, wBuf, 100);
        (void) UDP_send(handle, wBuf, 100);
        (void) UDP_send(handle, wBuf, 100);
        
        sleep(1);
        
        res = CNbyte_count(handle);         // see how many bytes we got for reading on this socket
        ok = (res == 300) ? 1 : 0;      
        
        out_result_error(ok, res);
        //-----------------------------------
        out_test_header(0x0131, "UDP - CNget_block() on 3 datagrams");
        
                                            // receive it using 3x CNget_block()
        a = CNget_block (handle, rBuf, 100);
        b = CNget_block (handle, rBuf, 100);
        c = CNget_block (handle, rBuf, 100);
        
        ok = (a == 100 && b == 100 && c == 100) ? 1 : 0;
        out_result_error(ok, res);
        //-----------------------------------
        
        UDP_close(handle);
    } else {
        out_result_string(0, "UDP_open failed");
    }
    
    //////////////////////////////////////////////////////////////////////////////
    
    out_test_header(0x0132, "UDP - CNbyte_count() after partial read");
    handle = UDP_open(SERVER_ADDR, SERVER_PORT_START + 4);
    if(handle >= 0) {
        // send 300 bytes
        (void) UDP_send(handle, wBuf, 300);
        
        sleep(1);
        
        res = CNbyte_count(handle);         // see how many bytes we got for reading on this socket
        ok = (res == 300) ? 1 : 0;      

        if(!ok) {                           // if not enough data, fail
            out_result_error_string(ok, res, "not enough data");
        } else {
            a = CNget_block (handle, rBuf, 100);
            b = CNbyte_count(handle);
            
            ok = (a == 100 && b == 200) ? 1 : 0;
            out_result_error(ok, b);
         }
        
        UDP_close(handle);
    } else {
        out_result_string(0, "UDP_open failed");
    }    
    
    //////////////////////////////////////////////////////////////////////////////
    
    out_test_header(0x0133, "UDP - get 3 DGRAMs with 1 CNget_block");
    handle = UDP_open(SERVER_ADDR, SERVER_PORT_START + 4);
    if(handle >= 0) {
        // send 300 bytes
        (void) UDP_send(handle, wBuf, 100);
        (void) UDP_send(handle, wBuf, 100);
        (void) UDP_send(handle, wBuf, 100);
        
        sleep(1);
        
        res = CNbyte_count(handle);         // see how many bytes we got for reading on this socket
        ok = (res == 300) ? 1 : 0;      

        if(!ok) {                           // if not enough data, fail
            out_result_error_string(ok, res, "not enough data");
        } else {
            a = CNget_block (handle, rBuf, 300);
            b = CNbyte_count(handle);
            
            ok = (a == 300 && b == 0) ? 1 : 0;
            out_result_error(ok, b);
         }
        
        UDP_close(handle);
    } else {
        out_result_string(0, "UDP_open failed");
    }        
    
    //////////////////////////////////////////////////////////////////////////////
    
    out_test_header(0x0134, "UDP - get 3 DGRAMs with 3x CNget_NDB");
    handle = UDP_open(SERVER_ADDR, SERVER_PORT_START + 4);
    if(handle >= 0) {
        // send 300 bytes
        (void) UDP_send(handle, wBuf, 100);
        (void) UDP_send(handle, wBuf, 100);
        (void) UDP_send(handle, wBuf, 100);
        
        sleep(1);
        
        res = CNbyte_count(handle);         // see how many bytes we got for reading on this socket
        ok = (res == 300) ? 1 : 0;      

        if(!ok) {                           // if not enough data, fail
            out_result_error_string(ok, res, "not enough data");
        } else {
            NDB *m,*n,*o;
            
            m = CNget_NDB(handle);
            n = CNget_NDB(handle);
            o = CNget_NDB(handle);
            
            ok = (m != NULL && n != NULL && o != NULL) ? 1 : 0;
            
            if(!ok) {
                out_result_string(ok, "some CNget_NDB failed");
            } else {
                ok = (m->len == 100 && n->len == 100 && o->len == 100) ? 1 : 0;
                
                if(!ok) {
                    out_result_string(ok, "length of NDB block wrong");
                } else {
                    out_result(1);
                }
            }
         }
        
        UDP_close(handle);
    } else {
        out_result_string(0, "UDP_open failed");
    }        

    //////////////////////////////////////////////////////////////////////////////
    
    out_test_header(0x0135, "UDP - get 3 DGRAMs with CNget_char");
    handle = UDP_open(SERVER_ADDR, SERVER_PORT_START + 4);
    if(handle >= 0) {
        // send 300 bytes
        (void) UDP_send(handle, wBuf, 100);
        (void) UDP_send(handle, wBuf, 100);
        (void) UDP_send(handle, wBuf, 100);
        
        sleep(1);
        
        res = CNbyte_count(handle);         // see how many bytes we got for reading on this socket
        ok = (res == 300) ? 1 : 0;      

        b = 1;                              // good for now
        if(!ok) {                           // if not enough data, fail
            out_result_error_string(ok, res, "not enough data");
        } else {
            int i;
            for(i=0; i<300; i++) {
                a = CNget_char(handle);
                
                if(a >= 0) {
                    rBuf[i] = a;
                } else {
                    out_result_error_string(0, a, "error on CNget_char");
                    b = 0;                  // failed
                    break;
                }
            }
        
            if(b) {                         // if good, check data
                a = memcmp(rBuf,       wBuf, 100);
                b = memcmp(rBuf + 100, wBuf, 100);
                c = memcmp(rBuf + 200, wBuf, 100);
                
                ok = (a == 0 && b == 0 && c == 0) ? 1 : 0;
                out_result(ok);
            }
         }
        
        UDP_close(handle);
    } else {
        out_result_string(0, "UDP_open failed");
    }
}


