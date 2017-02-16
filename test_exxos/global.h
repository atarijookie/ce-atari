#ifndef _GLOBAL_H_
#define _GLOBAL_H_

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

#define HOSTMOD_CONFIG              1
#define HOSTMOD_LINUX_TERMINAL      2
#define HOSTMOD_TRANSLATED_DISK     3
#define HOSTMOD_NETWORK_ADAPTER     4
#define HOSTMOD_FDD_SETUP           5

#define TRAN_CMD_IDENTIFY           0
#define TRAN_CMD_GETDATETIME        1

#define DATE_OK                     0
#define DATE_ERROR                  2
#define DATE_DATETIME_UNKNOWN       4

#define DEVTYPE_OFF                 0
#define DEVTYPE_SD                  1
#define DEVTYPE_RAW                 2
#define DEVTYPE_TRANSLATED          3

#define Clear_home()    (void) Cconws("\33E")

#endif
