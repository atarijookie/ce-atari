#ifndef _STDLIB_CE_HDD_IF_H_
#define _STDLIB_CE_HDD_IF_H_

#include "acsi.h"

void *  memcpy  ( void * destination, const void * source, int num );
int     memcmp  ( const void *a, const void *b, int num );
void *  memset  ( void * ptr, int value, int num );
int     strlen  ( const char * str );
char *  strcpy  ( char * destination, const char * source);
char *  strncpy ( char * destination, const char * source, int num );
char *  strcat  ( char * destination, const char * source);
int     strcmp  ( const char * str1, const char * str2);
int     strncmp ( const char * str1, const char * str2, int num );
void    sleep   ( int seconds );
void    msleep  ( int ms );
void    msleepInSuper(int ms);

DWORD getTicks(void);
DWORD getTicksAsUser(void);

void showInt(int value, int length);

#endif
