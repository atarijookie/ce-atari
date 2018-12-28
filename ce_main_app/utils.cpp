#include "utils.h"

#include <libgen.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

//--------
// following includes are here for the code for getting IP address of interfaces
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
//--------

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

    year    = (atariDate >> 9)   + 1980; // 0-119 with 0=1980
    month   = (atariDate >> 5)   & 0x0f; // 1-12
    day     =  atariDate         & 0x1f; // 1-31

    hours   =  (atariTime >> 11) & 0x1f;	// 0-23
    minutes =  (atariTime >>  5) & 0x3f;	// 0-59
    seconds = ( atariTime        & 0x1f) * 2;	// in unit of two

	memset(ptm, 0, sizeof(struct tm));
	ptm->tm_year	= year - 1900;		// number of years since 1900.
	ptm->tm_mon		= month - 1;	// The number of months since January, in the range 0 to 11
	ptm->tm_mday	= day;		// The day of the month, in the range 1 to 31.

	ptm->tm_hour	= hours;	// The number of hours past midnight, in the range 0 to 23
	ptm->tm_min		= minutes;	// The number of minutes after the hour, in the range 0 to 59
	ptm->tm_sec		= seconds;	// The number of seconds after the minute, normally in the range 0 to 59,
								//  but can be up to 60 to allow for leap seconds.
}

void Utils::mergeHostPaths(std::string &dest, const std::string &tail)
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

void Utils::splitFilenameFromPath(const std::string &pathAndFile, std::string &path, std::string &file)
{
    size_t sepPos = pathAndFile.rfind(HOSTPATH_SEPAR_STRING);

    if(sepPos == std::string::npos) {                   // not found?
        path.clear();
        file = pathAndFile;                                     // pretend we don't have path, just filename
    } else {                                                    // separator found?
        path    = pathAndFile.substr(0, sepPos + 1);            // path is before separator
        file    = pathAndFile.substr(sepPos + 1);               // file is after separator
    }
}

