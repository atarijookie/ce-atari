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
#include "translated/translatedhelper.h"
#include "translated/gemdos.h"
#include "debug.h"
#include "version.h"
#include "downloader.h"
#include "gpio.h"
#include "mounter.h"
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

    from = fopen((char *) src.c_str(), "rb");               // open source file

    if(!from) {
        Debug::out(LOG_ERROR, "Utils::copyFile - failed to open source file %s", (char *) src.c_str());
        return false;
    }

    to = fopen((char *) dst.c_str(), "wb");                 // open destrination file

    if(!to) {
        fclose(from);

        Debug::out(LOG_ERROR, "Utils::copyFile - failed to open destination file %s", (char *) dst.c_str());
        return false;
    }

    bool res = copyFileByHandles(from, to);

    fclose(from);
    fclose(to);

    return res;
}

bool Utils::copyFile(FILE *from, std::string &dst)
{
    FILE *to;

    to = fopen((char *) dst.c_str(), "wb");                 // open destrination file

    if(!to) {
        Debug::out(LOG_ERROR, "Utils::copyFile - failed to open destination file %s", (char *) dst.c_str());
        return false;
    }

    bool res = copyFileByHandles(from, to);

    fclose(to);

    return res;
}

bool Utils::copyFileByHandles(FILE *from, FILE *to)
{
    BYTE bfr64k[64 * 1024];

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

    return true;
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

void Utils::getIpAdds(BYTE *bfr)
{
    struct ifaddrs *ifaddr, *ifa;
    int family, n;

    memset(bfr, 0, 10);                                         // set to 0 - this means 'not present'

    if (getifaddrs(&ifaddr) == -1) {
        return;
    }

    // Walk through linked list, maintaining head pointer so we can free list later
    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if (ifa->ifa_addr == NULL)
            continue;

        family = ifa->ifa_addr->sa_family;

        if (family != AF_INET) {                                // not good interface? skip it
            continue;
        }

        sockaddr_in *sai    = (sockaddr_in *) ifa->ifa_addr;
        DWORD ip            = sai->sin_addr.s_addr;

        BYTE *p = NULL;
        if(strcmp(ifa->ifa_name,"eth0")==0) {                   // for eth0 - store at offset 0
            p = bfr;
        }

        if(strcmp(ifa->ifa_name,"wlan0")==0) {                  // for wlan0 - store at offset 5
            p = bfr + 5;
        }

        if(p == NULL) {                                         // if not an interface we're interested in, skip it
            continue;
        }

        p[0] = 1;                                               // enabled?
        p[1] = (BYTE)  ip;                                      // store the ip
        p[2] = (BYTE) (ip >>  8);
        p[3] = (BYTE) (ip >> 16);
        p[4] = (BYTE) (ip >> 24);
    }

    freeifaddrs(ifaddr);
}

void Utils::forceSync(void)
{
	TMounterRequest tmr;			
	tmr.action	= MOUNTER_ACTION_SYNC;                          // let the mounter thread do filesystem caches sync 						
	mountAdd(tmr);
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

void Utils::createTimezoneString(char *str)
{
    Settings  s;
    float     utcOffset;
    utcOffset = s.getFloat((char *) "TIME_UTC_OFFSET", 0);          // read UTC offset from settings

    char  signChar;
    int   ofsHours, ofsMinutes;
    float fOfsHours;
    
    if(utcOffset >= 0.0f) {         // is UTC offset positive? (e.g. Berlin is +1 / +2) TZ should have '-' because it's east of Prime Meridian
        signChar = '-';
    } else {                        // is UTC offset negative? (e.g. New York is -5 / -4) TZ should have '+' because it's west of Prime Meridian
        signChar = '+';
    }
    
    ofsHours    = (int)   utcOffset;                            // int  : hours only    (including sign)
    fOfsHours   = (float) ofsHours;                             // float: hours only    (including sign)
    ofsHours    = abs(ofsHours);                                // int  : hours only    (without   sign)
    ofsMinutes  = abs((int) ((utcOffset - fOfsHours) * 60.0f)); // not get only minutes (without   sign)
    
    if(ofsHours != 0 && ofsMinutes != 0) {                      // got offset - both hours and minutes?
        sprintf(str, "UTC%c%02d:%02d", signChar, ofsHours, ofsMinutes);
    } else if(ofsHours != 0 && ofsMinutes == 0) {               // got offset - only hours (no minutes)
        sprintf(str, "UTC%c%02d", signChar, ofsHours);
    } else {                                                    // no offset?
        sprintf(str, "UTC");
    }
}

void Utils::setTimezoneVariable_inProfileScript(void)
{
    char utcOfsset[64];
    createTimezoneString(utcOfsset);
    
    char tzString[128];
    sprintf(tzString, "echo 'export TZ=\"%s\"' > /etc/profile.d/set_timezone.sh", utcOfsset);
    
    Debug::out(LOG_DEBUG, "Utils::setTimezoneVariable_inProfileScript() -- creating timezone setting script like this: %s\n", tzString);
    
    system("mkdir -p /etc/profile.d");                          // if this dir doesn't exist, create it
    system(tzString);                                           // now create the script in the dir above
    system("chmod 755 /etc/profile.d/set_timezone.sh");         // make it executable
    
    forceSync();                                                // make sure it does to disk
}

void Utils::setTimezoneVariable_inThisContext(void)
{
    char utcOfsset[64];
    createTimezoneString(utcOfsset);

    Debug::out(LOG_DEBUG, "Utils::setTimezoneVariable_inThisContext() -- setting TZ variable to: %s\n", utcOfsset);
    
    setenv("TZ", utcOfsset, 1);
}
