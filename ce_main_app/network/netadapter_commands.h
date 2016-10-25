#ifndef _NETADAPTER_COMMANDS_H_
#define _NETADAPTER_COMMANDS_H_

/* command format :
 * short : ID 'C' 'E' CMD arg1 arg2
 * long :  ID|1f 00 'C' 'E' CMD arg1 arg2 arg3 arg4 ...
 */

#define NET_CMD_IDENTIFY                0x00
/* no argument
 * return a buffer with :
 * 32 bytes = identification string + padding
 * 4 bytes = local IP address */

// TCP functions
#define NET_CMD_TCP_OPEN                0x10
/* data buffer argument :
 * 4 bytes : remote host (0 for listen)
 * 2 bytes : remote port
 * 2 bytes : type of service (not used)
 * 2 bytes : buffer size (CNget_NDB() packet size)
 * 2 bytes : local port (useful for listening sockets
 * returns a connection handle
 */
#define NET_CMD_TCP_CLOSE               0x11
/* data buffer argument :
 * 2 bytes : connection handle
 * returns E_PARAMETER / E_NORMAL */
#define NET_CMD_TCP_SEND                0x12
/* arg1 = connection handle
 * arg2/arg3 = data length (word)
 * arg4 = isOdd
 * arg5 = oddByte
 * + data buffer
 * returns E_PARAMETER / E_OBUFFULL / E_NORMAL */
#define NET_CMD_TCP_WAIT_STATE          0x13
/* not used */
#define NET_CMD_TCP_ACK_WAIT            0x14
/* not used */
#define NET_CMD_TCP_INFO                0x15
/* not used */

// UDP FUNCTIONS
#define NET_CMD_UDP_OPEN                0x20
/* data buffer argument :
 * 4 bytes : remote host (0 for listen)
 * 2 bytes : remote port
 * 2 bytes : type of service (not used)
 * 2 bytes : buffer size (CNget_NDB() packet size)
 * 2 bytes : local port (useful for listening sockets
 * returns a connection handle
 */
#define NET_CMD_UDP_CLOSE               0x21
/* data buffer argument :
 * 2 bytes : connection handle
 * returns E_PARAMETER / E_NORMAL */
#define NET_CMD_UDP_SEND                0x22
/* arg1 = connection handle
 * arg2/arg3 = data length (word)
 * arg4 = isOdd
 * arg5 = oddByte
 * + data buffer
 * returns E_PARAMETER / E_OBUFFULL / E_NORMAL */

// ICMP FUNCTIONS
#define NET_CMD_ICMP_SEND_EVEN          0x30
#define NET_CMD_ICMP_SEND_ODD           0x31
#define NET_CMD_ICMP_HANDLER            0x32
#define NET_CMD_ICMP_DISCARD            0x33
#define NET_CMD_ICMP_GET_DGRAMS         0x34

// CONNECTION MANAGER
#define NET_CMD_CNKICK                  0x40
/* not used */
#define NET_CMD_CNBYTE_COUNT            0x41
/* not used */
#define NET_CMD_CNGET_CHAR              0x42
/* arg1 = connection handle
 * arg5 = "charsUsed"
 * return a data buffer :
 * 1 byte : data byte count
 * n bytes : data bytes
 * returns E_PARAMETER / E_NORMAL */
#define NET_CMD_CNGET_NDB               0x43
/* arg1 = connection handle
 * arg2 = data flag. If zero, returns just size. If non-zero, return data.
 * arg5 = "charsUsed"
 * return a data buffer, either :
 *   n bytes : data bytes (predefined ndbSize)
 * or :
 *   4 bytes : "nextNdbSize"
 *   1 byte : sector count
 * returns E_PARAMETER / E_NORMAL */
#define NET_CMD_CNGET_BLOCK             0x44
/* arg1 = connection handle
 * arg2,arg3 (Word) = block length
 * arg5 = "charsUsed"
 * return a data buffer :
 * block length bytes = data
 * returns E_PARAMETER / E_NODATA / E_NORMAL */
#define NET_CMD_CNGETINFO               0x45
/* not used */
#define NET_CMD_CNGETS                  0x46
/* arg1 = connection handle
 * arg2,arg3 (Word) = max length
 * arg4 = delimiter character
 * arg5 = "charsUsed"
 * return a data buffer :
 * n bytes (null terminated string, excluding delimiter)
 * returns E_PARAMETER / E_NODATA / E_BIGBUF / E_NORMAL */

#define NET_CMD_CN_UPDATE_INFO          0x47
/* no arguments
 * return a buffer of data
 * NET_HANDLES_COUNT x 4 = bytes waiting to be read in socket
 * NET_HANDLES_COUNT x 1 = connection status
 * NET_HANDLES_COUNT x 2 = local port
 * NET_HANDLES_COUNT x 4 = remote host
 * NET_HANDLES_COUNT x 2 = remote port
 * 4 = bytes waiting to be read on ICMP socket
 */
#define NET_CMD_GET_NEXT_NDB_SIZE       0x4E

// MISC
#define NET_CMD_RESOLVE                 0x50
#define NET_CMD_ON_PORT                 0x51
/* not used */
#define NET_CMD_OFF_PORT                0x52
/* not used */
#define NET_CMD_QUERY_PORT              0x53
/* not used */
#define NET_CMD_CNTRL_PORT              0x54
/* not used */

#define NET_CMD_RESOLVE_GET_RESPONSE    0x55

#define RW_ALL_TRANSFERED   0       // return this is all the required data was read / written
#define RW_PARTIAL_TRANSFER 1       // return this if not all of the required data was read / written

#define DELIMITER_NOT_FOUND             0xffffffff

#define RES_DIDNT_FINISH_YET            0xdf

#endif