void Utils::splitFilenameFromExt(const std::string &filenameAndExt, std::string &filename, std::string &ext)
{
    size_t sepPos = filenameAndExt.rfind('.');

    if(sepPos == std::string::npos) {                   		// not found?
        filename = filenameAndExt;                              // pretend we don't have extension, just filename
        ext.clear();
    } else {                                                    // separator found?
        filename = filenameAndExt.substr(0, sepPos);            // filename is before separator
        ext      = filenameAndExt.substr(sepPos + 1);           // extension is after separator
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

bool Utils::copyFile(std::string &src, std::string &dst)
{
    FILE *from, *to;

    from = fopen(src.c_str(), "rb");               // open source file

    if(!from) {
        Debug::out(LOG_ERROR, "Utils::copyFile - failed to open source file %s", src.c_str());
        return false;
    }

    to = fopen(dst.c_str(), "wb");                 // open destrination file

    if(!to) {
        fclose(from);

        Debug::out(LOG_ERROR, "Utils::copyFile - failed to open destination file %s", dst.c_str());
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

    to = fopen(dst.c_str(), "wb");                 // open destrination file

    if(!to) {
        Debug::out(LOG_ERROR, "Utils::copyFile - failed to open destination file %s", dst.c_str());
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

void Utils::getIpAdds(BYTE *bfrIPs, BYTE *bfrMasks)
{
    struct ifaddrs *ifaddr, *ifa;
    int family, n;

    memset(bfrIPs, 0, 10);                                          // set to 0 - this means 'not present'

    if(bfrMasks) {
        memset(bfrMasks, 0, 10);                                    // set to 0 - this means 'not present'
    }

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

        sockaddr_in *saiIp  = (sockaddr_in *) ifa->ifa_addr;
        sockaddr_in *saiMsk = (sockaddr_in *) ifa->ifa_netmask;

        DWORD ip            = saiIp->sin_addr.s_addr;
        DWORD mask          = saiMsk->sin_addr.s_addr;

        BYTE *pIp   = NULL;
        BYTE *pMsk  = NULL;

        if(strcmp(ifa->ifa_name,"eth0")==0) {                   // for eth0 - store at offset 0
            pIp = bfrIPs;

            if(bfrMasks) {                                      // got masks buffer? store offset 0
                pMsk = bfrMasks;
            }
        }

        if(strcmp(ifa->ifa_name,"wlan0")==0) {                  // for wlan0 - store at offset 5
            pIp = bfrIPs + 5;

            if(bfrMasks) {                                      // got masks buffer? store offset 5
                pMsk = bfrMasks + 5;
            }
        }

        if(pIp == NULL) {                                       // if not an interface we're interested in, skip it
            continue;
        }

        pIp[0] = 1;                                             // enabled?
        pIp[1] = (BYTE)  ip;                                    // store the ip
        pIp[2] = (BYTE) (ip >>  8);
        pIp[3] = (BYTE) (ip >> 16);
        pIp[4] = (BYTE) (ip >> 24);

        if(pMsk) {
            pMsk[0] = 1;                                        // enabled?
            pMsk[1] = (BYTE)  mask;                             // store the mask
            pMsk[2] = (BYTE) (mask >>  8);
            pMsk[3] = (BYTE) (mask >> 16);
            pMsk[4] = (BYTE) (mask >> 24);
        }
    }

    freeifaddrs(ifaddr);
}

void Utils::forceSync(void)
{
	TMounterRequest tmr;
	tmr.action	= MOUNTER_ACTION_SYNC;                          // let the mounter thread do filesystem caches sync
	Mounter::add(tmr);
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
    utcOffset = s.getFloat("TIME_UTC_OFFSET", 0);          // read UTC offset from settings

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

std::string Utils::getDeviceLabel(const std::string & devicePath)
{
#define DEV_BY_LABEL_PATH "/dev/disk/by-label/"
	std::string label("");

	if(devicePath.substr(0,5) != "/dev/") return label;
	std::string devShort = devicePath.substr(5);

	DIR *d = opendir(DEV_BY_LABEL_PATH);
	if(d != NULL) {
		//FILE *f = fopen("/tmp/labels.txt", "w");
		//fprintf(f, "%s\n", devShort.c_str());
		struct dirent * de;
		char link_path[288];
		char link_target[256];
		while((de = readdir(d)) != NULL) {
			if(de->d_type == DT_LNK) {	// symbolic link
				snprintf(link_path, sizeof(link_path), "%s%s", DEV_BY_LABEL_PATH, de->d_name);
				ssize_t n = readlink(link_path, link_target, sizeof(link_target) - 1);
				//fprintf(f, "%s %d\n", link_path, (int)n);
				if(n >= 0) {
					link_target[n] = '\0';
					char * target_short = strrchr(link_target, '/');
					if(target_short != NULL) {
						target_short++;
						if(devShort == target_short) label = de->d_name;
						//fprintf(f, "%x %s %s\n", de->d_type, de->d_name, target_short);
					}
				}
			}
		}
		//fclose(f);
		closedir(d);
	}
	size_t pos;
	while((pos = label.find("\\x")) != std::string::npos) {
		int c;
		std::stringstream ss;
		ss << std::hex << label.substr(pos + 2, 2);
		ss >> c;
		label.replace(pos, 4, 1, (char)c);
	}
	return label;
}

void Utils::splitString(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
}

// make dir recursively (like mkdir -p)
int Utils::mkpath(const char *dir, int mode)
{
    struct stat sb;

    if (!dir) {
        errno = EINVAL;
        return -1;
    }

    if (!stat(dir, &sb))
        return 0;

    mkpath(dirname(strdupa(dir)), mode);

    return mkdir(dir, mode);
}

bool Utils::unZIPfloppyImageAndReturnFirstImage(const char *inZipFilePath, std::string &outImageFilePath)
{
    outImageFilePath.clear();                       // out path doesn't contain anything yet

    system("rm    -rf /tmp/zipedfloppy");           // delete this dir, if it exists
    system("mkdir -p  /tmp/zipedfloppy");           // create that dir

    char unzipCommand[512];
    sprintf(unzipCommand, "unzip -o '%s' -d /tmp/zipedfloppy > /dev/null 2> /dev/null", inZipFilePath);
    system(unzipCommand);                           // unzip the downloaded ZIP file into that tmp directory

    // find the first usable floppy image
    DIR *dir = opendir("/tmp/zipedfloppy");         // try to open the dir

    if(dir == NULL) {                               // not found?
        Debug::out(LOG_DEBUG, "Utils::unZIPfloppyImageAndReturnFirstImage -- opendir() failed");
        return false;
    }

    bool found          = false;
    struct dirent *de   = NULL;

    const char *pExt = NULL;

    while(1) {                                      // avoid buffer overflow
        de = readdir(dir);                          // read the next directory entry

        if(de == NULL) {                            // no more entries?
            break;
        }

        if(de->d_type != DT_REG) {                  // not a file? skip it
            continue;
        }

        int fileNameLen = strlen(de->d_name);       // get length of filename

        if(fileNameLen < 3) {                       // if it's too short, skip it
            continue;
        }

        pExt = getExtension(de->d_name);            // get where the extension starts

        if(pExt == NULL) {                          // extension not found? skip it
            continue;
        }

        if(strcasecmp(pExt, "st") == 0 || strcasecmp(pExt, "msa") == 0) {  // the extension of the file is valid for a floppy image?
            found = true;
            break;
        }
    }

    closedir(dir);                                  // close the dir

    if(!found) {                                    // not found? return with a fail
        Debug::out(LOG_DEBUG, "Utils::unZIPfloppyImageAndReturnFirstImage -- couldn't find an image inside of %s", inZipFilePath);
        return false;
    }

    // construct path to unZIPed image
    outImageFilePath = "/tmp/zipedfloppy/";
    outImageFilePath.append(de->d_name);

    Debug::out(LOG_DEBUG, "Utils::unZIPfloppyImageAndReturnFirstImage -- this ZIP file: %s contains this floppy image file: %s", inZipFilePath, outImageFilePath.c_str());
    return true;
}

const char *Utils::getExtension(const char *fileName)
{
    const char *pExt;
    pExt = strrchr(fileName, '.');  // find last '.'

    if(pExt == NULL) {              // last '.' not found? skip it
        return NULL;
    }

    pExt++;                         // move beyond '.'
    return pExt;
}

bool Utils::isZIPfile(const char *fileName)
{
    const char *ext = Utils::getExtension(fileName);     // find extension

    if(ext == NULL) {                       // no extension? not a ZIP file then
        return false;
    }

    return (strcasecmp(ext, "zip") == 0);   // it's a ZIP file when the extension is this
}

void Utils::createPathWithOtherExtension(std::string &inPathWithOriginalExt, const char *otherExtension, std::string &outPathWithOtherExtension)
{
    outPathWithOtherExtension = inPathWithOriginalExt;  // first just copy the input path

    const char *originalExt = Utils::getExtension(inPathWithOriginalExt.c_str());

    if(originalExt == NULL) {                           // failed to find extension? fail
        return;
    }
    int originalExtLen = strlen(originalExt);           // get length of original extension

    if(outPathWithOtherExtension.size() < ((size_t) originalExtLen)) { // path shorter than extension? (how could this happen???) fail
        return;
    }

    outPathWithOtherExtension.resize(outPathWithOtherExtension.size() - originalExtLen);    // remove original extension
    outPathWithOtherExtension = outPathWithOtherExtension + std::string(otherExtension);    // append other extension
}

bool Utils::fileExists(std::string &hostPath)
{
    int res = access(hostPath.c_str(), F_OK);
    return (res != -1);     // if not error, file exists
}
