#include <time.h>
#include <unistd.h>

#include "utils.h"
#include "translated/translatedhelper.h"
#include "translated/gemdos.h"

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

    atariDate |= (ptm->tm_year - 1980) << 9;            // year
    atariDate |= (ptm->tm_mon        ) << 5;            // month
    atariDate |= (ptm->tm_mday       );                 // day

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


