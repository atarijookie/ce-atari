#ifndef _STDLIB_H_
#define _STDLIB_H_

#include "global.h"

void *	memcpy ( void * destination, const void * source, int num );
void *	memset ( void * ptr, int value, int num );
int		strlen ( const char * str );
char *	strncpy ( char * destination, const char * source, int num );
char *  strcpy_switch_rn ( char * destination, const char * source);
int		strncmp ( const char * str1, const char * str2, int num );
void	sleep(int seconds);

DWORD getTicks(void);

#endif
