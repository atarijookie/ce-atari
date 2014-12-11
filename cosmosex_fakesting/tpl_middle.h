#ifndef _TPL_MIDDLE_H_
#define _TPL_MIDDLE_H_

	//-------------
	// memory alloc / free functions
    void *       KRmalloc_mid (int32);
    void         KRfree_mid (void *);
    int32        KRgetfree_mid (int16);
    void *       KRrealloc_mid (void *, int32);
	//-------------
	// misc
    char *       get_err_text_mid (int16);							// Returns error description for a given error number.
    char *       getvstr_mid (char *);								// Inquires about a configuration string.
    int16        carrier_detect_mid (void);							// obsolete, just dummy
	//-------------
	// TCP functions
    int16        TCP_open_mid (uint32, uint16, uint16, uint16);
    int16        TCP_close_mid (int16, int16, int16 *);
    int16        TCP_send_mid (int16, void *, int16);
    int16        TCP_wait_state_mid (int16, int16, int16);
    int16        TCP_ack_wait_mid (int16, int16);
	//-------------
	// UDP function
    int16        UDP_open_mid (uint32, uint16);
    int16        UDP_close_mid (int16);
    int16        UDP_send_mid (int16, void *, int16);
	//-------------
	// Connection Manager
    int16        CNkick_mid (int16);								// Kick a connection.
    int16        CNbyte_count_mid (int16);							// Inquires about the number of received bytes pending.
    int16        CNget_char_mid (int16);							// Fetch a received character or byte from a connection.
    NDB *        CNget_NDB_mid (int16);								// Fetch a received chunk of data from a connection.
    int16        CNget_block_mid (int16, void *, int16);			// Fetch a received block of data from a connection.
	//-------------
	// misc
    void         housekeep_mid (void);								// obsolete, just dummy
    int16        resolve_mid (char *, char **, uint32 *, int16);	// Carries out DNS queries.
	//-------------
	// serial port functions, just dummies
    void         ser_disable_mid (void);							// obsolete, just dummy
    void         ser_enable_mid (void);								// obsolete, just dummy
	//-------------
    int16        set_flag_mid (int16);								// Requests a semaphore.
    void         clear_flag_mid (int16);							// Releases a semaphore.
    CIB *        CNgetinfo_mid (int16);								// Fetch information about a connection.
    int16        on_port_mid (char *);								// Switches a port into active mode and triggers initialisation.
    void         off_port_mid (char *);								// Switches a port into inactive mode.
    int16        setvstr_mid (char *, char *);						// Sets configuration strings.
    int16        query_port_mid (char *);							// Inquires if a specified port is currently active.
    int16        CNgets_mid (int16, char *, int16, char);			// Fetch a delimited block of data from a connection.
	//-------------
	// ICMP functions
    int16        ICMP_send_mid       (uint32, uint8, uint8, void *, uint16);
    int16        ICMP_handler_mid    (int16 (*) (IP_DGRAM *), int16);
    void         ICMP_discard_mid    (IP_DGRAM *);
	//-------------
    int16        TCP_info_mid (int16, TCPIB *);
    int16        cntrl_port_mid (char *, uint32, int16);				// Inquires and sets various parameters of STinG ports.

    char        *get_error_text_mid(int16 error_code);
    void        house_keep_mid(void);
    void        serial_dummy_mid(void);
    
#endif    
