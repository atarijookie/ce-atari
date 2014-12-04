#ifndef _GLOBDEFS_H_
#define _GLOBDEFS_H_

//----------------------------------------
// CosmosEx fake STiNG - by Jookie, 2014
// Based on sources of original STiNG
//----------------------------------------

#include <mint/basepage.h>
 
#define  FALSE       0
#define  TRUE        1

#define  TCP_DRIVER_VERSION    "01.20"
#define  STX_LAYER_VERSION     "01.05"
#define  CFG_NUM               100
#define  MAX_HANDLE            10
#define  MAX_SEMAPHOR          64
#define  LOOPBACK              0x7f000001L

#define DMA_BUFFER_SIZE         (8 * 512)
/*--------------------------------------------------------------------------*/
// CosmosEx - types of devices / modules we support

#define HOSTMOD_CONFIG				1
#define HOSTMOD_LINUX_TERMINAL		2
#define HOSTMOD_TRANSLATED_DISK		3
#define HOSTMOD_NETWORK_ADAPTER		4
#define HOSTMOD_FDD_SETUP           5

/*--------------------------------------------------------------------------*/

/*
 *   Protocols.
 */

#define  ICMP        1        /* IP assigned number for ICMP                */
#define  TCP         6        /* IP assigned number for TCP                 */
#define  UDP        17        /* IP assigned number for UDP                 */


/*
 *   A concession to portability ...
 */

typedef           char   int8;        /*   Signed  8 bit (char)             */
typedef  unsigned char  uint8;        /* Unsigned  8 bit (byte, octet)      */
typedef           int    int16;       /*   Signed 16 bit (int)              */
typedef  unsigned int   uint16;       /* Unsigned 16 bit (word)             */
typedef           long   int32;       /*   Signed 32 bit                    */
typedef  unsigned long  uint32;       /* Unsigned 32 bit (longword)         */



/*--------------------------------------------------------------------------*/


/*
 *   Network Data Block.  For data delivery.
 */

typedef  struct ndb {
    char        *ptr;       /*  +0: Pointer to base of block. (For free() ;-)    */
    char        *ndata;     /*  +4: Pointer to next data to deliver              */
    uint16      len;        /*  +8: Length of remaining data.                    */
    struct ndb  *next;      /* +10: Next NDB in chain or NULL                    */
 } __attribute__((packed)) NDB;


/*
 *   Addressing information block.
 */

typedef  struct {
    uint16      lport;      /* Local  port        (ie: local machine)       */
    uint16      rport;      /* Remote port        (ie: remote machine)      */
    uint32      rhost;      /* Remote IP address  (ie: remote machine)      */
    uint32      lhost;      /* Local  IP address  (ie: local machine)       */
 } __attribute__((packed)) CAB;


/*
 *   Connection information block.
 */

typedef  struct {
    uint16      protocol;   /* TCP or UDP or ... 0 means CIB is not in use  */
    CAB         address;    /* Adress information                           */
    uint16      status;     /* Net status. 0 means normal                   */
 } __attribute__((packed)) CIB;


typedef struct tcpib {      /* TCP Information Block                        */
    int16       state;      /* Connection state                             */
 } __attribute__((packed)) TCPIB;

/*--------------------------------------------------------------------------*/


/*
 *   IP packet header.
 */

typedef struct {
    unsigned  version   : 4;    /*  +0: IP Version                               */
    unsigned  hd_len    : 4;    /*      Internet Header Length                   */
    unsigned  tos       : 8;    /*  +1: Type of Service                          */
    uint16    length;           /*  +2: Total of all header, options and data    */
    uint16    ident;            /*  +4: Identification for fragmentation         */
    unsigned  reserved  : 1;    /*  +6: Reserved : Must be zero                  */
    unsigned  dont_frg  : 1;    /*      Don't fragment flag                      */
    unsigned  more_frg  : 1;    /*      More fragments flag                      */
    unsigned  frag_ofst : 13;   /*      Fragment offset                          */
    uint8     ttl;              /*  +8: Time to live                             */
    uint8     protocol;         /*  +9: Protocol                                 */
    uint16    hdr_chksum;       /* +10: Header checksum                          */
    uint32    ip_src;           /* +12: Source IP address                        */
    uint32    ip_dest;          /* +16: Destination IP address                   */
 } __attribute__((packed)) IP_HDR;


/*
 *   Internal IP packet representation.
 */

