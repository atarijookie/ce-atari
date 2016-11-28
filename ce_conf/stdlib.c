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

int sleepSeconds;
static void sleepInSupervisor(void);

void sleep(int seconds)
{
	sleepSeconds = seconds;
	Supexec(sleepInSupervisor);
}

static void sleepInSupervisor(void)
{
	DWORD now, until;
	DWORD tics = sleepSeconds * 200;

	now = getTicks();						// get current ticks
	until = now + tics;   					// calc value timer must get to

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

DWORD getTicksAsUser(void)
{
    DWORD res = Supexec(getTicks);
    return res;
}

int countIntDigits(int value)
{
    int i, div = 10;

    for(i=1; i<6; i++) {                // try from 10 to 1000000
        if((value / div) == 0) {        // after division the result is zero? we got the length
            return i;
        }

        div = div * 10;                 // increase the divisor by 10
    }

    return 6;
}

void showInt(int value, int length)
{
    char tmp[10];
    memset(tmp, 0, 10);

    //--------------------------------------
    // determine length?
    if(length == -1) {
        length = countIntDigits(value);
    }
    //--------------------------------------
    // check if it will fit in the displayed lenght
    int bigDiv = 1;
    int i;
    for(i=0; i<length; i++) {               // create the big divider, which will determine if the number will fit in the length or not
        bigDiv *= 10;
    }
    
    if((value / bigDiv) != 0) {             // if this value won't fit in the specified length
        value = 999999;
    }
    //--------------------------------------
    // show the digits
    for(i=0; i<length; i++) {               // go through the int lenght and get the digits
        int val, mod;

        val = value / 10;
        mod = value % 10;

        tmp[length - 1 - i] = mod + 48;     // store the current digit

        value = val;
    }

    (void) Cconws(tmp);                     // write it out
}

