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
void    showInt(int value, int length);
int     countIntDigits(int value);

uint32_t getTicks(void);
uint32_t getTicksAsUser(void);
uint16_t getTOSversion(void);

void showHexByte(uint8_t val);
void showHexWord(uint16_t val);
void showHexDword(uint32_t val);

void logMsg(char* logMsg);
void logMsgProgress(uint32_t current, uint32_t total);

#endif
