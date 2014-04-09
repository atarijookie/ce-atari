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

#define FDD_CMD_UPLOADIMGBLOCK_FULL         10
#define FDD_CMD_UPLOADIMGBLOCK_PART         11
#define FDD_CMD_UPLOADIMGBLOCK_DONE_OK      12
#define FDD_CMD_UPLOADIMGBLOCK_DONE_FAIL    13

#define FDD_CMD_SWAPSLOTS                   20
#define FDD_CMD_REMOVESLOT                  21

#endif
