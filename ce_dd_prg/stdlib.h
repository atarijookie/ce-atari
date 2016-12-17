#ifndef _STDLIB_H_
#define _STDLIB_H_

#include "acsi.h"

void *	memcpy  ( void * destination, const void * source, int num );
int     memcmp  ( const void *a, const void *b, int num );
void *	memset  ( void * ptr, int value, int num );
int		strlen  ( const char * str );
char *	strncpy ( char * destination, const char * source, int num );
int		strncmp ( const char * str1, const char * str2, int num );
void	sleep   ( int seconds );
void    msleep  ( int ms );
void    msleepInSuper(int ms);

DWORD getTicks(void);

#endif
