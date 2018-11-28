#include <mint/sysbind.h>
#include <mint/osbind.h>

#include "acsi.h"
#include "stdlib.h"
#include "keys.h"

void *memcpy ( void * destination, const void * source, int num )
{
    BYTE *dst = (BYTE *) destination;
    BYTE *src = (BYTE *) source;
    int i;
    
    for(i=0; i<num; i++) {              // copy all from src to dst
        dst[i] = src[i];
    }
    
    return destination;
}

void *memset ( void * ptr, int value, int num )
{
    BYTE *p = (BYTE *) ptr;
    int i;
    
    for(i=0; i<num; i++) {              // set all in ptr to value
        p[i] = value;
    }
    
    return ptr;
}

int strlen ( const char * str )
{
    int i;

    for(i=0; i<2048; i++) {             // find first zero and return it's position
        if(str[i] == 0) {
            return i;
        }
    }
    
    return 0;
}

char *strncpy ( char * destination, const char * source, int num )
{
    int i;
    
    for(i=0; i<num; i++) {              // copy max. num chars, but even less when termination zero is found
        destination[i] = source[i];
        
        if(source[i] == 0) {            // terminating zero found?
            break;
        }
    }
    
    return destination;
}

char *strcpy( char * destination, const char * source)
{
    int i;
    
    for(i=0; i<1024; i++) {             
        destination[i] = source[i];
        
        if(source[i] == 0) {            // terminating zero found?
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

int strcmp(const char * str1, const char * str2)
{
    int len1 = strlen(str1);
    int len2 = strlen(str2);

    if(len1 != len2) {      // length not equal? strings different
        return 1;
    }

    // length equal, compare strings
    return strncmp(str1, str2, len1);
}

int strncmp ( const char * str1, const char * str2, int num )
{
    int i;

    for(i=0; i<num; i++) {          
        if(str1[i] == str2[i]) {            // chars matching? continue
            continue;
        }

        if(str1[i] == 0 && str2[i] != 0) {  // 1st string terminated, 2nd string continues? 
            return -1; 
        }
        
        if(str1[i] != 0 && str2[i] == 0) {  // 1st string continues, 2nd string terminated? 
            return 1; 
        }
        
        if(str1[i] > str2[i]) {
            return 1;
        } else {
            return -1;
        }
    }
    
    return 0;                           // if came here, all chars matching
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

DWORD getTicks(void)
{
    /* return system 200Hz counter value */
    return (*HZ_200);
}

static void sleepInSupervisor(void)
{
    DWORD now, until;

    now = getTicks();                       // get current ticks
    until = now + sleepTics;                // calc value timer must get to

    while(now < until) {
        now = getTicks();                   // get current ticks
    }
}

DWORD getTicksAsUser(void)
{
    DWORD res = Supexec(getTicks);
    return res;
}

void showHexByte(BYTE val)
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

void showHexWord(WORD val)
{
    showHexByte(val >> 8);
    showHexByte(val & 0xff);
}

void showHexDword(DWORD val)
{
    showHexByte(val >> 24);
    showHexByte(val >> 16);
    showHexByte(val >>  8);
    showHexByte(val      );
}

BYTE getKey(void)
{
    DWORD scancode;
    BYTE key, vkey;

    scancode = Cnecin();                    /* get char form keyboard, no echo on screen */

    vkey    = (scancode >> 16)  & 0xff;
    key     =  scancode         & 0xff;

    key     = atariKeysToSingleByte(vkey, key); /* transform BYTE pair into single BYTE */
    
    return key;
}

BYTE getKeyIfPossible(void)
{
    DWORD scancode;
    BYTE key, vkey, res;

    res = Cconis();                             // see if there's something waiting from keyboard 

    if(res == 0) {                              // nothing waiting from keyboard?
        return 0;
    }
    
    scancode = Cnecin();                        // get char form keyboard, no echo on screen 

    vkey = (scancode>>16) & 0xff;
    key  =  scancode      & 0xff;

    key = atariKeysToSingleByte(vkey, key);     // transform BYTE pair into single BYTE
    return key;
}

BYTE atariKeysToSingleByte(BYTE vkey, BYTE key)
{
    WORD vkeyKey;
    vkeyKey = (((WORD) vkey) << 8) | ((WORD) key);      /* create a WORD with vkey and key together */

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
