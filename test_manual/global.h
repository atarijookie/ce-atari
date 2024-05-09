#ifndef _GLOBAL_H_
#define _GLOBAL_H_

#define Clear_home()    (void) Cconws("\33E")
#define Cursor_on()     (void) Cconws("\33e")

#include <stdint.h>

    #ifndef uint8_t
        #define uint8_t  	unsigned char
        #define uint16_t  	uint16_t
        #define uint32_t 	uint32_t
    #endif

    #ifndef FALSE
        #define FALSE		0
        #define TRUE		1
    #endif

#define HZ_200     ((volatile uint32_t *) 0x04BA) /* 200 Hz system clock */ 

#endif
