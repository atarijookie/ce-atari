#ifndef _STDLIB_H_
#define _STDLIB_H_

#include "global.h"

void *	memcpy              (void * destination, const void * source, int num);
void *	memset              (void * ptr, int value, int num);
int     memcmp              (const void *a, const void *b, int num);
int		strlen              (const char * str);
int     strlen_ex           (const char * str, char terminator);
char *	strncpy             (char * destination, const char * source, int num);
char *  strcpy              (char *dest, const char *source);
char *  strcat              (char *dest, const char *src);
char *  strcpy_switch_rn    (char * destination, const char * source);
int		strncmp             (const char * str1, const char * str2, int num);
void	sleep               (int seconds);
void    sleepMs             (int ms);

DWORD getTicks(void);
DWORD getTicksInSupervisor(void);

#endif
