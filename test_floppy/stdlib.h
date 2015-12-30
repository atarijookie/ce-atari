#ifndef _STDLIB_H_
#define _STDLIB_H_

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

// 200 Hz system clock
#define HZ_200     ((volatile DWORD *) 0x04BA)

void *	memcpy ( void * destination, const void * source, int num );
void *	memset ( void * ptr, int value, int num );
int		strlen ( const char * str );
char *	strncpy ( char * destination, const char * source, int num );
int		strncmp ( const char * str1, const char * str2, int num );
void	sleep(int seconds);
void    msleep(int ms);

DWORD getTicks(void);

#endif
