#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define HOSTMOD_CONFIG				1
#define HOSTMOD_LINUX_TERMINAL		2
#define HOSTMOD_TRANSLATED_DISK		3
#define HOSTMOD_NETWORK_ADAPTER		4

#define CFG_CMD_IDENTIFY			0
#define CFG_CMD_KEYDOWN				1
#define CFG_CMD_SET_RESOLUTION      2
#define CFG_CMD_UPDATING_QUERY      3
#define CFG_CMD_REFRESH             0xfe
#define CFG_CMD_GO_HOME				0xff

#define CFG_CMD_LINUXCONSOLE_GETSTREAM  10

// two values of a last byte of LINUXCONSOLE stream - more data, or no more data
#define LINUXCONSOLE_NO_MORE_DATA   0x00
#define LINUXCONSOLE_GET_MORE_DATA  0xda

#define Clear_home()    (void) Cconws("\33E")
#define Cursor_on()     (void) Cconws("\33e")

#include <stdint.h>

    #ifndef BYTE
        #define BYTE  	unsigned char
        #define WORD  	uint16_t
        #define DWORD 	uint32_t
    #endif

    #ifndef FALSE
        #define FALSE		0
        #define TRUE		1
    #endif

#define HZ_200     ((volatile DWORD *) 0x04BA) /* 200 Hz system clock */ 

#endif
