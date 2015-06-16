// typed of devices / modules we support
#define HOSTMOD_CONFIG				1
#define HOSTMOD_LINUX_TERMINAL		2
#define HOSTMOD_TRANSLATED_DISK		3
#define HOSTMOD_NETWORK_ADAPTER		4

// commands for HOSTMOD_TRANSLATED_DISK
#define TRAN_CMD_IDENTIFY           0
// ...other commands are just function codes from gemdos.h

#define ACC_GET_MOUNTS			0x80
#define ACC_UNMOUNT_DRIVE       0x81


