#include <mint/sysbind.h>
#include <mint/osbind.h>

#include "stdlib.h"

BYTE showHex_toLogNotScreen;

void *memcpy ( void * destination, const void * source, int num )
{
	BYTE *dst = (BYTE *) destination;
	BYTE *src = (BYTE *) source;
	int i;
	
	for(i=0; i<num; i++) {				// copy all from src to dst
		dst[i] = src[i];
	}
	
	return destination;
}

void *memset ( void * ptr, int value, int num )
{
	BYTE *p = (BYTE *) ptr;
	int i;
	
	for(i=0; i<num; i++) {				// set all in ptr to value
		p[i] = value;
	}
	
	return ptr;
}

int strlen ( const char * str )
{
	int i;

	for(i=0; i<2048; i++) {				// find first zero and return it's position
		if(str[i] == 0) {
			return i;
		}
	}
	
	return 0;
}

char *strcat( char * destination, const char * source)
{
	int len = strlen(destination);

	strcpy(destination + len, source);
	
	return destination;
}

char *strcpy ( char * destination, const char * source)
{
	int i = 0;
	
	while(1){
		destination[i] = source[i];
		
		if(source[i] == 0) {			// terminating zero found?
			break;
		}
		
		i++;
	}
	
	return destination;
}

char *strncpy ( char * destination, const char * source, int num )
{
	int i;
	
	for(i=0; i<num; i++) {				// copy max. num chars, but even less when termination zero is found
		destination[i] = source[i];
		
		if(source[i] == 0) {			// terminating zero found?
			break;
		}
	}
	
	return destination;
}

int strncmp ( const char * str1, const char * str2, int num )
{
	int i;

	for(i=0; i<num; i++) {	
		if(str1[i] == str2[i]) {			// chars matching? continue
			continue;
		}

		if(str1[i] == 0 && str2[i] != 0) {	// 1st string terminated, 2nd string continues? 
			return -1; 
		}
		
		if(str1[i] != 0 && str2[i] == 0) {	// 1st string continues, 2nd string terminated? 
			return 1; 
		}
		
		if(str1[i] > str2[i]) {
			return 1;
		} else {
			return -1;
		}
	}
	
	return 0;							// if came here, all chars matching
}

int strcmp ( const char * str1, const char * str2)
{
    int len1 = strlen(str1);
    int len2 = strlen(str2);
    
    if(len1 != len2) {
        return ((len1 > len2) ? 1 : -1);
    }
    
	int i = 0;
	for(i=0; i<len1; i++) {
		if(str1[i] == str2[i]) {			// chars matching? continue
			continue;
		}

		if(str1[i] == 0 && str2[i] != 0) {	// 1st string terminated, 2nd string continues? 
			return -1; 
		}
		
		if(str1[i] != 0 && str2[i] == 0) {	// 1st string continues, 2nd string terminated? 
			return 1; 
		}
		
		if(str1[i] > str2[i]) {
			return 1;
		} else {
			return -1;
		}
	}
	
	return 0;							// if came here, all chars matching
}

int sleepSeconds;
int sleepMilisecs;
static void sleepInSupervisor(void);
static void sleepMsInSupervisor(void);

void sleep(int seconds)
{
	sleepSeconds = seconds;
	Supexec(sleepInSupervisor);
}

void sleepMs(int ms)
{
	sleepMilisecs = ms;
	Supexec(sleepMsInSupervisor);
}

static void sleepInSupervisor(void)
{
	DWORD now, until;
	DWORD tics = sleepSeconds * 200;

	now = getTicks_fromSupervisor();        // get current ticks
	until = now + tics;                     // calc value timer must get to

	while(1) {
		now = getTicks_fromSupervisor();    // get current ticks
		
		if(now >= until) {
			break;
		}
	}
}

static void sleepMsInSupervisor(void)
{
	DWORD now, until;
	DWORD tics = sleepMilisecs / 5;         // one tick is 5 ms, so /5 will convert ms to ticks

	now = getTicks_fromSupervisor();        // get current ticks
	until = now + tics;   					// calc value timer must get to

	while(1) {
		now = getTicks_fromSupervisor();    // get current ticks
		
		if(now >= until) {
			break;
		}
	}
}

