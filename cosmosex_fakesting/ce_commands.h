#ifndef _CE_COMMANDS_H_
#define _CE_COMMANDS_H_

#define NET_CMD_IDENTIFY                0x00

// TCP functions
#define NET_CMD_TCP_OPEN                0x10
#define NET_CMD_TCP_CLOSE               0x11
#define NET_CMD_TCP_SEND                0x12
#define NET_CMD_TCP_WAIT_STATE          0x13
#define NET_CMD_TCP_ACK_WAIT            0x14
#define NET_CMD_TCP_INFO                0x15

// UDP FUNCTION
#define NET_CMD_UDP_OPEN                0x20
#define NET_CMD_UDP_CLOSE               0x21
#define NET_CMD_UDP_SEND                0x22

// ICMP FUNCTIONS
#define NET_CMD_ICMP_SEND_EVEN          0x30
#define NET_CMD_ICMP_SEND_ODD           0x31
#define NET_CMD_ICMP_HANDLER            0x32
#define NET_CMD_ICMP_DISCARD            0x33
#define NET_CMD_ICMP_GET_DGRAMS         0x34

// CONNECTION MANAGER
#define NET_CMD_CNKICK                  0x40
#define NET_CMD_CNBYTE_COUNT            0x41
#define NET_CMD_CNGET_CHAR              0x42
#define NET_CMD_CNGET_NDB               0x43
#define NET_CMD_CNGET_BLOCK             0x44
#define NET_CMD_CNGETINFO               0x45
#define NET_CMD_CNGETS                  0x46

#define NET_CMD_CN_UPDATE_INFO          0x47

#define NET_CMD_CN_READ_DATA            0x4A
#define NET_CMD_CN_GET_DATA_COUNT       0x4B
#define NET_CMD_CN_LOCATE_DELIMITER     0x4C

// MISC
#define NET_CMD_RESOLVE                 0x50
#define NET_CMD_ON_PORT                 0x51
#define NET_CMD_OFF_PORT                0x52
#define NET_CMD_QUERY_PORT              0x53
#define NET_CMD_CNTRL_PORT              0x54

#define NET_CMD_RESOLVE_GET_RESPONSE    0x55

#define RW_ALL_TRANSFERED   0       // return this is all the required data was read / written
#define RW_PARTIAL_TRANSFER 1       // return this if not all of the required data was read / written

#define DELIMITER_NOT_FOUND             0xffffffff
#define RES_DIDNT_FINISH_YET            0xdf

#endif

