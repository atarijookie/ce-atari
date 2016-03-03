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

void doTest01(void)
{
    generateWriteBuffer();
    
    test01TcpNotUdp(0,    1);          // offset    0, TCP
    test01TcpNotUdp(0x10, 0);          // offset 0x10, UDP
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
