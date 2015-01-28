#ifndef _TPL_MIDDLE_H_
#define _TPL_MIDDLE_H_

#include "api.h"

	//-------------
	// memory alloc / free functions
    void *       KRmalloc_mid (BYTE *sp);
    void         KRfree_mid (BYTE *sp);
    int32        KRgetfree_mid (BYTE *sp);
    void *       KRrealloc_mid (BYTE *sp);
	//-------------
	// misc
    char *       get_err_text_mid (BYTE *sp);							// Returns error description for a given error number.
    char *       getvstr_mid (BYTE *sp);								// Inquires about a configuration string.
    int16        carrier_detect_mid(BYTE *sp);							// obsolete, just dummy
	//-------------
	// TCP functions
    int16        TCP_open_mid (BYTE *sp);
    int16        TCP_close_mid (BYTE *sp);
    int16        TCP_send_mid (BYTE *sp);
    int16        TCP_wait_state_mid (BYTE *sp);
    int16        TCP_ack_wait_mid (BYTE *sp);
	//-------------
	// UDP function
    int16        UDP_open_mid (BYTE *sp);
    int16        UDP_close_mid (BYTE *sp);
    int16        UDP_send_mid (BYTE *sp);
	//-------------
	// Connection Manager
    int16        CNkick_mid (BYTE *sp);								// Kick a connection.
    int16        CNbyte_count_mid (BYTE *sp);						// Inquires about the number of received bytes pending.
    int16        CNget_char_mid (BYTE *sp);							// Fetch a received character or byte from a connection.
    NDB *        CNget_NDB_mid (BYTE *sp);							// Fetch a received chunk of data from a connection.
    int16        CNget_block_mid (BYTE *sp);			            // Fetch a received block of data from a connection.
	//-------------
	// misc
    void         housekeep_mid (BYTE *sp);							// obsolete, just dummy
    int16        resolve_mid (BYTE *sp);	                        // Carries out DNS queries.
	//-------------
	// serial port functions, just dummies
    void         ser_disable_mid (BYTE *sp);						// obsolete, just dummy
    void         ser_enable_mid (BYTE *sp);							// obsolete, just dummy
	//-------------
    int16        set_flag_mid (BYTE *sp);							// Requests a semaphore.
    void         clear_flag_mid (BYTE *sp);							// Releases a semaphore.
    CIB *        CNgetinfo_mid (BYTE *sp);							// Fetch information about a connection.
    int16        on_port_mid (BYTE *sp);							// Switches a port into active mode and triggers initialisation.
    void         off_port_mid (BYTE *sp);							// Switches a port into inactive mode.
    int16        setvstr_mid (BYTE *sp);						    // Sets configuration strings.
    int16        query_port_mid (BYTE *sp);							// Inquires if a specified port is currently active.
    int16        CNgets_mid (BYTE *sp);			                    // Fetch a delimited block of data from a connection.
	//-------------
	// ICMP functions
    int16        ICMP_send_mid       (BYTE *sp);
    int16        ICMP_handler_mid    (BYTE *sp);
    void         ICMP_discard_mid    (BYTE *sp);
	//-------------
    int16        TCP_info_mid (BYTE *sp);
    int16        cntrl_port_mid (BYTE *sp);				// Inquires and sets various parameters of STinG ports.

    char        *get_error_text_mid(BYTE *sp);
    void        serial_dummy_mid(BYTE *sp);
    
#endif    
