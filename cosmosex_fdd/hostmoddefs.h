#ifndef _HOSTMODDEFS_H_
#define _HOSTMODDEFS_H_

// typed of devices / modules we support
#define HOSTMOD_CONFIG				1
#define HOSTMOD_LINUX_TERMINAL		2
#define HOSTMOD_TRANSLATED_DISK		3
#define HOSTMOD_NETWORK_ADAPTER		4
#define HOSTMOD_FDD_SETUP           5

// commands for HOSTMOD_FDD_SETUP
#define FDD_CMD_IDENTIFY                    0
#define FDD_CMD_GETSILOCONTENT              1

#define FDD_CMD_UPLOADIMGBLOCK_START        10
#define FDD_CMD_UPLOADIMGBLOCK_FULL         11
#define FDD_CMD_UPLOADIMGBLOCK_PART         12
#define FDD_CMD_UPLOADIMGBLOCK_DONE_OK      13
#define FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL    14

#define FDD_CMD_SWAPSLOTS                   20
#define FDD_CMD_REMOVESLOT                  21
#define FDD_CMD_NEW_EMPTYIMAGE              22

#define FDD_CMD_DOWNLOADIMG_START           30
#define FDD_CMD_DOWNLOADIMG_GETBLOCK        31
#define FDD_CMD_DOWNLOADIMG_DONE            32

#define FDD_UPLOADSTART_RES_ONDEVICECOPY    0xDC

#endif
