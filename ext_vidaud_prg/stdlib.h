#ifndef _STDLIB_H_
#define _STDLIB_H_

#include <stdint.h>

#define Clear_home()    (void) Cconws("\33E")
#define Cursor_on()     (void) Cconws("\33e")

#include <stdint.h>

#ifndef FALSE
    #define FALSE       0
    #define TRUE        1
#endif

#define HZ_200     ((volatile uint32_t *) 0x04BA) /* 200 Hz system clock */ 

void *  memcpy ( void * destination, const void * source, int num );
void *  memset ( void * ptr, int value, int num );
int     strlen ( const char * str );
char *  strcpy ( char * destination, const char * source);
char *  strncpy( char * destination, const char * source, int num );
int     strncmp( const char * str1, const char * str2, int num );
void    sleep  (int seconds);
void    showInt(int value, int length);

uint32_t getTicks(void);
uint32_t getTicksAsUser(void);

uint16_t getWord(uint8_t *bfr);
uint32_t getDword(uint8_t *bfr);
void storeWord(uint8_t *bfr, uint16_t val);
void storeDword(uint8_t *bfr, uint32_t val);

#endif
