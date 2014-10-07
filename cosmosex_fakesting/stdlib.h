#ifndef _STDLIB_H_
#define _STDLIB_H_

#include "acsi.h"

void *	memcpy ( void * destination, const void * source, int num );
void *	memset ( void * ptr, int value, int num );
int		strlen ( const char * str );
char *  strcpy ( char * destination, const char * source);
char *	strcat( char * destination, const char * source);
char *	strncpy ( char * destination, const char * source, int num );
int		strncmp ( const char * str1, const char * str2, int num );
int		strcmp ( const char * str1, const char * str2);
void	sleep(int seconds);
char	toupper(char a);


DWORD getTicks(void);

#endif