DWORD getTicks(void)
{
    DWORD res;
    res = Supexec(getTicks_fromSupervisor);

    return res;
}

DWORD getTicks_fromSupervisor(void)
{
	DWORD now;
	
	now = *HZ_200;
	return now;
}

BYTE *storeByte(BYTE *bfr, BYTE value)
{
    *bfr = (BYTE) value;                // store byte
    bfr++;                              // advance to next byte
    
    return bfr;                         // return the updated buffer address
}

BYTE *storeWord(BYTE *bfr, WORD value)
{
    *bfr = (BYTE) (value >> 8);         // store higher part
    bfr++;                              // advance to next byte

    *bfr = (BYTE) (value);              // store lower part
    bfr++;                              // advance to next byte
    
    return bfr;                         // return the updated buffer address
}

BYTE *storeDword(BYTE *bfr, DWORD value)
{
    *bfr = (BYTE) (value >> 24);        // store highest part
    bfr++;                              // advance to next byte

    *bfr = (BYTE) (value >> 16);        // store mid hi part
    bfr++;                              // advance to next byte

    *bfr = (BYTE) (value >>  8);        // store mid low part
    bfr++;                              // advance to next byte

    *bfr = (BYTE) (value);              // store lowest part
    bfr++;                              // advance to next byte
    
    return bfr;                         // return the updated buffer address
}

WORD getWord(BYTE *bfr)
{
    WORD val;
    
    val = ((WORD) bfr[0]) << 8;         // get upper part
    val = val | ((WORD) bfr[1]);        // get lower part
    
    return val;
}

DWORD getDword(BYTE *bfr)
{
    DWORD val;
    
    val =       ((DWORD) bfr[0]) << 24;
    val = val | ((DWORD) bfr[1]) << 16;
    val = val | ((DWORD) bfr[2]) <<  8;
    val = val | ((DWORD) bfr[3]);
    
    return val;
}

WORD getWordByByteOffset(void *base, int ofs)
{
    BYTE *pByte     = (BYTE *) base;
    WORD *pWord     = (WORD *) (pByte + ofs);
    WORD val        = *pWord;
    return val;
}

DWORD getDwordByByteOffset(void *base, int ofs)
{
    BYTE  *pByte    = (BYTE  *)  base;
    DWORD *pDword   = (DWORD *) (pByte + ofs);
    DWORD val       = *pDword;     
    return val;
}

void *getVoidpByByteOffset(void *base, int ofs)
{
    void *p = (void *) getDwordByByteOffset(base, ofs);
    return p;
}


void setWordByByteOffset(void *base, int ofs, WORD val)
{
    BYTE *pByte  = (BYTE *)  base;
    WORD *pWord     = (WORD *) (pByte + ofs);
    *pWord          = val;
}

void setDwordByByteOffset(void *base, int ofs, DWORD val)
{
    BYTE *pByte     = (BYTE *) base;
    DWORD *pDword   = (DWORD *) (pByte + ofs);
    *pDword         = val;
}

//-------------------
#ifdef DEBUG_STRING
void logStr(char *str)
{
//    (void) Cconws(str);

    int len; 
    int f = Fopen("C:\\ce_sting.log", 1);       // open file for writing
    
    if(f < 0) {                                 // if failed to open existing file
        f = Fcreate("C:\\ce_sting.log", 0);     // create file
        
        if(f < 0) {                             // if failed to create new file
            return;
        }
    }
    
    len = strlen(str);                          // get length

    Fseek(0, f, 2);                             // move to the end of file
    Fwrite(f, len, str);                        // write it to file
    Fclose(f);                                  // close it
}

void logBfr(BYTE *bfr, int len)
{
    int i;

    logStr("\n");
    
    for(i=0; i<len; i++) {
        showHexByte(bfr[i]);
        logStr(" ");
    }

    logStr("\n");
}
#endif    
//-------------------