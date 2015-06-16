#include <mint/sysbind.h>
#include <mint/osbind.h>

#include "stdlib.h"

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

DWORD sleepTics;
static void sleepInSupervisor(void);

void msleep(int ms)
{
    DWORD fiveMsCount = ms / 5;         // convert mili-seconds to 5-milisecond intervals (because 200 HZ timer has a 5 ms resolution)
    
    if(fiveMsCount == 0) {              // for less than 5 ms sleep - do at least 5 ms sleep
        fiveMsCount++;
    }
    
    sleepTics = fiveMsCount;
	Supexec(sleepInSupervisor);         // wait
}

void sleep(int seconds)
{
    sleepTics = seconds * 200; 
	Supexec(sleepInSupervisor);
}

static void sleepInSupervisor(void)
{
	DWORD now, until;

	now = getTicks();						// get current ticks
	until = now + sleepTics;    			// calc value timer must get to

	while(1) {
		now = getTicks();					// get current ticks
		
		if(now >= until) {
			break;
		}
	}
}

DWORD getTicks(void)
{
	DWORD now;
	
	now = *HZ_200;
	return now;
}

