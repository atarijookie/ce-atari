#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include "utils.h"
#include "translated/translatedhelper.h"
#include "translated/gemdos.h"
#include "debug.h"
#include "version.h"
#include "downloader.h"
#include "gpio.h"

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

void Utils::attributesHostToAtari(bool isReadOnly, bool isDir, BYTE &attrAtari)
{
    attrAtari = 0;

    if(isReadOnly)
        attrAtari |= FA_READONLY;

/*
    if(attrHost & FILE_ATTRIBUTE_HIDDEN)
        attrAtari |= FA_HIDDEN;

    if(attrHost & FILE_ATTRIBUTE_SYSTEM)
        attrAtari |= FA_SYSTEM;
		
    if(attrHost &                      )
		attrAtari |= FA_VOLUME;
*/
	
    if(isDir)
        attrAtari |= FA_DIR;

/*
    if(attrHost & FILE_ATTRIBUTE_ARCHIVE)
        attrAtari |= FA_ARCHIVE;
*/		
}

WORD Utils::fileTimeToAtariDate(struct tm *ptm)
{
    WORD atariDate = 0;
	
	if(ptm == NULL) {
		return 0;
	}

    atariDate |= (ptm->tm_year - 80) << 9;            // year (tm_year is 'years since 1900', we want 'years since 1980', so the difference is -80
    atariDate |= (ptm->tm_mon  +  1) << 5;            // month
    atariDate |= (ptm->tm_mday     );                 // day

    return atariDate;
}

WORD Utils::fileTimeToAtariTime(struct tm *ptm)
{
    WORD atariTime = 0;

	if(ptm == NULL) {
		return 0;
	}
	
    atariTime |= (ptm->tm_hour		) << 11;        // hours
    atariTime |= (ptm->tm_min		) << 5;         // minutes
    atariTime |= (ptm->tm_sec	/ 2	);              // seconds

    return atariTime;
}

void Utils::fileDateTimeToHostTime(WORD atariDate, WORD atariTime, struct tm *ptm)
{
    WORD year, month, day;
    WORD hours, minutes, seconds;

    year    = (atariDate >> 9)   + 1980;
    month   = (atariDate >> 5)   & 0x0f;
    day     =  atariDate         & 0x1f;

    hours   =  (atariTime >> 11) & 0x1f;
    minutes =  (atariTime >>  5) & 0x3f;
    seconds = ((atariTime >>  5) & 0x1f) * 2;
	
	ptm->tm_year	= year;
	ptm->tm_mon		= month;
	ptm->tm_mday	= day;
	
	ptm->tm_hour	= hours;
	ptm->tm_min		= minutes;
	ptm->tm_sec		= seconds;
}

void Utils::mergeHostPaths(std::string &dest, std::string &tail)
{
    if(dest.empty()) {      // if the 1st part is empty, then result is just the 2nd part
        dest = tail;
        return;
    }

    if(tail.empty()) {      // if the 2nd part is empty, don't do anything
        return;
    }

    bool endsWithSepar      = (dest[dest.length() - 1] == HOSTPATH_SEPAR_CHAR);
    bool startsWithSepar    = (tail[0] == HOSTPATH_SEPAR_CHAR);

    if(!endsWithSepar && !startsWithSepar){     // both don't have separator char? add it between them
        dest = dest + HOSTPATH_SEPAR_STRING + tail;
        return;
    }

    if(endsWithSepar && startsWithSepar) {      // both have separator char? remove one
        dest[dest.length() - 1] = 0;
        dest = dest + tail;
        return;
    }

    // in this case one of them has separator, so just merge them together
    dest = dest + tail;
}

void Utils::splitFilenameFromPath(std::string &pathAndFile, std::string &path, std::string &file)
{
    int sepPos = pathAndFile.rfind(HOSTPATH_SEPAR_STRING);

    if(sepPos == ((int) std::string::npos)) {                   // not found?
        path.clear();
        file = pathAndFile;                                     // pretend we don't have path, just filename
    } else {                                                    // separator found?
        path    = pathAndFile.substr(0, sepPos + 1);            // path is before separator
        file    = pathAndFile.substr(sepPos + 1);               // file is after separator
    }
}

void Utils::sleepMs(DWORD ms)
{
	DWORD us = ms * 1000;
	
	usleep(us);
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

bool Utils::copyFile(std::string &src, std::string &dst)
{
    FILE *from, *to;

    BYTE bfr64k[64 * 1024];

    from = fopen((char *) src.c_str(), "rb");               // open source file

    if(!from) {
        Debug::out("Utils::onDeviceCopy - failed to open source file %s", (char *) src.c_str());
        return false;
    }

    to = fopen((char *) dst.c_str(), "wb");                 // open destrination file

    if(!to) {
        Debug::out("Utils::onDeviceCopy - failed to open destination file %s", (char *) dst.c_str());
        return false;
    }

    while(1) {                                              // copy the file in loop 
        size_t read = fread(bfr64k, 1, 64 * 1024, from);

        if(read == 0) {                                     // didn't read anything? quit
            break;
        }

        fwrite(bfr64k, 1, read, to);

        if(feof(from)) {                                    // end of file reached? quit
            break;
        }
    }

    fclose(from);
    fclose(to);

    return true;
}


