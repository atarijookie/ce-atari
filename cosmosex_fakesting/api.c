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
#include "udp.h"
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

CLIENT_API  tpl  = { "TRANSPORT_TCPIP", "Jookie", TCP_DRIVER_VERSION, 
                      KRmalloc_mid, KRfree_mid, KRgetfree_mid, KRrealloc_mid, 
                      get_error_text_mid, getvstr_mid, carrier_detect_mid, 
                      TCP_open_mid, TCP_close_mid, TCP_send_mid, TCP_wait_state_mid, TCP_ack_wait_mid, 
                      UDP_open_mid, UDP_close_mid, UDP_send_mid, 
                      CNkick_mid, CNbyte_count_mid, CNget_char_mid, CNget_NDB_mid, CNget_block_mid, 
                      house_keep_mid, resolve_mid, serial_dummy_mid, serial_dummy_mid, 
                      set_flag_mid, clear_flag_mid, CNgetinfo_mid, 
                      on_port_mid, off_port_mid, setvstr_mid, query_port_mid, CNgets_mid, 
                      ICMP_send_mid, ICMP_handler_mid, ICMP_discard_mid, TCP_info_mid, cntrl_port_mid,
                      UDP_info_mid, RAW_open_mid, RAW_close_mid, RAW_out_mid, CN_setopt_mid, CN_getopt_mid, CNfree_NDB_mid
               };
               
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
