#ifndef _STDLIB_H_
#define _STDLIB_H_

char * 	strcpy(char *dest, char *src);
void *	memcpy ( void * destination, const void * source, int num );
void *	memset ( void * ptr, int value, int num );
int		strlen ( const char * str );
int		strncmp ( const char * str1, const char * str2, int num );
void	sleep(int seconds);

#endif

