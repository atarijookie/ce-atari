//----------------------------------------
// CosmosEx fake STiNG - by Jookie, 2014
// Based on sources of original STiNG
//----------------------------------------

#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <mint/ostruct.h>
#include <unistd.h>
#include <support.h>
#include <stdint.h>

#include <stdio.h>
#include <string.h>

#include "globdefs.h"
#include "tcp.h"
#include "udp.h"
#include "con_man.h"

#define  NUM_LAYER   2

extern int32 _pbase;

typedef  struct drv_header {
           char     *module, *author, *version;
     } DRV_HDR;

typedef  struct driver {
           char     magic[10];
           DRV_HDR  * /* cdecl */ (*get_drvfunc) (char *);
           int16      /* cdecl */ (*ETM_exec) (char *);
           CONFIG   *cfg;
           BASEPAGE *basepage;
           DRV_HDR  *layer[NUM_LAYER];
     } GENERIC;

typedef  struct client_layer {
    char *     module;      /* Specific string that can be searched for     */
    char *     author;      /* Any string                                   */
    char *     version;     /* Format `00.00' Version:Revision              */
	//-------------
	// memory alloc / free functions
    void *     /* cdecl */  (* KRmalloc) (int32);
    void       /* cdecl */  (* KRfree) (void *);
    int32      /* cdecl */  (* KRgetfree) (int16);
    void *     /* cdecl */  (* KRrealloc) (void *, int32);
	//-------------
	// misc
    char *     /* cdecl */  (* get_err_text) (int16);							// Returns error description for a given error number.
    char *     /* cdecl */  (* getvstr) (char *);								// Inquires about a configuration string.
    int16      /* cdecl */  (* carrier_detect) (void);							// obsolete, just dummy
	//-------------
	// TCP functions
    int16      /* cdecl */  (* TCP_open) (uint32, uint16, uint16, uint16);
    int16      /* cdecl */  (* TCP_close) (int16, int16, int16 *);
    int16      /* cdecl */  (* TCP_send) (int16, void *, int16);
    int16      /* cdecl */  (* TCP_wait_state) (int16, int16, int16);
    int16      /* cdecl */  (* TCP_ack_wait) (int16, int16);
	//-------------
	// UDP function
    int16      /* cdecl */  (* UDP_open) (uint32, uint16);
    int16      /* cdecl */  (* UDP_close) (int16);
    int16      /* cdecl */  (* UDP_send) (int16, void *, int16);
	//-------------
	// Connection Manager
    int16      /* cdecl */  (* CNkick) (int16);									// Kick a connection.
    int16      /* cdecl */  (* CNbyte_count) (int16);							// Inquires about the number of received bytes pending.
    int16      /* cdecl */  (* CNget_char) (int16);								// Fetch a received character or byte from a connection.
    NDB *      /* cdecl */  (* CNget_NDB) (int16);								// Fetch a received chunk of data from a connection.
    int16      /* cdecl */  (* CNget_block) (int16, void *, int16);				// Fetch a received block of data from a connection.
	//-------------
	// misc
    void       /* cdecl */  (* housekeep) (void);								// obsolete, just dummy
    int16      /* cdecl */  (* resolve) (char *, char **, uint32 *, int16);		// Carries out DNS queries.
	//-------------
	// serial port functions, just dummies
    void       /* cdecl */  (* ser_disable) (void);								// obsolete, just dummy
    void       /* cdecl */  (* ser_enable) (void);								// obsolete, just dummy
	//-------------
    int16      /* cdecl */  (* set_flag) (int16);								// Requests a semaphore.
    void       /* cdecl */  (* clear_flag) (int16);								// Releases a semaphore.
    CIB *      /* cdecl */  (* CNgetinfo) (int16);								// Fetch information about a connection.
    int16      /* cdecl */  (* on_port) (char *);								// Switches a port into active mode and triggers initialisation.
    void       /* cdecl */  (* off_port) (char *);								// Switches a port into inactive mode.
    int16      /* cdecl */  (* setvstr) (char *, char *);						// Sets configuration strings.
    int16      /* cdecl */  (* query_port) (char *);							// Inquires if a specified port is currently active.
    int16      /* cdecl */  (* CNgets) (int16, char *, int16, char);			// Fetch a delimited block of data from a connection.
	//-------------
	// ICMP functions
    int16      /* cdecl */  (* ICMP_send) (uint32, uint8, uint8, void *, uint16);
    int16      /* cdecl */  (* ICMP_handler) (int16 /* cdecl */ (*) (IP_DGRAM *), int16);
    void       /* cdecl */  (* ICMP_discard) (IP_DGRAM *);
	//-------------
    int16      /* cdecl */  (* TCP_info) (int16, void *);
    int16      /* cdecl */  (* cntrl_port) (char *, uint32, int16);				// Inquires and sets various parameters of STinG ports.
 } CLIENT_API;