typedef  struct ip_packet {
    IP_HDR    hdr;              /*  +0: Header of IP packet                      */
    void      *options;         /* +20: Options data block                       */
    int16     opt_length;       /* +24: Length of options data block             */
    void      *pkt_data;        /* +26: IP packet data block                     */
    int16     pkt_length;       /* +30: Length of IP packet data block           */
    uint32    timeout;          /* +32: Timeout of packet life                   */
    uint32    ip_gateway;       /* +36: Gateway for forwarding this packet       */
    struct port_desc  *recvd;   /* +40: Receiving port                           */
    struct ip_packet  *next;    /* +44: Next IP packet in IP packet queue        */
 } __attribute__((packed)) IP_DGRAM;


/*
 *   Defragmentation queue entries.
 */

typedef struct defrag_rsc {
    IP_DGRAM  *dgram;           /* Datagram to be reassembled               */
    uint16    ttl_data;         /* Total data length for defragmentation    */
    uint16    act_space;        /* Current space of reassembly buffer       */
    void      *blk_bits;        /* Fragment block bits table                */
    struct defrag_rsc  *next;   /* Next defrag resources in defrag queue    */
 } __attribute__((packed)) DEFRAG;


/*
 *   Protocol array entry for received data.
 */

typedef  struct {
    int16     active;           /* Protocol is installed                    */
    IP_DGRAM  *queue;           /* Link to first entry in received queue    */
    DEFRAG    *defrag;          /* Link to defragmentation queue            */
    int16  (* process)	(IP_DGRAM *);   /* Call to process packet     */
 } __attribute__((packed)) IP_PRTCL;



/*--------------------------------------------------------------------------*/


/*
 *   Internal port descriptor.
 */

typedef  struct port_desc {
    char      *name;            /* Name of port                             */
    int16     type;             /* Type of port                             */
    int16     active;           /* Flag for port active or not              */
    uint32    flags;            /* Type dependent operational flags         */
    uint32    ip_addr;          /* IP address of this network adapter       */
    uint32    sub_mask;         /* Subnet mask of attached network          */
    int16     mtu;              /* Maximum packet size to go through        */
    int16     max_mtu;          /* Maximum allowed value for mtu            */
    int32     stat_sd_data;     /* Statistics of sent data                  */
    IP_DGRAM  *send;            /* Link to first entry in send queue        */
    int32     stat_rcv_data;    /* Statistics of received data              */
    IP_DGRAM  *receive;         /* Link to first entry in receive queue     */
    int16     stat_dropped;     /* Statistics of dropped datagrams          */
    struct drv_desc   *driver;  /* Driver program to handle this port       */
    struct port_desc  *next;    /* Next port in port chain                  */
 } __attribute__((packed)) PORT;


/*
 *   Link Type Definitions.
 */

#define  L_INTERNAL   0           /* Internal pseudo port                   */
#define  L_SER_PTP    1           /*   Serial point to point type link      */
#define  L_PAR_PTP    2           /* Parallel point to point type link      */
#define  L_SER_BUS    3           /*   Serial            bus type link      */
#define  L_PAR_BUS    4           /* Parallel            bus type link      */
#define  L_SER_RING   5           /*   Serial           ring type link      */
#define  L_PAR_RING   6           /* Parallel           ring type link      */
#define  L_MASQUE     7           /*   Masquerading pseudo port             */


/*
 *   Port driver descriptor.
 */

typedef  struct drv_desc {
    int16   (* set_state) (PORT *, int16);       /* Setup and shutdown */
    int16   (* cntrl) (PORT *, uint32, int16);   /* Control functions  */
    void    (* send) (PORT *);                   /* Send packets       */
    void    (* receive) (PORT *);                /* Receive packets    */
    char             *name;     /* Name of driver                           */
    char             *version;  /* Version of driver in "xx.yy" format      */
    uint16           date;      /* Compile date in GEMDOS format            */
    char             *author;   /* Name of programmer                       */
    struct drv_desc  *next;     /* Next driver in driver chain              */
    BASEPAGE         *basepage; /* Basepage of this module                  */
 } __attribute__((packed)) DRIVER;



/*--------------------------------------------------------------------------*/


/*
 *   High level protocol module descriptor.
 */

