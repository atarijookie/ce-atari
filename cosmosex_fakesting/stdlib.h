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
void    sleepMs(int ms);

DWORD getTicks_fromSupervisor(void);
DWORD getTicks(void);

BYTE *storeByte(BYTE *bfr, BYTE value);
BYTE *storeWord(BYTE *bfr, WORD value);
BYTE *storeDword(BYTE *bfr, DWORD value);

WORD  getWord(BYTE *bfr);
DWORD getDword(BYTE *bfr);

void showHexByte(int val);
void showHexWord(WORD val);
void showHexDword(DWORD val);

WORD   getWordByByteOffset (void *base, int ofs);
DWORD  getDwordByByteOffset(void *base, int ofs);
void  *getVoidpByByteOffset(void *base, int ofs);
void   setWordByByteOffset (void *base, int ofs, WORD val);
void   setDwordByByteOffset(void *base, int ofs, DWORD val);

void logStr(char *str);

#endif
