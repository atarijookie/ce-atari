#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

//--------
// following includes are here for the code for getting IP address of interfaces
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
//--------

#include "utils.h"
#include "debug.h"
#include "version.h"
#include "gpio.h"
#include "settings.h"

DWORD Utils::getCurrentMs(void)
{
	struct timespec tp;
	int res;
	
	res = clock_gettime(CLOCK_MONOTONIC, &tp);					// get current time
	
	if(res != 0) {												// if failed, fail
		return 0;
	}
	
	DWORD val = (tp.tv_sec * 1000) + (tp.tv_nsec / 1000000);	// convert to milli seconds
	return val;
}

DWORD Utils::getEndTime(DWORD offsetFromNow)
{
	DWORD val;
	
	val = getCurrentMs() + offsetFromNow;
	
	return val;
}

void Utils::sleepMs(DWORD ms)
{
	DWORD us = ms * 1000;
	
	usleep(us);
}

void Utils::franzToResetState(void)
{
	bcm2835_gpio_write(PIN_RESET_FRANZ,			LOW);
}

void Utils::resetHansAndFranz(void)
{
	bcm2835_gpio_write(PIN_RESET_HANS,			LOW);		// reset lines to RESET state
	bcm2835_gpio_write(PIN_RESET_FRANZ,			LOW);

	Utils::sleepMs(10);										// wait a while to let the reset work
	
	bcm2835_gpio_write(PIN_RESET_HANS,			HIGH);		// reset lines to RUN (not reset) state
	bcm2835_gpio_write(PIN_RESET_FRANZ,			HIGH);
	
	Utils::sleepMs(50);										// wait a while to let the devices boot
}

void Utils::resetHans(void)
{
	bcm2835_gpio_write(PIN_RESET_HANS,			LOW);		// reset lines to RESET state
	Utils::sleepMs(10);										// wait a while to let the reset work
	bcm2835_gpio_write(PIN_RESET_HANS,			HIGH);		// reset lines to RUN (not reset) state
	Utils::sleepMs(50);										// wait a while to let the devices boot
}

void Utils::resetFranz(void)
{
	bcm2835_gpio_write(PIN_RESET_FRANZ,			LOW);
	Utils::sleepMs(10);										// wait a while to let the reset work
	bcm2835_gpio_write(PIN_RESET_FRANZ,			HIGH);
	Utils::sleepMs(50);										// wait a while to let the devices boot
}

void Utils::SWAPWORD(WORD &w)
{
    WORD a,b;

    a = w >> 8;         // get top
    b = w  & 0xff;      // get bottom

    w = (b << 8) | a;   // store swapped
}

WORD Utils::SWAPWORD2(WORD w)
{
    WORD a,b;

    a = w >> 8;         // get top
    b = w  & 0xff;      // get bottom

    w = (b << 8) | a;   // store swapped
    return w;
}

WORD Utils::getWord(BYTE *bfr)
{
    WORD val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get lo

    return val;
}

DWORD Utils::getDword(BYTE *bfr)
{
    DWORD val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get mid hi
    val = val << 8;

    val |= bfr[2];      // get mid lo
    val = val << 8;

    val |= bfr[3];      // get lo

    return val;
}

DWORD Utils::get24bits(BYTE *bfr)
{
    DWORD val = 0;

    val  = bfr[0];       // get hi
    val  = val << 8;

    val |= bfr[1];      // get mid
    val  = val << 8;

    val |= bfr[2];      // get lo

    return val;
}

void Utils::storeWord(BYTE *bfr, WORD val)
{
    bfr[0] = val >> 8;  // store hi
    bfr[1] = val;       // store lo
}

void Utils::storeDword(BYTE *bfr, DWORD val)
{
    bfr[0] = val >> 24; // store hi
    bfr[1] = val >> 16; // store mid hi
    bfr[2] = val >>  8; // store mid lo
    bfr[3] = val;       // store lo
}