typedef  struct stx_layer {
    char *     module;      /* Specific string that can be searched for     */
    char *     author;      /* Any string                                   */
    char *     version;     /* Format `00.00' Version:Revision              */
    void       /* cdecl */  (* set_dgram_ttl) (IP_DGRAM *);
    int16      /* cdecl */  (* check_dgram_ttl) (IP_DGRAM *);
    int16      /* cdecl */  (* routing_table) (void);
    int32      /* cdecl */  (* set_sysvars) (int16, int16);
    void       /* cdecl */  (* query_chains) (PORT **, DRIVER **, LAYER **);
    int16      /* cdecl */  (* IP_send) (uint32, uint32, uint8, uint16, uint8, uint8, uint16,
                                   void *, uint16, void *, uint16);
    IP_DGRAM * /* cdecl */  (* IP_fetch) (int16);
    int16      /* cdecl */  (* IP_handler) (int16, int16 /* cdecl */ (*) (IP_DGRAM *), int16);
    void       /* cdecl */  (* IP_discard) (IP_DGRAM *, int16);
    int16      /* cdecl */  (* PRTCL_announce) (int16);
    int16      /* cdecl */  (* PRTCL_get_parameters) (uint32, uint32 *, int16 *, uint16 *);
    int16      /* cdecl */  (* PRTCL_request) (void *, CN_FUNCS *);
    void       /* cdecl */  (* PRTCL_release) (int16);
    void *     /* cdecl */  (* PRTCL_lookup) (int16, CN_FUNCS *);
    int16      /* cdecl */  (* TIMER_call) (void /* cdecl */ (*) (void), int16);
    int32      /* cdecl */  (* TIMER_now) (void);
    int32      /* cdecl */  (* TIMER_elapsed) (int32);
    int32      /* cdecl */  (* protect_exec) (void *, int32 /* cdecl */ (*) (void *));
    int16      /* cdecl */  (* get_route_entry) (int16, uint32 *, uint32 *, PORT **, uint32 *);
    int16      /* cdecl */  (* set_route_entry) (int16, uint32, uint32, PORT *, uint32);
 } STX_API;


int16      /* cdecl */  setvstr (char name[], char value[]);
char *     /* cdecl */  getvstr (char name[]);

char *     /* cdecl */  get_error_text (int16 error_code);
void *     /* cdecl */  KRmalloc (int32 size);
void       /* cdecl */  KRfree (void *mem_block);
int32      /* cdecl */  KRgetfree (int16 block_flag);
void *     /* cdecl */  KRrealloc (void *mem_block, int32 new_size);

int16      /* cdecl */  set_flag (int16 flag);
void       /* cdecl */  clear_flag (int16 flag);

int16      /* cdecl */  on_port (char *port);
void       /* cdecl */  off_port (char *port);
int16      /* cdecl */  query_port (char *port);
int16      /* cdecl */  cntrl_port (char *port, uint32 argument, int16 code);

int16      /* cdecl */  ICMP_send (uint32 dest, uint8 type, uint8 code, void *data, uint16 len);
int16      /* cdecl */  ICMP_handler (int16 /* cdecl */ (* hndlr) (IP_DGRAM *), int16 flag);
void       /* cdecl */  ICMP_discard (IP_DGRAM *datagram);

long        			init_cookie (void);
DRV_HDR *  /* cdecl */  get_drv_func (char *drv_name);
int16      /* cdecl */  ETM_exec (char *module);
int16      /* cdecl */  resolve (char *domain, char **real, uint32 *ip_list, int16 ip_num);
void       /* cdecl */  serial_dummy (void);
int16      /* cdecl */  carrier_detect (void);
void       /* cdecl */  house_keep (void);


CONFIG      conf;
CLIENT_API  tpl  = { "TRANSPORT_TCPIP", "Peter Rottengatter", TCP_DRIVER_VERSION, 
                      KRmalloc, KRfree, KRgetfree, KRrealloc, 
                      get_error_text, getvstr, carrier_detect, 
                      TCP_open, TCP_close, TCP_send, TCP_wait_state, TCP_ack_wait, 
                      UDP_open, UDP_close, UDP_send, 
                      CNkick, CNbyte_count, CNget_char, CNget_NDB, CNget_block, 
                      house_keep, resolve, serial_dummy, serial_dummy, 
                      set_flag, clear_flag, CNgetinfo, 
                      on_port, off_port, setvstr, query_port, CNgets, 
                      ICMP_send, ICMP_handler, ICMP_discard, TCP_info, cntrl_port
               };
STX_API     stxl = { "MODULE_LAYER", "Peter Rottengatter", STX_LAYER_VERSION, 
                      NULL, NULL, NULL, NULL, NULL, 
                      NULL, NULL, NULL, NULL, 
                      NULL, NULL, NULL, NULL, 
                      NULL, NULL, NULL, NULL,
                      NULL, NULL, NULL
               };
GENERIC     cookie = { "STiKmagic", get_drv_func, ETM_exec, &conf, NULL, { (DRV_HDR *) &tpl, (DRV_HDR *) & stxl } };
long        my_jar[8] = {  0L, 4L  };



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


DRV_HDR *  /* cdecl */  get_drv_func (drv_name)

char  *drv_name;
{
   int  count;

   for (count = 0; count < NUM_LAYER; count++)
        if (strcmp (cookie.layer[count]->module, drv_name) == 0)
             return (cookie.layer[count]);

   return ((DRV_HDR *) NULL);
 }


int16  /* cdecl */  ETM_exec (module)

char  *module;

{
   return (0);
 }

int16  /* cdecl */  resolve (char *domain, char **real_domain, uint32 *ip_list, int16 ip_num)
{

   return (E_CANTRESOLVE);
}


void  /* cdecl */  serial_dummy()
{
   /* Do really nothing, as these functions are obsolete ! */
 }


int16  /* cdecl */  carrier_detect()

{
   return (+1);
}


void  /* cdecl */  house_keep()
{
   /* Do really nothing, as this function is obsolete ! */
}

int16 /* cdecl */ set_flag (int16 flag)
{

	return 0;
}

void /* cdecl */ clear_flag (int16 flag)
{

}

 