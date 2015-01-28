#ifndef _API_H_
#define _API_H_

#define  NUM_LAYER   2

typedef  struct drv_header {
           char     *module, *author, *version;
     } __attribute__((packed)) DRV_HDR;

typedef  struct driver {
           char     magic[10];
           DRV_HDR  *  (*get_drvfunc) (char *);
           int16       (*ETM_exec) (char *);
           CONFIG   *cfg;
           BASEPAGE *basepage;
           DRV_HDR  *layer[NUM_LAYER];
     } __attribute__((packed)) GENERIC;

typedef struct udpib
{	uint32	request;	/* 32 bit flags requesting various info (following)	*/
	uint16	state;		/* current UDP pseudo state 						*/
	uint32	reserve1;	/* reserved */
	uint32	reserve2;	/* reserved */
}	UDPIB;
     
typedef  struct client_layer {
    char *     module;      /* Specific string that can be searched for     */
    char *     author;      /* Any string                                   */
    char *     version;     /* Format `00.00' Version:Revision              */
    
	//-------------
	// memory alloc / free functions
    int32 (* KRmalloc)      (void);
    int32 (* KRfree)        (void);
    int32 (* KRgetfree)     (void);
    int32 (* KRrealloc)     (void);
	//-------------
	// misc
    int32 (* get_err_text)  (void);
    int32 (* getvstr)       (void);
    int32 (* carrier_detect)(void);
	//-------------
	// TCP functions
    int32 (* TCP_open)      (void);
    int32 (* TCP_close)     (void);
    int32 (* TCP_send)      (void);
    int32 (* TCP_wait_state)(void);
    int32 (* TCP_ack_wait)  (void);
	//-------------
	// UDP function
    int32 (* UDP_open)      (void);
    int32 (* UDP_close)     (void);
    int32 (* UDP_send)      (void);
	//-------------
	// Connection Manager
    int32 (* CNkick)        (void);
    int32 (* CNbyte_count)  (void);
    int32 (* CNget_char)    (void);
    int32 (* CNget_NDB)     (void);
    int32 (* CNget_block)   (void);
	//-------------
	// misc
    int32 (* housekeep)     (void);
    int32 (* resolve)       (void);
	//-------------
	// serial port functions, just dummies
    int32 (* ser_disable)   (void);
    int32 (* ser_enable)    (void);
	//-------------
    int32 (* set_flag)      (void);
    int32 (* clear_flag)    (void);
    int32 (* CNgetinfo)     (void);
    int32 (* on_port)       (void);
    int32 (* off_port)      (void);
    int32 (* setvstr)       (void);
    int32 (* query_port)    (void);
    int32 (* CNgets)        (void);
	//-------------
	// ICMP functions
    int32 (* ICMP_send)     (void);
    int32 (* ICMP_handler)  (void);
    int32 (* ICMP_discard)  (void);
	//-------------
    int32 (* TCP_info)      (void);
    int32 (* cntrl_port)    (void);
 } __attribute__((packed)) CLIENT_API;

typedef  struct stx_layer {
    char *     module;      /* Specific string that can be searched for     */
    char *     author;      /* Any string                                   */
    char *     version;     /* Format `00.00' Version:Revision              */
    void         (* set_dgram_ttl) (IP_DGRAM *);
    int16        (* check_dgram_ttl) (IP_DGRAM *);
    int16        (* routing_table) (void);
    int32        (* set_sysvars) (int16, int16);
    void         (* query_chains) (PORT **, DRIVER **, LAYER **);
    int16        (* IP_send) (uint32, uint32, uint8, uint16, uint8, uint8, uint16,
                                   void *, uint16, void *, uint16);
    IP_DGRAM *   (* IP_fetch) (int16);
    int16        (* IP_handler) (int16, int16  (*) (IP_DGRAM *), int16);
    void         (* IP_discard) (IP_DGRAM *, int16);
    int16        (* PRTCL_announce) (int16);
    int16        (* PRTCL_get_parameters) (uint32, uint32 *, int16 *, uint16 *);
    int16        (* PRTCL_request) (void *, CN_FUNCS *);
    void         (* PRTCL_release) (int16);
    void *       (* PRTCL_lookup) (int16, CN_FUNCS *);
    int16        (* TIMER_call) (void  (*) (void), int16);
    int32        (* TIMER_now) (void);
    int32        (* TIMER_elapsed) (int32);
    int32        (* protect_exec) (void *, int32  (*) (void *));
    int16        (* get_route_entry) (int16, uint32 *, uint32 *, PORT **, uint32 *);
    int16        (* set_route_entry) (int16, uint32, uint32, PORT *, uint32);
 } __attribute__((packed)) STX_API;
 
long         init_cookie (void);
DRV_HDR *    get_drv_func (char *drv_name);

int16        ETM_exec (char *module);
void         house_keep (void);
void         serial_dummy (void);
int16        carrier_detect (void);

#endif
