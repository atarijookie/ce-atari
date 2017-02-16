// vim: expandtab shiftwidth=4 tabstop=4
//--------------------------------------------------
#include <mint/osbind.h>
#include <stdio.h>

#include "version.h"
#include "global.h"
#include "transprt.h"
#include "stdlib.h"
#include "out.h"

extern BYTE *rBuf;

void speedCNgets(void)
{
    int res, handle;
    int n;
    DWORD bytes = 0;
    BYTE txBuf[2];
    DWORD start, end;
    char speed[16];

    out_test_header(0x8000, "TCP SPEED CNgets");
    handle = TCP_open(SERVER_ADDR, SERVER_PORT_START + 2, 0, 3000);
    if(handle < 0) {
        out_result_error_string(0, handle, "open failed");
        return;
    }
    res = TCP_wait_state(handle, TESTABLISH, 3);
    if(res != E_NORMAL) {
        out_result_error_string(0, res, "cannot connect");
        TCP_close(handle, 0, 0);
        return;
    }
    n = 1000; /* 1000 lines */
    txBuf[0] = (n >> 8) & 0xff;
    txBuf[1] = n & 0xff;
    res = TCP_send(handle, txBuf, 2);
    if(res != E_NORMAL) {
        out_result_error_string(0, res, "error on send");
        TCP_close(handle, 0, 0);
        return;
    }
    start = getTicks();
    while(n > 0) {
        res = CNgets(handle, rBuf, 256, '\n');
        if(res < 0) {
            if(res == E_NODATA) continue;   // try again !
            out_result_error_string(0, res, "error on receive");
            TCP_close(handle, 0, 0);
            return;
        }
        bytes += res;
        n--;
    }
    end = getTicks();
    // speed in kB/sec = (bytes * 200) / (end - start) * 1000)
    // kB/sec = b/msec
    speed[0] = ' ';
    intToString(bytes / ((end - start) * 5), -1, speed + 1);
    strcat(speed, " kB/sec");
    out_result_string(1, speed);
}

void speedCNget_block(void)
{
    int res, handle;
    int n;
    DWORD bytes = 0;
    BYTE txBuf[2];
    DWORD start, end;
    char speed[16];

    out_test_header(0x8000, "TCP SPEED CNget_block");
    handle = TCP_open(SERVER_ADDR, SERVER_PORT_START + 2, 0, 3000);
    if(handle < 0) {
        out_result_error_string(0, handle, "open failed");
        return;
    }
    res = TCP_wait_state(handle, TESTABLISH, 3);
    if(res != E_NORMAL) {
        out_result_error_string(0, res, "cannot connect");
        TCP_close(handle, 0, 0);
        return;
    }
    n = 20000; /* 20000 lines */
    txBuf[0] = (n >> 8) & 0xff;
    txBuf[1] = n & 0xff;
    res = TCP_send(handle, txBuf, 2);
    if(res != E_NORMAL) {
        out_result_error_string(0, res, "error on send");
        TCP_close(handle, 0, 0);
        return;
    }
    start = getTicks();
    for(;;) {
        res = CNbyte_count(handle);
        if(res < 0) {
            if(res == E_EOF) break; // end of stream
            if(res == E_NODATA) continue;   // try again !
            out_result_error_string(0, res, "CNbyte_count() error");
            TCP_close(handle, 0, 0);
            return;
        } else if (res == 0) {
            continue;   // try again
        }
        res = CNget_block(handle, rBuf, res); // maximum is 32767
        if(res < 0) {
            if(res == E_EOF) break; // end of stream
            if(res == E_NODATA) continue;   // try again !
            out_result_error_string(0, res, "error on receive");
            TCP_close(handle, 0, 0);
            return;
        }
        bytes += res;
    }
    end = getTicks();
    // speed in kB/sec = (bytes * 200) / (end - start) * 1000)
    // kB/sec = b/msec
    speed[0] = ' ';
    intToString(bytes / ((end - start) * 5), -1, speed + 1);
    strcat(speed, " kB/sec");
    out_result_string(1, speed);
}

void speedCNget_NDB(void)
{
    int res, handle;
    int n;
    DWORD bytes = 0;
    BYTE txBuf[2];
    DWORD start, end;
    char speed[16];
    NDB *ndb;

    out_test_header(0x8000, "TCP SPEED CNget_NDB");
    handle = TCP_open(SERVER_ADDR, SERVER_PORT_START + 2, 0, 3000);
    if(handle < 0) {
        out_result_error_string(0, handle, "open failed");
        return;
    }
    res = TCP_wait_state(handle, TESTABLISH, 3);
    if(res != E_NORMAL) {
        out_result_error_string(0, res, "cannot connect");
        TCP_close(handle, 0, 0);
        return;
    }
    n = 20000; /* 20000 lines */
    txBuf[0] = (n >> 8) & 0xff;
    txBuf[1] = n & 0xff;
    res = TCP_send(handle, txBuf, 2);
    if(res != E_NORMAL) {
        out_result_error_string(0, res, "error on send");
        TCP_close(handle, 0, 0);
        return;
    }
    start = getTicks();
    for(;;) {
        res = CNbyte_count(handle);
        if(res < 0) {
            if(res == E_EOF) break; // end of stream
            if(res == E_NODATA) continue;   // try again !
            out_result_error_string(0, res, "CNbyte_count() error");
            TCP_close(handle, 0, 0);
            return;
        } else if (res == 0) {
            continue;   // try again
        }
        ndb = CNget_NDB(handle);
        if(ndb) {
            // ndb->ndata / ndb->next ?
            bytes += ndb->len;
            KRfree (ndb->ptr);                  // free the ram
            KRfree (ndb);
        } else {
            out_result_string(0, "CNget_NDB() returned NULL");
            TCP_close(handle, 0, 0);
            return;
        }
    }
    end = getTicks();
    // speed in kB/sec = (bytes * 200) / (end - start) * 1000)
    // kB/sec = b/msec
    speed[0] = ' ';
    intToString(bytes / ((end - start) * 5), -1, speed + 1);
    strcat(speed, " kB/sec");
    out_result_string(1, speed);
}

void doSpeedTest(void)
{
    speedCNgets();
    speedCNget_block();
    speedCNget_NDB();
}
