#include "stdlib.h"
#include "acsi.h"

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

void sleep(int seconds)
{
	DWORD now, until;
	DWORD tics = seconds * 200;
	
	now = *HZ_200;
	until = now + tics;   				/* calc value timer must get to */

	while(1) {
		now = *HZ_200;
		
		if(now >= until) {
			break;
		}
	}
}

