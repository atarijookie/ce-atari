#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stdint.h>

#define BYTE  	unsigned char
#define WORD  	uint16_t
#define DWORD 	uint32_t

void *	memcpy ( void * destination, const void * source, int num );
void *	memset ( void * ptr, int value, int num );
int		strlen ( const char * str );
char *	strncpy ( char * destination, const char * source, int num );
int		strncmp ( const char * str1, const char * str2, int num );
void	sleep(int seconds);

#endif