typedef  struct lay_desc {
    char             *name;          /* Name of layer                       */
    char             *version;       /* Version of layer in xx.yy format    */
    uint32           flags;          /* Private data                        */
    uint16           date;           /* Compile date in GEMDOS format       */
    char             *author;        /* Name of programmer                  */
    int16            stat_dropped;   /* Statistics of dropped data units    */
    struct lay_desc  *next;          /* Next layer in driver chain          */
    BASEPAGE         *basepage;      /* Basepage of this module             */
 } __attribute__((packed)) LAYER;



/*--------------------------------------------------------------------------*/


/*
 *   Entry definition for function chain.
 */

typedef  struct func_list {
    int16        (* handler) (IP_DGRAM *);
    struct func_list  *next;
 } __attribute__((packed)) FUNC_LIST;



/*--------------------------------------------------------------------------*/


/*
 *   CN functions structure for TCP and UDP.
 */

typedef  struct {
    int16    (* CNkick) (void *);
    int16    (* CNbyte_count) (void *);
    int16    (* CNget_char) (void *);
    NDB *    (* CNget_NDB) (void *);
    int16    (* CNget_block) (void *, void *, int16);
    CIB *    (* CNgetinfo) (void *);
    int16    (* CNgets) (void *, char *, int16, char);
 } __attribute__((packed)) CN_FUNCS;



/*--------------------------------------------------------------------------*/


/*
 *   STinG global configuration structure.
 */

typedef  struct {
    uint32     client_ip;       /* IP address of local machine (obsolete)   */
    uint16     ttl;             /* Default TTL for normal packets           */
    char       *cv[CFG_NUM+1];  /* Space for config variables               */
    int16      max_num_ports;   /* Maximum number of ports supported        */
    uint32     received_data;   /* Counter for data being received          */
    uint32     sent_data;       /* Counter for data being sent              */
    int16      active;          /* Flag for polling being active            */
    int16      thread_rate;     /* Time between subsequent thread calls     */
    int16      frag_ttl;        /* Time To Live for reassembly resources    */
    PORT       *ports;          /* Pointer to first entry in PORT chain     */
    DRIVER     *drivers;        /* Pointer to first entry in DRIVER chain   */
    LAYER      *layers;         /* Pointer to first entry in LAYER chain    */
    FUNC_LIST  *interupt;       /* List of application interupt handlers    */    
    FUNC_LIST  *icmp;           /* List of application ICMP handlers        */    
    int32      stat_all;        /* All datagrams that pass are counted here */
    int32      stat_lo_mem;     /* Dropped due to low memory                */
    int32      stat_ttl_excd;   /* Dropped due to Time-To-Live exceeded     */
    int32      stat_chksum;     /* Dropped due to failed checksum test      */
    int32      stat_unreach;    /* Dropped due to no way to deliver it      */
    void       *memory;         /* Pointer to main memory for KRcalls       */
    int16      new_cookie;      /* Flag indicating if new jar was created   */
 } __attribute__((packed)) CONFIG;



/*--------------------------------------------------------------------------*/


/*
 *   Entry for routing table.
 */

typedef  struct {
    uint32  template;           /* Net to be reached this way               */
    uint32  netmask;            /* Corresponding subnet mask                */
    uint32  ip_gateway;         /* Next gateway on the way to dest. host    */
    PORT    *port;              /* Port to route the datagram to            */
 } __attribute__((packed)) ROUTE_ENTRY;


/*
 *   Router return values.
 */

#define  NET_UNREACH     ((void *)  0L)    /* No entry for IP found         */
#define  HOST_UNREACH    ((void *) -1L)    /* Entry found but port inactive */
#define  NO_NETWORK      ((void *) -6L)    /* Routing table empty           */
#define  NO_HOST         ((void *) -7L)    /* Currently unused              */



/*--------------------------------------------------------------------------*/


/*
 *   ICMP types.
 */

#define  ICMP_ECHO_REPLY      0       /* Response to echo request           */
#define  ICMP_DEST_UNREACH    3       /* IP error : Destination unreachable */
#define  ICMP_SRC_QUENCH      4       /* IP error : Source quench           */
#define  ICMP_REDIRECT        5       /* IP hint : Redirect datagrams       */
#define  ICMP_ECHO            8       /* Echo requested                     */
#define  ICMP_ROUTER_AD       9       /* Router advertisement               */
#define  ICMP_ROUTER_SOL      10      /* Router solicitation                */
#define  ICMP_TIME_EXCEED     11      /* Datagram TTL exceeded, discarded   */
#define  ICMP_PARAMETER       12      /* IP error : Parameter problem       */
#define  ICMP_STAMP_REQU      13      /* Timestamp requested                */
#define  ICMP_STAMP_REPLY     14      /* Response to timestamp request      */
#define  ICMP_INFO_REQU       15      /* Information requested (obsolete)   */
#define  ICMP_INFO_REPLY      16      /* Response to info req. (obsolete)   */
#define  ICMP_MASK_REQU       17      /* Subnet mask requested              */
#define  ICMP_MASK_REPLY      18      /* Response to subnet mask request    */



