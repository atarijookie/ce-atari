//----------------------------------------
// CosmosEx fake STiNG - by Jookie, 2014
// Based on sources of original STiNG
//----------------------------------------

#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <support.h>
#include <stdint.h>
#include <stdio.h>

#include "globdefs.h"
#include "stdlib.h"
#include "api.h"
#include "tcp.h"
#include "icmp.h"
#include "con_man.h"
#include "setup.h"
#include "port.h"
#include "tpl_middle.h"

extern int32 _pbase;

int16        setvstr (char name[], char value[]);
char *       getvstr (char name[]);

char *       get_error_text (int16 error_code);

int16        set_flag (int16 flag);
void         clear_flag (int16 flag);

CONFIG      conf;

int32 fun00(void); int32 fun01(void); int32 fun02(void); int32 fun03(void); int32 fun04(void); int32 fun05(void); int32 fun06(void); int32 fun07(void); int32 fun08(void); int32 fun09(void); 
int32 fun10(void); int32 fun11(void); int32 fun12(void); int32 fun13(void); int32 fun14(void); int32 fun15(void); int32 fun16(void); int32 fun17(void); int32 fun18(void); int32 fun19(void); 
int32 fun20(void); int32 fun21(void); int32 fun22(void); int32 fun23(void); int32 fun24(void); int32 fun25(void); int32 fun26(void); int32 fun27(void); int32 fun28(void); int32 fun29(void); 
int32 fun30(void); int32 fun31(void); int32 fun32(void); int32 fun33(void); int32 fun34(void); int32 fun35(void); int32 fun36(void); int32 fun37(void); int32 fun38(void); int32 fun39(void); 

CLIENT_API  tpl  = { "TRANSPORT_TCPIP", "Jookie", TCP_DRIVER_VERSION, 
                      fun00, fun01, fun02, fun03, fun04, fun05, fun06, fun07, fun08, fun09,
                      fun10, fun11, fun12, fun13, fun14, fun15, fun16, fun17, fun18, fun19,
                      fun20, fun21, fun22, fun23, fun24, fun25, fun26, fun27, fun28, fun29,
                      fun30, fun31, fun32, fun33, fun34, fun35, fun36 };
               
STX_API     stxl = { "MODULE_LAYER", "Jookie", STX_LAYER_VERSION, 
                      NULL, NULL, NULL, NULL, NULL, 
                      NULL, NULL, NULL, NULL, 
                      NULL, NULL, NULL, NULL, 
                      NULL, NULL, NULL, NULL,
                      NULL, NULL, NULL
               };
GENERIC     cookie = { "STiKmagic", get_drv_func, ETM_exec, &conf, NULL, { (DRV_HDR *) &tpl, (DRV_HDR *) & stxl } };
long        my_jar[8] = {  0L, 4L  };

extern char semaphors[MAX_SEMAPHOR];

long  init_cookie()
{
   int   cnt_cookie;
   long  *work, *jar, *new;

   conf.new_cookie = FALSE;

   if ((work = * (long **) 0x5a0L) == NULL) {
        conf.new_cookie = TRUE;
        * (long **) 0x5a0L = & my_jar[0];
      }

   for (work = * (long **) 0x5a0L, cnt_cookie = 0; *work != 0L; work += 2, cnt_cookie++)
        if (*work == 'STiK')
             return (-1L);

   if (work[1] - 1 <= cnt_cookie) {
        if ((jar = (long *) Malloc ((cnt_cookie + 8) * 2 * sizeof (long))) == NULL)
             return (-1L);
        for (work = * (long **) 0x5a0L, new = jar; *work != 0L; work += 2, new += 2)
             new[0] = work[0],  new[1] = work[1];
        new[0] = 0L;
        new[1] = cnt_cookie + 8;
        work = new;
        * (long **) 0x5a0L = jar;
        conf.new_cookie = TRUE;
      }

   work[2] = work[0];   work[0] = 'STiK';
   work[3] = work[1];   work[1] = (long) &cookie;

   cookie.basepage = (BASEPAGE *) _pbase;

   return (0L);
}

DRV_HDR *get_drv_func (char *drv_name)
{
    int val = strcmp(drv_name, "TRANSPORT_TCPIP");
    if(val == 0) {
        return (DRV_HDR *) &tpl;
    }
    
    return ((DRV_HDR *) NULL);
}

int16 ETM_exec (char *module)
{

    return 0;
}

void house_keep(void)
{
    update_con_info();                          // update connections info structs (max once per 100 ms)
}

int16 set_flag(int16 flag)                      // set semaphore
{
    if(flag >= 0 && flag < MAX_SEMAPHOR) {      // valid semaphore number?
        if(semaphors[flag] != 0) {              // It was set ? Return TRUE
            return 1;
        } else {                                // It wasn't set? 
            semaphors[flag] = 0xff;             // set the semaphore
            return 0;                           // return FALSE
        }        
    } 
    
    // invalid semaphore number? return false
	return 0;
}

void clear_flag(int16 flag)                     // clear semaphore
{
    if(flag >= 0 && flag < MAX_SEMAPHOR) {      // valid semaphore number?
        semaphors[flag] = 0;
    }
}

void serial_dummy(void)
{
    // Do really nothing - obsolete
}

int16 carrier_detect(void)
{
    // Do really nothing - obsolete
    return 1;
} 
