#include "global.h"

void *  memcpy ( void * destination, const void * source, int num );
void *  memset ( void * ptr, int value, int num );
int     strlen ( const char * str );
char *  strncpy ( char * destination, const char * source, int num );
char *  strcpy( char * destination, const char * source);
char *  strcat( char * destination, const char * source);
int     strcmp(const char * str1, const char * str2);
int     strncmp ( const char * str1, const char * str2, int num );
void    sleep(int seconds);
void    msleep(int ms);
DWORD   getTicks(void);
DWORD getTicksAsUser(void);
void showHexByte(BYTE val);
void showHexWord(WORD val);
void showHexDword(DWORD val);


BYTE getKey(void);
BYTE getKeyIfPossible(void);
BYTE atariKeysToSingleByte(BYTE vkey, BYTE key);
