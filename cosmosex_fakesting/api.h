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

typedef  struct client_layer {
    char *     module;      /* Specific string that can be searched for     */
    char *     author;      /* Any string                                   */
    char *     version;     /* Format `00.00' Version:Revision              */
	//-------------
	// memory alloc / free functions
    void *       (* KRmalloc) (int32);
    void         (* KRfree) (void *);
    int32        (* KRgetfree) (int16);
    void *       (* KRrealloc) (void *, int32);
	//-------------
	// misc
    char *       (* get_err_text) (int16);							// Returns error description for a given error number.
    char *       (* getvstr) (char *);								// Inquires about a configuration string.
    int16        (* carrier_detect) (void);							// obsolete, just dummy
	//-------------
	// TCP functions
    int16        (* TCP_open) (uint32, uint16, uint16, uint16);
    int16        (* TCP_close) (int16, int16, int16 *);
    int16        (* TCP_send) (int16, void *, int16);
    int16        (* TCP_wait_state) (int16, int16, int16);
    int16        (* TCP_ack_wait) (int16, int16);
	//-------------
	// UDP function
    int16        (* UDP_open) (uint32, uint16);
    int16        (* UDP_close) (int16);
    int16        (* UDP_send) (int16, void *, int16);
	//-------------
	// Connection Manager
    int16        (* CNkick) (int16);									// Kick a connection.
    int16        (* CNbyte_count) (int16);							// Inquires about the number of received bytes pending.
    int16        (* CNget_char) (int16);								// Fetch a received character or byte from a connection.
    NDB *        (* CNget_NDB) (int16);								// Fetch a received chunk of data from a connection.
    int16        (* CNget_block) (int16, void *, int16);				// Fetch a received block of data from a connection.
	//-------------
	// misc
    void         (* housekeep) (void);								// obsolete, just dummy
    int16        (* resolve) (char *, char **, uint32 *, int16);		// Carries out DNS queries.
	//-------------
	// serial port functions, just dummies
    void         (* ser_disable) (void);								// obsolete, just dummy
    void         (* ser_enable) (void);								// obsolete, just dummy
	//-------------
    int16        (* set_flag) (int16);								// Requests a semaphore.
    void         (* clear_flag) (int16);								// Releases a semaphore.
    CIB *        (* CNgetinfo) (int16);								// Fetch information about a connection.
    int16        (* on_port) (char *);								// Switches a port into active mode and triggers initialisation.
    void         (* off_port) (char *);								// Switches a port into inactive mode.
    int16        (* setvstr) (char *, char *);						// Sets configuration strings.
    int16        (* query_port) (char *);							// Inquires if a specified port is currently active.
    int16        (* CNgets) (int16, char *, int16, char);			// Fetch a delimited block of data from a connection.
	//-------------
	// ICMP functions
    int16        (* ICMP_send)       (uint32, uint8, uint8, void *, uint16);
    int16        (* ICMP_handler)    (int16 (*) (IP_DGRAM *), int16);
    void         (* ICMP_discard)    (IP_DGRAM *);
	//-------------
    int16        (* TCP_info) (int16, void *);
    int16        (* cntrl_port) (char *, uint32, int16);				// Inquires and sets various parameters of STinG ports.
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
