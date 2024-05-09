#include <mint/sysbind.h>
#include <mint/osbind.h>

#include "stdlib.h"
#include "keys.h"

void *memcpy ( void * destination, const void * source, int num )
{
	uint8_t *dst = (uint8_t *) destination;
	uint8_t *src = (uint8_t *) source;
	int i;

	for(i=0; i<num; i++) {				// copy all from src to dst
		dst[i] = src[i];
	}

	return destination;
}

int memcmp(const void *a, const void *b, int num)
{
	uint8_t *aa = (uint8_t *) a;
	uint8_t *bb = (uint8_t *) b;
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
	uint8_t *p = (uint8_t *) ptr;
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

char *strcpy ( char * destination, const char * source)
{
    int len = strlen(source);
    return strncpy (destination, source, len);
}

int strcmp(const char * str1, const char * str2)
{
    int len1 = strlen(str1);
    int len2 = strlen(str2);

    if(len1 != len2) {  // length is different, strings are different
        return 1;
    }

    return strncmp(str1, str2, len1);
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

static uint32_t sleepTics;
static void sleepInSupervisor(void);

void msleepInSuper(int ms)
{
    uint32_t fiveMsCount = ms / 5;         // convert mili-seconds to 5-milisecond intervals (because 200 HZ timer has a 5 ms resolution)

    if(fiveMsCount == 0) {              // for less than 5 ms sleep - do at least 5 ms sleep
        fiveMsCount++;
    }

    sleepTics = fiveMsCount;
	sleepInSupervisor();                // wait
}

void msleep(int ms)
{
    uint32_t fiveMsCount = ms / 5;         // convert mili-seconds to 5-milisecond intervals (because 200 HZ timer has a 5 ms resolution)

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
	uint32_t now, until;

	now = getTicks();						// get current ticks
	until = now + sleepTics;    			// calc value timer must get to

	while(now < until) {
		now = getTicks();					// get current ticks
	}
}

uint32_t getTicks(void)
{
	/* return system 200Hz counter value */
	return (*HZ_200);
}

uint32_t getTicksAsUser(void)
{
	return Supexec(getTicks);
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

uint16_t getTOSversionInSupervisor(void)
{
    // detect TOS version and try to automatically choose the interface
    uint8_t  *pSysBase     = (uint8_t *) 0x000004F2;
    uint8_t  *ppSysBase    = (uint8_t *)  ((uint32_t )  *pSysBase);                      // get pointer to TOS address
    uint16_t  tosVersion    = (uint16_t  ) *(( uint16_t *) (ppSysBase + 2));                // TOS +2: TOS version
    return tosVersion;
}

uint16_t getTOSversion(void)
{
    uint8_t mode = Super(1);    // this interrogates the current mode of the processor

    // If the processor is in user mode a SUP_USER (0) is returned,
    // otherwise a SUP_SUPER (1) is returned.

    if(mode == 0) { // user mode a SUP_USER (0), call with Supexec()
        return Supexec(getTOSversionInSupervisor);
    }

    // SUP_SUPER (1) is returned, running in supervisor, just call the function
    return getTOSversionInSupervisor();
}

void showHexByte(uint8_t val)
{
    int hi, lo;
    char tmp[3];
    char table[16] = {"0123456789ABCDEF"};
    
    hi = (val >> 4) & 0x0f;;
    lo = (val     ) & 0x0f;

    tmp[0] = table[hi];
    tmp[1] = table[lo];
    tmp[2] = 0;
    
    (void) Cconws(tmp);
}

void showHexWord(uint16_t val)
{
    uint8_t a,b;
    a = val >>  8;
    b = val;
    
    showHexByte(a);
    showHexByte(b);
}

void showHexDword(uint32_t val)
{
    uint8_t a,b,c,d;
    a = val >> 24;
    b = val >> 16;
    c = val >>  8;
    d = val;
    
    showHexByte(a);
    showHexByte(b);
    showHexByte(c);
    showHexByte(d);
}

void logMsg(char* logMsg)
{
//    if(showLogs) {
//        (void) Cconws(logMsg);
//    }
}

void logMsgProgress(uint32_t current, uint32_t total)
{
//    if(!showLogs) {
//        return;
//    }

//    (void) Cconws("Progress: ");
//    showHexDword(current);
//    (void) Cconws(" out of ");
//    showHexDword(total);
//    (void) Cconws("\n\r");
}

uint8_t getMachineType(void)
{
    uint32_t *cookieJarAddr    = (uint32_t *) 0x05A0;
    uint32_t *cookieJar        = (uint32_t *) *cookieJarAddr;     // get address of cookie jar

    if(cookieJar == 0) {                        // no cookie jar? it's an old ST
        return MACHINE_ST;
    }

    uint32_t cookieKey, cookieValue;

    int i;
    for(i=0; i<64; i++) {                       // go through the list of cookies
        cookieKey   = *cookieJar++;
        cookieValue = *cookieJar++;

        if(cookieKey == 0) {                    // end of cookie list? then cookie not found, it's an ST
            break;
        }

        if(cookieKey == 0x5f4d4348) {           // is it _MCH key?
            uint16_t machine = cookieValue >> 16;

            switch(machine) {                   // depending on machine, either it's TT or FALCON
                case 2: return MACHINE_TT;
                case 3: return MACHINE_FALCON;
            }

            break;                              // or it's ST
        }
    }

    return MACHINE_ST;                          // it's an ST
}
//--------------------------------------------------
char *strcat( char * destination, const char * source)
{
    int len = strlen(destination);
    strcpy(destination + len, source);
    return destination;
}

uint8_t getKey(void)
{
    uint32_t scancode;
    uint8_t key, vkey;

    scancode = Cnecin();                    /* get char form keyboard, no echo on screen */

    vkey    = (scancode >> 16)  & 0xff;
    key     =  scancode         & 0xff;

    key     = atariKeysToSingleByte(vkey, key); /* transform uint8_t pair into single uint8_t */
    
    return key;
}

uint8_t getKeyIfPossible(void)
{
    uint32_t scancode;
    uint8_t key, vkey, res;

    res = Cconis();                             // see if there's something waiting from keyboard 

    if(res == 0) {                              // nothing waiting from keyboard?
        return 0;
    }
    
    scancode = Cnecin();                        // get char form keyboard, no echo on screen 

    vkey = (scancode>>16) & 0xff;
    key  =  scancode      & 0xff;

    key = atariKeysToSingleByte(vkey, key);     // transform uint8_t pair into single uint8_t
    return key;
}

uint8_t atariKeysToSingleByte(uint8_t vkey, uint8_t key)
{
    uint16_t vkeyKey;
    vkeyKey = (((uint16_t) vkey) << 8) | ((uint16_t) key);      /* create a uint16_t with vkey and key together */

    switch(vkeyKey) {
        case 0x5032: return KEY_PAGEDOWN;
        case 0x4838: return KEY_PAGEUP;
    }

    if(key >= 32 && key < 127) {        /* printable ASCII key? just return it */
        return key;
    }

    if(key == 0) {                      /* will this be some non-ASCII key? convert it */
        switch(vkey) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4b: return KEY_LEFT;
            case 0x4d: return KEY_RIGHT;
            case 0x52: return KEY_INSERT;
            case 0x47: return KEY_HOME;
            case 0x62: return KEY_HELP;
            case 0x61: return KEY_UNDO;
            case 0x3b: return KEY_F1;
            case 0x3c: return KEY_F2;
            case 0x3d: return KEY_F3;
            case 0x3e: return KEY_F4;
            case 0x3f: return KEY_F5;
            case 0x40: return KEY_F6;
            case 0x41: return KEY_F7;
            case 0x42: return KEY_F8;
            case 0x43: return KEY_F9;
            case 0x44: return KEY_F10;
            default: return 0;          /* unknown key */
        }
    }

    switch(vkeyKey) {                   /* some other no-ASCII key, but check with vkey too */
        case 0x011b: return KEY_ESC;
        case 0x537f: return KEY_DELETE;
        case 0x0e08: return KEY_BACKSP;
        case 0x0f09: return KEY_TAB;
        case 0x1c0d: return KEY_ENTER;
        case 0x720d: return KEY_ENTER;
    }

    return 0;                           /* unknown key */
}

void showHexBytes(uint8_t *bfr, int cnt)
{
    int i;
    for(i=0; i<cnt; i++) {
        showHexByte(bfr[i]);
    }
}

