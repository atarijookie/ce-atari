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

extern DWORD SERVER_ADDR;

#define SERVER_PORT_START   10000

#endif
