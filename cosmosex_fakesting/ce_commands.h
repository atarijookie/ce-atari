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
#define NET_CMD_ICMP_SEND               0x30
#define NET_CMD_ICMP_HANDLER            0x31
#define NET_CMD_ICMP_DISCARD            0x32

// CONNECTION MANAGER
#define NET_CMD_CNKICK                  0x40
#define NET_CMD_CNBYTE_COUNT            0x41
#define NET_CMD_CNGET_CHAR              0x42
#define NET_CMD_CNGET_NDB               0x43
#define NET_CMD_CNGET_BLOCK             0x44
#define NET_CMD_CNGETINFO               0x45
#define NET_CMD_CNGETS                  0x46

#define NET_CMD_CN_UPDATE_INFO          0x47

// MISC
#define NET_CMD_RESOLVE                 0x50
#define NET_CMD_ON_PORT                 0x51
#define NET_CMD_OFF_PORT                0x52
#define NET_CMD_QUERY_PORT              0x53
#define NET_CMD_CNTRL_PORT              0x54

#endif

