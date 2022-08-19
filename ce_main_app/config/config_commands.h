// vim: expandtab shiftwidth=4 tabstop=4
#ifndef CONFIG_COMMANDS_H
#define CONFIG_COMMANDS_H

// commands for HOSTMOD_CONFIG
#define CFG_CMD_IDENTIFY            0
#define CFG_CMD_KEYDOWN             1
#define CFG_CMD_SET_RESOLUTION      2
#define CFG_CMD_UPDATING_QUERY      3
#define CFG_CMD_REFRESH             0xfe
#define CFG_CMD_GO_HOME             0xff

#define CFG_CMD_SET_CFGVALUE        5

#define CFG_CMD_LINUXCONSOLE_GETSTREAM  10

#define CFG_CMD_GET_APP_NAMES       20
#define CFG_CMD_SET_APP_INDEX       21

// two values of a last byte of LINUXCONSOLE stream - more data, or no more data
#define LINUXCONSOLE_NO_MORE_DATA   0x00
#define LINUXCONSOLE_GET_MORE_DATA  0xda

// types for CFG_CMD_SET_CFGVALUE
#define CFGVALUE_TYPE_STRING    1
#define CFGVALUE_TYPE_INT       2
#define CFGVALUE_TYPE_BOOL      3
#define CFGVALUE_TYPE_ST_PATH   4   // to be translated

#endif // CONFIG_COMMANDS_H
