#include <mint/sysbind.h>
#include <mint/osbind.h>

#include "stdlib.h"

void* memcpy(void* destination, const void* source, int num)
{
    uint8_t *dst = (uint8_t *) destination;
    uint8_t *src = (uint8_t *) source;
    int i;
    
    for(i=0; i<num; i++) {              // copy all from src to dst
        dst[i] = src[i];
    }
    
    return destination;
}

void *memset(void* ptr, int value, int num)
{
    uint8_t *p = (uint8_t *) ptr;
    int i;
    
    for(i=0; i<num; i++) {              // set all in ptr to value
        p[i] = value;
    }
    
    return ptr;
}

int strlen(const char * str)
{
    int i;

    for(i=0; i<2048; i++) {             // find first zero and return it's position
        if(str[i] == 0) {
            return i;
        }
    }
    
    return 0;
}

char *strcpy(char* destination, const char* source)
{
    int len = strlen(source) + 1;
    return strncpy(destination, source, len);
}

char *strncpy(char* destination, const char* source, int num)
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

int strncmp(const char* str1, const char* str2, int num)
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

int sleepSeconds;
static void sleepInSupervisor(void);

void sleep(int seconds)
{
    sleepSeconds = seconds;
    Supexec(sleepInSupervisor);
}

static void sleepInSupervisor(void)
{
    uint32_t now, until;
    uint32_t tics = sleepSeconds * 200;

    now = getTicks();                       // get current ticks
    until = now + tics;                     // calc value timer must get to

    while(1) {
        now = getTicks();                   // get current ticks
        
        if(now >= until) {
            break;
        }
    }
}

uint32_t getTicks(void)
{
    uint32_t now;
    
    now = *HZ_200;
    return now;
}

uint32_t getTicksAsUser(void)
{
    uint32_t res = Supexec(getTicks);
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

uint16_t getWord(uint8_t *bfr)
{
    uint16_t val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get lo

    return val;
}

uint32_t getDword(uint8_t *bfr)
{
    uint32_t val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get mid hi
    val = val << 8;

    val |= bfr[2];      // get mid lo
    val = val << 8;

    val |= bfr[3];      // get lo

    return val;
}

void storeWord(uint8_t *bfr, uint16_t val)
{
    bfr[0] = val >> 8;  // store hi
    bfr[1] = val;       // store lo
}

void storeDword(uint8_t *bfr, uint32_t val)
{
    bfr[0] = val >> 24; // store hi
    bfr[1] = val >> 16; // store mid hi
    bfr[2] = val >>  8; // store mid lo
    bfr[3] = val;       // store lo
}

uint8_t getMachineType(void)
{
    uint32_t *cookieJarAddr    = (uint32_t *) 0x05A0;
    uint32_t *cookieJar        = (uint32_t *) *cookieJarAddr;     // get address of cookie jar

    if(cookieJar == 0) {                        // no cookie jar? it's an old ST
        return MACHINE_ST;
    }

    uint32_t cookieKey, cookieValue;

    while(1) {                                  // go through the list of cookies
        cookieKey   = *cookieJar++;
        cookieValue = *cookieJar++;

        if(cookieKey == 0) {                    // end of cookie list? then cookie not found, it's an ST
            break;
        }

        if(cookieKey == 0x5f4d4348) {           // is it _MCH key?
            uint16_t machineMajor = cookieValue >> 16;
            uint16_t machineMinor = (uint16_t) cookieValue;

            switch(machineMajor) {
                case 0: return MACHINE_ST;

                case 1: {   // major is 1, determine specific machine based on minor
                    if(machineMinor == 0 || machineMinor == 16) {   // 0 and 16 are STE and Mega STE
                        return MACHINE_STE;
                    }
                    break;
                };

                case 2: return MACHINE_TT;
                case 3: return MACHINE_FALCON;
            }

            break;                              // or it's ST
        }
    }

    return MACHINE_ST;                          // it's an ST
}

void showMessage(const char* message, int sleepTime)
{
    (void) Cconws(message);
    sleep(sleepTime);
    (void) Cnecin();
}

// create buffer pointer to even address
uint8_t* addrToEven(uint8_t* addrIn)
{
    uint32_t toEven = (uint32_t) addrIn;

    if(toEven & 1) {        // not even number?
        toEven++;
    }

    return (uint8_t*) toEven; 
}

// create buffer pointer to address with the lowest byte being zero
uint8_t* addrToLowestByteZero(uint8_t* addrIn)
{
    uint32_t toEven = (uint32_t) addrIn;

    if((toEven & 0xff) != 0) {          // 8 lowest bits are not zero?
        toEven = toEven & 0xffffff00;   // clear lowest 8 bits
        toEven += 0x00000100;           // increate the address to next boundary with all zeros lowest byte
    }

    return (uint8_t*) toEven; 
}
