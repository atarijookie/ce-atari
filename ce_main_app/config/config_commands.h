#ifndef CONFIG_COMMANDS_H
#define CONFIG_COMMANDS_H

// commands for HOSTMOD_CONFIG
#define CFG_CMD_IDENTIFY            0
#define CFG_CMD_KEYDOWN             1
#define CFG_CMD_SET_RESOLUTION      2
#define CFG_CMD_UPDATING_QUERY      3
#define CFG_CMD_REFRESH             0xfe
#define CFG_CMD_GO_HOME             0xff

#define CFG_CMD_LINUXCONSOLE_GETSTREAM  10

// two values of a last byte of LINUXCONSOLE stream - more data, or no more data
#define LINUXCONSOLE_NO_MORE_DATA   0x00
#define LINUXCONSOLE_GET_MORE_DATA  0xda

#endif // CONFIG_COMMANDS_H
