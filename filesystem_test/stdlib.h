#ifndef _STDLIB_H_
#define _STDLIB_H_

#ifndef FALSE
    #define FALSE		0
    #define TRUE		1
#endif

#include <stdint.h>

#ifndef BYTE
    #define BYTE  	unsigned char
    #define WORD  	uint16_t
    #define DWORD 	uint32_t
#endif

#define HZ_200     ((volatile DWORD *) 0x04BA) // 200 Hz system clock  

void *	memcpy ( void * destination, const void * source, int num );
void *	memset ( void * ptr, int value, int num );
int		strlen ( const char * str );
char *  strcpy ( char * destination, const char * source);
char *	strcat( char * destination, const char * source);
char *	strncpy ( char * destination, const char * source, int num );
int		strncmp ( const char * str1, const char * str2, int num );
int		strcmp ( const char * str1, const char * str2);
int		strcmpi( const char * str1, const char * str2);
void	sleep(int seconds);
void    sleepMs(int ms);

void toUpper(const char *src, char *dst);

DWORD getTicks_fromSupervisor(void);
DWORD getTicks(void);

BYTE *storeByte(BYTE *bfr, BYTE value);
BYTE *storeWord(BYTE *bfr, WORD value);
BYTE *storeDword(BYTE *bfr, DWORD value);

WORD  getWord(BYTE *bfr);
DWORD getDword(BYTE *bfr);

WORD   getWordByByteOffset (void *base, int ofs);
DWORD  getDwordByByteOffset(void *base, int ofs);
void  *getVoidpByByteOffset(void *base, int ofs);
void   setWordByByteOffset (void *base, int ofs, WORD val);
void   setDwordByByteOffset(void *base, int ofs, DWORD val);

    #ifdef DEBUG_STRING
    void logStr(char *str);
    void logBfr(BYTE *bfr, int len);
    #endif

#endif
