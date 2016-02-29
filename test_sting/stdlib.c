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

int memcmp(const void *a, const void *b, int num)
{
	BYTE *aa = (BYTE *) a;
	BYTE *bb = (BYTE *) b;
	int i;
	
	for(i=0; i<num; i++) {				// copy all from src to dst
		if(aa[i] != bb[i]) {
            return 1;                   // difference detected!
        }
	}
	
	return 0;                           // no difference, they are equal
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

char *strcat(char *dest, const char *src) 
{
    int   endOfFirst    = strlen(dest);         // find end of first string
    char *p             = &dest[endOfFirst];    // get pointer to that place
    int   len2          = strlen(src);
    strncpy(p, src, len2);                      // copy the 2nd string to end of first
    p[len2]             = 0;                    // terminate string
    
    return dest;
}

char *strcpy(char *dest, const char *source)
{
    int len = strlen(source);
    memcpy(dest, source, len + 1);
    
    return dest;
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

char *strcpy_switch_rn ( char * destination, const char * source)
{
	int i = 0;
	
	while(1){
        if(source[i] != '\n' && source[i] != '\r') {             // if it's not \r, just copy it
            destination[i] = source[i];
        } else {                            // if it's \r, replace it with space
            if(source[i] == '\n') {
                destination[i] = '\r';
            } else {
                destination[i] = '\n';
            }
        }
		
		if(source[i] == 0) {			// terminating zero found?
			break;
		}
		
		i++;
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

	now = getTicksInSupervisor();			// get current ticks
	until = now + tics;   					// calc value timer must get to

	while(1) {
		now = getTicksInSupervisor();		// get current ticks
		
		if(now >= until) {
			break;
		}
	}
}

void sleepMs(int ms)
{
    DWORD ticks = ms / 5;
    DWORD until = getTicks() + ticks;
    
    while(1) {
        if(getTicks() >= until) {
            break;
        }
    }
}

DWORD getTicks(void)
{
    return Supexec(getTicksInSupervisor);
}

DWORD getTicksInSupervisor(void)
{
	DWORD now;
	
	now = *HZ_200;
	return now;
}

