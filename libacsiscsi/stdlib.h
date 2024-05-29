#ifndef _STDLIB_H_
#define _STDLIB_H_

#include "acsi.h"

#define MACHINE_ST      0
#define MACHINE_TT      2
#define MACHINE_FALCON  3

void *	memcpy  ( void * destination, const void * source, int num );
int     memcmp  ( const void *a, const void *b, int num );
void *	memset  ( void * ptr, int value, int num );
int		strlen  ( const char * str );
char *  strcat  ( char * destination, const char * source);
char *	strncpy ( char * destination, const char * source, int num );
char *  strcpy  ( char * destination, const char * source);
int		strncmp ( const char * str1, const char * str2, int num );
int     strcmp  ( const char * str1, const char * str2);
void	sleep   ( int seconds );
void    msleep  ( int ms );
void    msleepInSuper(int ms);
void    showInt(int value, int length);
int     countIntDigits(int value);

uint32_t getTicks(void);
uint32_t getTicksAsUser(void);
uint16_t getTOSversion(void);
uint8_t getMachineType(void);

void showHexBytes(uint8_t *bfr, int cnt);
void showHexByte(uint8_t val);
void showHexWord(uint16_t val);
void showHexDword(uint32_t val);

void logMsgHexByte(uint8_t val);
void logMsg(char* logMsg);
void logMsgProgress(uint32_t current, uint32_t total);

uint8_t getKey(void);
uint8_t getKeyIfPossible(void);
uint8_t atariKeysToSingleByte(uint8_t vkey, uint8_t key);

#endif
