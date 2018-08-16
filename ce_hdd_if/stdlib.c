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

char *strcpy( char * destination, const char * source)
{
    int len = strlen(source);
    return strncpy(destination, source, len+1);
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

char *strcat( char * destination, const char * source)
{
    int len = strlen(destination);

    strcpy(destination + len, source);

    return destination;
}

int strcmp( const char * str1, const char * str2)
{
    int len1 = strlen(str1);
    int len2 = strlen(str1);
    int len = (len1 > len2) ? len1 : len2;

    return strncmp(str1, str2, len);
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


/*  sleep() and msleep() functions */

static DWORD sleepTics;
static void sleepInSupervisor(void);

void msleepInSuper(int ms)
{
    DWORD fiveMsCount = ms / 5;         // convert mili-seconds to 5-milisecond intervals (because 200 HZ timer has a 5 ms resolution)

    if(fiveMsCount == 0) {              // for less than 5 ms sleep - do at least 5 ms sleep
        fiveMsCount++;
    }

    sleepTics = fiveMsCount;
	sleepInSupervisor();                // wait
}

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

	while(now < until) {
		now = getTicks();					// get current ticks
	}
}

DWORD getTicks(void)
{
	/* return system 200Hz counter value */
	return (*HZ_200);
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

/*--------------------------------------------------*/

void showAppVersion(void)
{
    char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char const *buildDate = __DATE__;
    
    int year = 0, month = 0, day = 0;
    int i;
    for(i=0; i<12; i++) {
        if(strncmp(months[i], buildDate, 3) == 0) {
            month = i + 1;
            break;
        }
    }
    
    day     = getIntFromStr(buildDate + 4, 2);
    year    = getIntFromStr(buildDate + 7, 4);
    
    if(day > 0 && month > 0 && year > 0) {
        showInt(year, 4);
        (void) Cconout('-');
        showInt(month, 2);
        (void) Cconout('-');
        showInt(day, 2);
    } else {
        (void) Cconws("YYYY-MM-DD");
    }
}

/*--------------------------------------------------*/

int getIntFromStr(const char *str, int len)
{
    int i;
    int val = 0;
    
    for(i=0; i<len; i++) {
        int digit;
        
        if(str[i] >= '0' && str[i] <= '9') {
            digit = str[i] - '0';
        } else {
            digit = 0;
        }
    
        val *= 10;
        val += digit;
    }
    
    return val;
}

/*--------------------------------------------------*/

void showIntWithPrepadding(int value, int fullLength, char prepadChar)
{
    int i, padCount;
    int digitsLength = countIntDigits(value);
    
    padCount = fullLength - digitsLength;       // count how many chars we need for pre-padding
    
    for(i=0; i<padCount; i++) {                 // prepad
        Cconout(prepadChar);
    }
    
    showInt(value, digitsLength);               // display the rest
}


void intToString(int value, int length, char *tmp)
{
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

    tmp[length] = 0;                        // terminate string
}