/*--------------------------------------------------------------------------*/


/*
 *   Handler flag values.
 */

#define  HNDLR_SET        0         /* Set new handler if space             */
#define  HNDLR_FORCE      1         /* Force new handler to be set          */
#define  HNDLR_REMOVE     2         /* Remove handler entry                 */
#define  HNDLR_QUERY      3         /* Inquire about handler entry          */



/*--------------------------------------------------------------------------*/


/*
 *   Buffer for inquiring port names.
 */

typedef  struct {
    PORT    *opaque;            /* PORT for current name                    */
    int16   name_len;           /* Length of port name buffer               */
    char    *port_name;         /* Buffer address                           */
 } __attribute__((packed)) PNTA;


#define  CTL_KERN_FIRST_PORT    ('K' << 8 | 'F')    /* Query first port     */
#define  CTL_KERN_NEXT_PORT     ('K' << 8 | 'N')    /* Query following port */
#define  CTL_KERN_FIND_PORT     ('K' << 8 | 'G')    /* Port struct. address */



/*--------------------------------------------------------------------------*/


/*
 *   Other cntrl_port() opcodes.
 */

#define  CTL_GENERIC_SET_IP     ('G' << 8 | 'H')    /* Set IP address       */
#define  CTL_GENERIC_GET_IP     ('G' << 8 | 'I')    /* Get IP address       */
#define  CTL_GENERIC_SET_MASK   ('G' << 8 | 'L')    /* Set subnet mask      */
#define  CTL_GENERIC_GET_MASK   ('G' << 8 | 'M')    /* Get subnet mask      */
#define  CTL_GENERIC_SET_MTU    ('G' << 8 | 'N')    /* Set line MTU         */
#define  CTL_GENERIC_GET_MTU    ('G' << 8 | 'O')    /* Get line MTU         */
#define  CTL_GENERIC_GET_MMTU   ('G' << 8 | 'P')    /* Get maximum MTU      */
#define  CTL_GENERIC_GET_TYPE   ('G' << 8 | 'T')    /* Get port type        */
#define  CTL_GENERIC_GET_STAT   ('G' << 8 | 'S')    /* Get statistics       */
#define  CTL_GENERIC_CLR_STAT   ('G' << 8 | 'C')    /* Clear statistics     */



/*--------------------------------------------------------------------------*/

/*
 *   TCP connection states.
 */

#define  TCLOSED       0    /* No connection.  Null, void, absent, ...      */
#define  TLISTEN       1    /* Wait for remote request                      */
#define  TSYN_SENT     2    /* Connect request sent, await matching request */
#define  TSYN_RECV     3    /* Wait for connection ack                      */
#define  TESTABLISH    4    /* Connection established, handshake completed  */
#define  TFIN_WAIT1    5    /* Await termination request or ack             */
#define  TFIN_WAIT2    6    /* Await termination request                    */
#define  TCLOSE_WAIT   7    /* Await termination request from local user    */
#define  TCLOSING      8    /* Await termination ack from remote TCP        */
#define  TLAST_ACK     9    /* Await ack of terminate request sent          */
#define  TTIME_WAIT   10    /* Delay, ensures remote has received term' ack */

/*--------------------------------------------------------------------------*/

/*
 *   Miscellaneous Definitions.
 */

#define  MAX_CLOCK    86400000L      /* Maximum value for sting_clock       */



/*--------------------------------------------------------------------------*/


/* 
Error return values:
0    - OK
0x50 - 0x6f -- connection handle - received from device
0xe0 - 0xff -- STiNG error codes
*/

