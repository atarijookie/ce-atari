#ifndef _STING_H_
#define _STING_H_

//--------------------------------------------------------------------------
// Protocols.
 
#define  ICMP        1        // IP assigned number for ICMP                
#define  TCP         6        // IP assigned number for TCP                 
#define  UDP        17        // IP assigned number for UDP                 

//--------------------------------------------------------------------------
// Router return values.

#define  NET_UNREACH     ((void *)  0L)    // No entry for IP found         
#define  HOST_UNREACH    ((void *) -1L)    // Entry found but port inactive 
#define  NO_NETWORK      ((void *) -6L)    // Routing table empty           
#define  NO_HOST         ((void *) -7L)    // Currently unused              

//--------------------------------------------------------------------------
// ICMP types.
 
#define  ICMP_ECHO_REPLY      0       // Response to echo request           
#define  ICMP_DEST_UNREACH    3       // IP error : Destination unreachable 
#define  ICMP_SRC_QUENCH      4       // IP error : Source quench           
#define  ICMP_REDIRECT        5       // IP hint : Redirect datagrams       
#define  ICMP_ECHO            8       // Echo requested                     
#define  ICMP_ROUTER_AD       9       // Router advertisement               
#define  ICMP_ROUTER_SOL      10      // Router solicitation                
#define  ICMP_TIME_EXCEED     11      // Datagram TTL exceeded, discarded   
#define  ICMP_PARAMETER       12      // IP error : Parameter problem       
#define  ICMP_STAMP_REQU      13      // Timestamp requested                
#define  ICMP_STAMP_REPLY     14      // Response to timestamp request      
#define  ICMP_INFO_REQU       15      // Information requested (obsolete)   
#define  ICMP_INFO_REPLY      16      // Response to info req. (obsolete)   
#define  ICMP_MASK_REQU       17      // Subnet mask requested              
#define  ICMP_MASK_REPLY      18      // Response to subnet mask request    

//--------------------------------------------------------------------------
// TCP connection states.
 
#define  TCLOSED       0    // No connection.  Null, void, absent, ...      
#define  TLISTEN       1    // Wait for remote request                      
#define  TSYN_SENT     2    // Connect request sent, await matching request 
#define  TSYN_RECV     3    // Wait for connection ack                      
#define  TESTABLISH    4    // Connection established, handshake completed  
#define  TFIN_WAIT1    5    // Await termination request or ack             
#define  TFIN_WAIT2    6    // Await termination request                    
#define  TCLOSE_WAIT   7    // Await termination request from local user    
#define  TCLOSING      8    // Await termination ack from remote TCP        
#define  TLAST_ACK     9    // Await ack of terminate request sent          
#define  TTIME_WAIT   10    // Delay, ensures remote has received term' ack 

//--------------------------------------------------------------------------
// Error return values:
// 0    - OK
// 0x50 - 0x6f -- connection handle - received from device
// 0xe0 - 0xff -- STiNG error codes

#define  E_NORMAL         0     // No error occured ...                     0x00 
#define  E_OBUFFULL      -1     // Output buffer is full                    0xff 
#define  E_NODATA        -2     // No data available                        0xfe 
#define  E_EOF           -3     // EOF from remote                          0xfd 
#define  E_RRESET        -4     // Reset received from remote               0xfc 
#define  E_UA            -5     // Unacceptable packet received, reset      0xfb 
#define  E_NOMEM         -6     // Something failed due to lack of memory   0xfa 
#define  E_REFUSE        -7     // Connection refused by remote             0xf9 
#define  E_BADSYN        -8     // A SYN was received in the window         0xf8 
#define  E_BADHANDLE     -9     // Bad connection handle used.              0xf7 
#define  E_LISTEN        -10    // The connection is in LISTEN state        0xf6 
#define  E_NOCCB         -11    // No free CCB's available                  0xf5 
#define  E_NOCONNECTION  -12    // No connection matches this packet (TCP)  0xf4 
#define  E_CONNECTFAIL   -13    // Failure to connect to remote port (TCP)  0xf3 
#define  E_BADCLOSE      -14    // Invalid TCP_close() requested            0xf2 
#define  E_USERTIMEOUT   -15    // A user function timed out                0xf1 
#define  E_CNTIMEOUT     -16    // A connection timed out                   0xf0 
#define  E_CANTRESOLVE   -17    // Can't resolve the hostname               0xef 
#define  E_BADDNAME      -18    // Domain name or dotted dec. bad format    0xee 
#define  E_LOSTCARRIER   -19    // The modem disconnected                   0xed 
#define  E_NOHOSTNAME    -20    // Hostname does not exist                  0xec 
#define  E_DNSWORKLIMIT  -21    // Resolver Work limit reached              0xeb 
#define  E_NONAMESERVER  -22    // No nameservers could be found for query  0xea 
#define  E_DNSBADFORMAT  -23    // Bad format of DS query                   0xe9 
#define  E_UNREACHABLE   -24    // Destination unreachable                  0xe8 
#define  E_DNSNOADDR     -25    // No address records exist for host        0xe7 
#define  E_NOROUTINE     -26    // Routine unavailable                      0xe6 
#define  E_LOCKED        -27    // Locked by another application            0xe5 
#define  E_FRAGMENT      -28    // Error during fragmentation               0xe4 
#define  E_TTLEXCEED     -29    // Time To Live of an IP packet exceeded    0xe3 
#define  E_PARAMETER     -30    // Problem with a parameter                 0xe2 
#define  E_BIGBUF        -31    // Input buffer is too small for data       0xe1 
#define  E_FNAVAIL       -32    // Function not available                   0xe0 
#define  E_LASTERROR      32    // ABS of last error code in this list      0xe0 

//--------------------------------------------------------------------------

#define handleIsFromCE(X)		(X >= 0x50 && X <= 0x6f)
#define handleAtariToCE(X)		(X  + 0x50)
#define handleCEtoAtari(X)		(X  - 0x50)

//--------------------------------------------------------------------------

#endif

