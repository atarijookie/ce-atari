//--------------------------------------------------
#include <mint/osbind.h> 
#include <stdio.h>

#include "version.h"
#include "global.h"
#include "transprt.h"
#include "stdlib.h"
#include "out.h"

//--------------------------------------------------
DRV_LIST  *sting_drivers;
TPL       *tpl;

#define MAGIC   "STiKmagic"
#define CJTAG   "STiK"

BYTE find_STiNG(void);

// 0xc0a87b9a -- 192.168.123.154
// 0xc0a87b8e -- 192.168.123.142
#define SERVER_ADDR         0xc0a87b8e
#define SERVER_PORT_START   10000

void doTest01(void);

int main(void)
{
    initBuffer();

    (void) Cconws("STiNG test...\r\n");

    BYTE found = Supexec (find_STiNG);
    if(!found) {
        sleep(3);
        return 0;
    }

    doTest01();
    
    writeBufferToFile();
    deinitBuffer();
    
    sleep(3);
	return 0;
}

void doTest01(void)
{
    //----------
    // open socket
    int handle = TCP_open(SERVER_ADDR, SERVER_PORT_START, 0, 1000);
    
    if(handle < 0) {
        out_tr_bw(0x0001, "TCP_open() failed ", 0, handle);
        return;
    }

    out_tr_bw(0x0001, "TCP_open() OK ", 1, handle);

    //----------
    // wait until connected
    int res, i;

    for(i=0; i<15; i++) {
        res = TCP_wait_state(handle, TESTABLISH, 1);
    
        if(res == E_NORMAL) {
            break;
        }

        out_tr_bw(0x0001, "TCP_wait_state() - another loop", 0, res);
    }
    
    if(res != E_NORMAL) {
        out_tr_bw(0x0001, "TCP_wait_state() failed ", 0, res);
        goto test01close;
    }
    
    //----------
    // send
    char tmpOut[32];
    for(i=0; i<32; i++) {
        tmpOut[i] = i;
    }
    
    while(1) {
        res = TCP_send(handle, tmpOut, 32); // try to send

        if(res != E_OBUFFULL) {
            break;
        }

        out_tr_bw(0x0001, "TCP_send() - another loop", 0, res);
    }
    
    if(res != E_NORMAL) { 
        out_tr_bw(0x0001, "TCP_send() failed ", 0, res);
        goto test01close;
    }
    out_tr_bw(0x0001, "TCP_send() OK ", 1, res);
    
    //----------
    // wait
    for(i=0; i<15; i++) {
        res = CNbyte_count(handle);
        
        if(res >= 32) {
            break;
        }

        out_sw("waiting for data, CNbyte_count() returned ", res);
        sleep(1);
    }
    
    if(res < 32) {                      // not enough data?
        out_tr_bw(0x0001, "CNbyte_count() - not enough data or error", 0, res);
        goto test01close;
    }
    
    //----------
    // receive
    char tmpIn[32];
    memset(tmpIn, 0, 32);
    
    res = CNget_block(handle, tmpIn, 32);
    
    if(res != 32) { 
        out_tr_bw(0x0001, "CNget_block() failed ", 0, res);
        goto test01close;
    }
    
    //----------
    // data are valid? 
    res = memcmp(tmpOut, tmpIn, 32);
    
    if(res != 0) {
        out_tr_bw(0x0001, "Received data mistmatch", 0, res);
        goto test01close;
    }
    
    out_tr_bw(0x0001, "Received data OK", 1, res);
    
test01close:
    res = TCP_close(handle, 0, 0);              // close
    
    int ok = (res == E_NORMAL) ? 1 : 0;    
    out_tr_bw(0x0001, "TCP_close", ok, res);
}

BYTE find_STiNG(void)
{
   DWORD *work;
   sting_drivers = NULL;
   
   for (work = * (DWORD **) 0x5a0L; *work != 0L; work += 2) {
        if (*work == 'STiK') {
            sting_drivers = (DRV_LIST *) *(++work);
            break;
        }
    }
    
    if(sting_drivers == NULL) {
        (void) Cconws("STiNG not found.\r\n");
        return 0;
    }
   
    if(strncmp(sting_drivers->magic, MAGIC, 8) != 0) {
        (void) Cconws("STiNG MAGIC mismatch.\r\n");
        return 0;
    }

    tpl = (TPL *) (*sting_drivers->get_dftab) (TRANSPORT_DRIVER);

    if (tpl == (TPL *) NULL) {
        (void) Cconws("STiNG TRANSPORT layer not found.\r\n");
        return 0;
    }

    (void) Cconws("STiNG found.\r\n");
    return 1;
}