#define  E_NORMAL         0     /* No error occured ...                     0x00 */
#define  E_OBUFFULL      -1     /* Output buffer is full                    0xff */
#define  E_NODATA        -2     /* No data available                        0xfe */
#define  E_EOF           -3     /* EOF from remote                          0xfd */
#define  E_RRESET        -4     /* Reset received from remote               0xfc */
#define  E_UA            -5     /* Unacceptable packet received, reset      0xfb */
#define  E_NOMEM         -6     /* Something failed due to lack of memory   0xfa */
#define  E_REFUSE        -7     /* Connection refused by remote             0xf9 */
#define  E_BADSYN        -8     /* A SYN was received in the window         0xf8 */
#define  E_BADHANDLE     -9     /* Bad connection handle used.              0xf7 */
#define  E_LISTEN        -10    /* The connection is in LISTEN state        0xf6 */
#define  E_NOCCB         -11    /* No free CCB's available                  0xf5 */
#define  E_NOCONNECTION  -12    /* No connection matches this packet (TCP)  0xf4 */
#define  E_CONNECTFAIL   -13    /* Failure to connect to remote port (TCP)  0xf3 */
#define  E_BADCLOSE      -14    /* Invalid TCP_close() requested            0xf2 */
#define  E_USERTIMEOUT   -15    /* A user function timed out                0xf1 */
#define  E_CNTIMEOUT     -16    /* A connection timed out                   0xf0 */
#define  E_CANTRESOLVE   -17    /* Can't resolve the hostname               0xef */
#define  E_BADDNAME      -18    /* Domain name or dotted dec. bad format    0xee */
#define  E_LOSTCARRIER   -19    /* The modem disconnected                   0xed */
#define  E_NOHOSTNAME    -20    /* Hostname does not exist                  0xec */
#define  E_DNSWORKLIMIT  -21    /* Resolver Work limit reached              0xeb */
#define  E_NONAMESERVER  -22    /* No nameservers could be found for query  0xea */
#define  E_DNSBADFORMAT  -23    /* Bad format of DS query                   0xe9 */
#define  E_UNREACHABLE   -24    /* Destination unreachable                  0xe8 */
#define  E_DNSNOADDR     -25    /* No address records exist for host        0xe7 */
#define  E_NOROUTINE     -26    /* Routine unavailable                      0xe6 */
#define  E_LOCKED        -27    /* Locked by another application            0xe5 */
#define  E_FRAGMENT      -28    /* Error during fragmentation               0xe4 */
#define  E_TTLEXCEED     -29    /* Time To Live of an IP packet exceeded    0xe3 */
#define  E_PARAMETER     -30    /* Problem with a parameter                 0xe2 */
#define  E_BIGBUF        -31    /* Input buffer is too small for data       0xe1 */
#define  E_FNAVAIL       -32    /* Function not available                   0xe0 */
#define  E_LASTERROR      32    /* ABS of last error code in this list      0xe0 */


/*--------------------------------------------------------------------------*/

// if sign bit is set, extend the sign to whole WORD
#define extendByteToWord(X)    ( ((X & 0x80)==0) ? X : (0xff00 | X) )

#define handleIsFromCE(X)		(X >= 0x50 && X <= 0x6f)
#define handleAtariToCE(X)		(X  + 0x50)
#define handleCEtoAtari(X)		(X  - 0x50)

/*--------------------------------------------------------------------------*/

#ifndef BYTE
    #include <stdint.h>

    #define BYTE  	unsigned char
    #define WORD  	uint16_t
    #define DWORD 	uint32_t
#endif

#define READ_BUFFER_SIZE    512

typedef struct {
    CIB     cib;                            // connection information block
    DWORD   bytesToRead;                    // how many bytes we can read from this connection
    BYTE    tcpConnectionState;             // TCP connection states -- TCLOSED, TLISTEN, ...
        
    WORD rCount;					        // how much data is buffer (specifies where the next read data could come from)
    WORD rStart;					        // starting index of where we should start reading the buffer
    BYTE rBuf[READ_BUFFER_SIZE];
} TConInfo;

#define FASTRAM_BUFFER_SIZE	4096
/*--------------------------------------------------------------------------*/
// for retrieving real params from stack, as gcc calling convention doesn't match the Pure C cdecl calling convention
#define getStackPointer()   BYTE *sp = __builtin_frame_address(0) + 8;

#define getDwordFromSP()  ({ DWORD a = (DWORD)  *((DWORD *) sp);    sp += 4;    a; })
#define getWordFromSP()   ({  WORD a = (WORD)   *(( WORD *) sp);    sp += 2;    a; })
#define getByteFromSP()   ({  BYTE a = (BYTE)   *(( BYTE *) sp);    sp += 2;    a; })
#define getVoidPFromSP()  ({ void *p = (void *) *((DWORD *) sp);    sp += 4;    p; })
/*--------------------------------------------------------------------------*/

#endif


