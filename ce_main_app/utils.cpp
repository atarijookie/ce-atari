// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
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
#include <map>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <fcntl.h>

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
#include "settings.h"
#include "global.h"

std::map<std::string, std::string> dotEnv;

uint32_t Utils::getCurrentMs(void)
{
    struct timespec tp;
    int res;

    res = clock_gettime(CLOCK_MONOTONIC, &tp);                  // get current time

    if(res != 0) {                                              // if failed, fail
        return 0;
    }

    uint32_t val = (tp.tv_sec * 1000) + (tp.tv_nsec / 1000000);    // convert to milli seconds
    return val;
}

uint32_t Utils::getEndTime(uint32_t offsetFromNow)
{
    uint32_t val;

    val = getCurrentMs() + offsetFromNow;

    return val;
}

void Utils::attributesHostToAtari(bool isReadOnly, bool isDir, uint8_t &attrAtari)
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

uint16_t Utils::fileTimeToAtariDate(struct tm *ptm)
{
    uint16_t atariDate = 0;

    if(ptm == NULL) {
        return 0;
    }

    atariDate |= (ptm->tm_year - 80) << 9;            // year (tm_year is 'years since 1900', we want 'years since 1980', so the difference is -80
    atariDate |= (ptm->tm_mon  +  1) << 5;            // month
    atariDate |= (ptm->tm_mday     );                 // day

    return atariDate;
}

uint16_t Utils::fileTimeToAtariTime(struct tm *ptm)
{
    uint16_t atariTime = 0;

    if(ptm == NULL) {
        return 0;
    }

    atariTime |= (ptm->tm_hour      ) << 11;        // hours
    atariTime |= (ptm->tm_min       ) << 5;         // minutes
    atariTime |= (ptm->tm_sec   / 2 );              // seconds

    return atariTime;
}

void Utils::fileDateTimeToHostTime(uint16_t atariDate, uint16_t atariTime, struct tm *ptm)
{
    uint16_t year, month, day;
    uint16_t hours, minutes, seconds;

    year    = (atariDate >> 9)   + 1980; // 0-119 with 0=1980
    month   = (atariDate >> 5)   & 0x0f; // 1-12
    day     =  atariDate         & 0x1f; // 1-31

    hours   =  (atariTime >> 11) & 0x1f;    // 0-23
    minutes =  (atariTime >>  5) & 0x3f;    // 0-59
    seconds = ( atariTime        & 0x1f) * 2;   // in unit of two

    memset(ptm, 0, sizeof(struct tm));
    ptm->tm_year    = year - 1900;      // number of years since 1900.
    ptm->tm_mon     = month - 1;    // The number of months since January, in the range 0 to 11
    ptm->tm_mday    = day;      // The day of the month, in the range 1 to 31.

    ptm->tm_hour    = hours;    // The number of hours past midnight, in the range 0 to 23
    ptm->tm_min     = minutes;  // The number of minutes after the hour, in the range 0 to 59
    ptm->tm_sec     = seconds;  // The number of seconds after the minute, normally in the range 0 to 59,
                                //  but can be up to 60 to allow for leap seconds.
}

std::string Utils::mergeHostPaths3(const std::string& head, const char* tail)
{
    std::string tailStr(tail);
    return Utils::mergeHostPaths2(head, tailStr);
}

std::string Utils::mergeHostPaths2(const std::string& head, const std::string& tail)
{
    // this method creates merged path, and it doesn't modifiy head:    rev_val = head + tail

    static std::string retVal;              // static string, which will be used as return value
    retVal = head;                          // copy of head into return value, so we won't modify head
    Utils::mergeHostPaths(retVal, tail);    // merge retVal (head) with tail
    return retVal;                          // return merged value
}

void Utils::mergeHostPaths(std::string &dest, const std::string &tail)
{
    // this method creates merged path, but it modifies dest:     dest = dest + tail

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
    splitToTwoByDelim(pathAndFile, path, file, HOSTPATH_SEPAR_CHAR);
}

void Utils::splitFilenameFromExt(const std::string &filenameAndExt, std::string &filename, std::string &ext)
{
    splitToTwoByDelim(filenameAndExt, filename, ext, '.');
}

void Utils::splitToTwoByDelim(const std::string &input, std::string &beforeDelim, std::string &afterDelim, char delim)
{
    size_t sepPos = input.rfind(delim);

    if(sepPos == std::string::npos) {                           // delimiter not found? everything belongs to beforeDelim
        beforeDelim = input;
        afterDelim.clear();
    } else {                                                    // delimiter found? split it to two parts
        beforeDelim = input.substr(0, sepPos);
        afterDelim = input.substr(sepPos + 1);
    }
}

void Utils::mergeFilenameAndExtension(const std::string& shortFn, const std::string& shortExt, bool extendWithSpaces, std::string& merged)
{
    std::string shortFn2 = shortFn, shortExt2 = shortExt;       // make copies so we don't modify originals

    if(shortFn2.size() > 8)             // filename too long? shorten
        shortFn2.resize(8);

    if(shortExt2.size() > 3)            // file ext too long? shorten
        shortExt2.resize(3);

    if(extendWithSpaces) {              // if should extend
        if(shortFn2.size() < 8)         // filename too short? extend
            shortFn2.resize(8, ' ');

        if(shortExt2.size() < 3)        // extension too short? extend
            shortExt2.resize(3, ' ');
    }

    merged = shortFn2;                  // put in the filename
    
    if(shortExt2.size() > 0)            // if got extension, append extension
        merged += std::string(".") + shortExt2;
}

void Utils::extendWithSpaces(const char *normalFname, char *extendedFn)
{
    std::string sNormalFname = normalFname, fname, ext, sExtendedFn;

    Utils::splitFilenameFromExt(normalFname, fname, ext);               // split 'FILE.C' to 'FILE' and 'C'
    Utils::mergeFilenameAndExtension(fname, ext, true, sExtendedFn);    // extend and merge, so will get 'FILE    .C  '

    strcpy(extendedFn, sExtendedFn.c_str());
}

void Utils::sleepMs(uint32_t ms)
{
    uint32_t us = ms * 1000;

    usleep(us);
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
    uint8_t bfr64k[64 * 1024];

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

void Utils::SWAPWORD(uint16_t &w)
{
    uint16_t a,b;

    a = w >> 8;         // get top
    b = w  & 0xff;      // get bottom

    w = (b << 8) | a;   // store swapped
}

uint16_t Utils::SWAPWORD2(uint16_t w)
{
    uint16_t a,b;

    a = w >> 8;         // get top
    b = w  & 0xff;      // get bottom

    w = (b << 8) | a;   // store swapped
    return w;
}

void Utils::getIpAdds(uint8_t *bfrIPs, uint8_t *bfrMasks)
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

        uint32_t ip            = saiIp->sin_addr.s_addr;
        uint32_t mask          = saiMsk->sin_addr.s_addr;

        uint8_t *pIp   = NULL;
        uint8_t *pMsk  = NULL;

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
        pIp[1] = (uint8_t)  ip;                                    // store the ip
        pIp[2] = (uint8_t) (ip >>  8);
        pIp[3] = (uint8_t) (ip >> 16);
        pIp[4] = (uint8_t) (ip >> 24);

        if(pMsk) {
            pMsk[0] = 1;                                        // enabled?
            pMsk[1] = (uint8_t)  mask;                             // store the mask
            pMsk[2] = (uint8_t) (mask >>  8);
            pMsk[3] = (uint8_t) (mask >> 16);
            pMsk[4] = (uint8_t) (mask >> 24);
        }
    }

    freeifaddrs(ifaddr);
}

void Utils::forceSync(void)
{
    printf("TODO: forceSync\n");
}

uint16_t Utils::getWord(uint8_t *bfr)
{
    uint16_t val = 0;

    val = bfr[0];       // get hi
    val = val << 8;

    val |= bfr[1];      // get lo

    return val;
}

uint32_t Utils::getDword(uint8_t *bfr)
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

uint32_t Utils::get24bits(uint8_t *bfr)
{
    uint32_t val = 0;

    val  = bfr[0];       // get hi
    val  = val << 8;

    val |= bfr[1];      // get mid
    val  = val << 8;

    val |= bfr[2];      // get lo

    return val;
}

void Utils::storeWord(uint8_t *bfr, uint16_t val)
{
    bfr[0] = val >> 8;  // store hi
    bfr[1] = val;       // store lo
}

void Utils::storeDword(uint8_t *bfr, uint32_t val)
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
            if(de->d_type == DT_LNK) {  // symbolic link
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
    char * dircopy;

    if (dir == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (stat(dir, &sb) == 0) {
        return 0;   // the directory already exists
    }

    // strdup() dir because some implementations of dirname() store the result in the input buffer
    dircopy = strdup(dir);
    mkpath(dirname(dircopy), mode); // create (recursively) parent directory
    free(dircopy);

    return mkdir(dir, mode);    // create directory itself, once all parents are created
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

bool Utils::fileExists(const std::string& path)
{
    return Utils::fileExists(path.c_str());
}

bool Utils::fileExists(const char* path)
{
    struct stat sb;

    // if can get stat and ISREG is in mode, it's a regular file
    if (stat(path, &sb) == 0 && S_ISREG(sb.st_mode)) {
        return true;
    }

    return false;
}

bool Utils::dirExists(std::string& path)
{
    struct stat sb;

    // if can get stat and ISDIR is in mode, it's a dir
    if (stat(path.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)) {
        return true;
    }

    return false;
}

bool Utils::devExists(std::string& path)
{
    struct stat sb;

    // if can get stat and S_ISBLK is in mode, it's a block device
    if (stat(path.c_str(), &sb) == 0 && S_ISBLK(sb.st_mode)) {
        return true;
    }

    return false;
}

int Utils::bcdToInt(int bcd)
{
    int a,b;

    a = bcd >> 4;       // upper nibble
    b = bcd &  0x0f;    // lower nibble

    return ((a * 10) + b);
}

void Utils::toUpperCaseString(std::string &st)
{
    int i, len;
    len = st.length();

    for(i=0; i<len; i++) {
        st[i] = toupper(st[i]);
    }
}

void Utils::loadDotEnv(void)
{
    /* try to load .env from multiple locations in their priority order */

    bool good = false;
    good = loadDotEnvFrom("/ce/services/.env");     // try to load from main location

    if(!good) {
        good = loadDotEnvFrom("./.env");            // if failed, try from local directory
    }

    if(good) {      // if something was loaded, do the vars subtitution
        dotEnvSubstituteVars();
    }
}

std::string Utils::dotEnvValue(std::string key, const char* defValue)
{
    /* get value from dotEnv map for specified key */

    try {
        std::string& value = dotEnv.at(key);             // get value and return it (if found in map)
        return value;
    }
    catch (const std::out_of_range&) {
        Debug::out(LOG_DEBUG, "Utils::dotEnvValue - no value for key '%s' !", key.c_str());
    }

    // if got here, the value wasn't found in map, but it still could be a real env var, so try getting it
    char* envVar = getenv(key.c_str());

    if(envVar) {    // some real env var was found with this name?
        static std::string retValueFromEnv;
        retValueFromEnv = envVar;
        Debug::out(LOG_DEBUG, "Utils::dotEnvValue - ...but found env var '%s' with value '%s'", key.c_str(), retValueFromEnv.c_str());
        return retValueFromEnv;
    }

    // if value not found and default value was provided, use it; otherwise return empty string
    std::string defValueStr = defValue ? std::string(defValue) : std::string("");
    return defValueStr;
}

void Utils::getDefaultValueFromVarName(std::string& varName, std::string& defValue, const std::string& delim)
{
    std::size_t varDef = varName.find(delim);    // check if this var name has also default value specified

    //Debug::out(LOG_DEBUG, "Utils::getDefaultValueFromVarName - varName '%s'", varName.c_str());

    if(varDef != std::string::npos) {           // if this variable name has also default value specified
        std::string newVarName = varName.substr(0, varDef); // get just var name without default value
        defValue = varName.substr(varDef + delim.length()); // get just the default value
        //Debug::out(LOG_DEBUG, "Utils::getDefaultValueFromVarName - varName with default: '%s', varName '%s', defValue: '%s'", varName.c_str(), newVarName.c_str(), defValue.c_str());
        varName = newVarName;                               // use the new var name
    }
}

int Utils::dotEnvSubstituteVars(void)
{
    /* go through the current dotEnv values and replace vars with values */

    Debug::out(LOG_DEBUG, "Utils::dotEnvSubstituteVars starting");

    int found = 0;

    std::map<std::string, std::string>::iterator it = dotEnv.begin();
    while (it != dotEnv.end())                          // go through all map values
    {
        std::string key = it->first;
        std::string value = it->second;

        std::size_t varStart = value.find("${");       // var start tag
        std::size_t varEnd = value.find("}");          // var end tag

        if(varStart != std::string::npos) {                 // start tag was found?
            std::string varName = value.substr(varStart + 2, varEnd - varStart - 2);    // get just var name

            std::string defValue;
            Utils::getDefaultValueFromVarName(varName, defValue, std::string(":-"));        // try the longer first
            Utils::getDefaultValueFromVarName(varName, defValue, std::string("-"));         // then shorter next

            std::size_t varDef = varName.find(":-");    // check if this var name has also default value specified

            //Debug::out(LOG_DEBUG, "Utils::dotEnvSubstituteVars - varName '%s'", varName.c_str());

            if(varDef != std::string::npos) {           // if this variable name has also default value specified
                std::string newVarName = varName.substr(0, varDef); // get just var name without default value
                defValue = varName.substr(varDef + 2);              // get just the default value
                //Debug::out(LOG_DEBUG, "Utils::dotEnvSubstituteVars - varName with default: '%s', varName '%s', defValue: '%s'", varName.c_str(), newVarName.c_str(), defValue.c_str());
                varName = newVarName;                               // use the new var name
            }

            std::string varValue = Utils::dotEnvValue(varName, defValue.c_str());   // get variable value with possible default value
            //Debug::out(LOG_DEBUG, "Utils::dotEnvSubstituteVars - for var '%s' found value '%s'", varName.c_str(), varValue.c_str());

            value.replace(varStart, varEnd - varStart + 1, varValue);   // replace variable in original value
            //Debug::out(LOG_DEBUG, "Utils::dotEnvSubstituteVars - value after replacing var: '%s'", value.c_str());

            dotEnv[key] = value;        // store new value back to map
            found++;
        }

        ++it;
    }

    return found;
}

bool Utils::loadDotEnvFrom(const char* path)
{
    /* try to load .env file from the specified path */

    FILE *f = fopen(path, "rt");        // try to open file

    if(!f) {
        Debug::out(LOG_ERROR, "Utils::loadDotEnv - failed to open file %s", path);
        return false;
    }

    char line[1024];

    while(true) {                           // go through file line by line
        if(feof(f)) {
            break;
        }

        int eqlPos = -1;                    // where the '=' is
        memset(line, 0, sizeof(line));
        fgets(line, sizeof(line) - 1, f);   // get one line, including '\n' symbol

        // first loop - remove new line chars, and everything after comment symbol
        int len = strlen(line);     // get length of line
        for(int i=0; i<len; i++) {
            if(line[i] == '\n' || line[i] == '\r' || line[i] == '#') {     // remove EOL, RET, and if it's a start of comment, ignore the rest of line
                line[i] = 0;        // string ends here now
                break;
            }

            if(line[i] == '=') {    // found equal (=) sign? store position
                eqlPos = i;
            }
        }

        // second loop - trim trailing white spaces
        len = strlen(line);         // get length of line
        for(int i=(len-1); i>=0; i--) {
            if(line[i] == ' ' || line[i] == '\t') {     // space or tab? trim
                line[i] = 0;
            }
        }

        // check if something remained after previous changes to line
        len = strlen(line);         // get length of line
        if(len < 1 || eqlPos < 0) { // line empty or no equal sign there? skip it
            continue;
        }

        line[eqlPos] = 0;           // split the string on the equal sign

        std::string key, value;
        key = line;                 // key   is on [0 : eqlPos-1]
        value = line + eqlPos + 1;  // value is on [eqlPos+1 : ...]
        //Debug::out(LOG_DEBUG, "Utils::loadDotEnv - found %s -> %s", key.c_str(), value.c_str());

        dotEnv[key] = value;        // store to map
    }

    fclose(f);
    return true;
}

void Utils::intToFileFromEnv(int value, const char* envKeyForFileName)
{
    std::string fileNameFromEnv = Utils::dotEnvValue(envKeyForFileName);    // fetch filename from env by key
    Utils::intToFile(value, fileNameFromEnv.c_str());                       // int to filename
}

void Utils::intToFile(int value, const char* filePath)
{
    char bfr[128];
    int lastIndex = sizeof(bfr) - 1;
    bfr[lastIndex] = 0;                     // zero terminate buffer

    snprintf(bfr, lastIndex, "%d", value);  // integer to string
    Utils::textToFile(bfr, filePath);       // string to file
}

void Utils::textToFileFromEnv(const char* text, const char* envKeyForFileName)
{
    std::string fileNameFromEnv = Utils::dotEnvValue(envKeyForFileName);    // fetch filename from env by key
    Utils::textToFile(text, fileNameFromEnv.c_str());                       // text to filename
}

void Utils::textToFile(const char* text, const char* filePath)
{
    if(!filePath || strlen(filePath) < 1) {     // null path or empty string path? quit
        return;
    }

    FILE *f = fopen(filePath, "wt");    // open file

    if(!f) {                            // could not open file? quit
        return;
    }

    fputs(text, f);                     // write text to file
    fclose(f);                          // close file
}

void Utils::textFromFile(char* bfr, uint32_t bfrSize, const char* filePath, bool trimTrailing)
{
    /*
        Read one line from text file specified by path, up to bfrSize of length.
    */

    if(!filePath || strlen(filePath) < 1) {     // null path or empty string path? quit
        return;
    }

    FILE *f = fopen(filePath, "rt");    // open file

    if(!f) {                            // could not open file? quit
        return;
    }

    fgets(bfr, bfrSize, f);             // try to read
    fclose(f);                          // close file

    if(trimTrailing) {                  // if should also trim trailing white spaces, do that
        Utils::trimTrail(bfr);
    }
}

void Utils::trimTrail(char *bfr)
{
    /* trim trailing white spaces from the specified buffer */

    int len = strlen(bfr);

    for(int i = (len-1); i >= 0; i--) {
        if(bfr[i] == '\n' || bfr[i] == '\r' || bfr[i] == '\t' || bfr[i] == ' ') {   // blank char found?
            bfr[i] = 0;     // clear
            continue;       // try next char
        }

        break;              // if got here, this char is not blank, wasn't cleared and we should stop
    }
}

void Utils::screenShotVblEnabled(bool enabled)
{
    events.screenShotVblEnabled = enabled;
    Utils::intToFileFromEnv((int) enabled, "SCREENSHOT_VBL_ENABLED_FILE");        // new value to file
}

void Utils::sendToMounter(const std::string& jsonString)
{
    std::string sockPath = Utils::dotEnvValue("MOUNT_SOCK_PATH");    // path to mounter socket

	// create a UNIX DGRAM socket
	int sockFd = socket(AF_UNIX, SOCK_DGRAM, 0);

	if (sockFd < 0) {   // if failed to create socket
	    Debug::out(LOG_ERROR, "sendToMounter: failed to create socket - errno: %d", errno);
	    return;
	}

    struct sockaddr_un addr;
    strcpy(addr.sun_path, sockPath.c_str());
    addr.sun_family = AF_UNIX;

    // try to send to mounter socket
    int res = sendto(sockFd, jsonString.c_str(), jsonString.length() + 1, 0, (struct sockaddr *) &addr, sizeof(struct sockaddr_un));

    if(res < 0) {       // if failed to send
	    Debug::out(LOG_ERROR, "sendToMounter: sendto failed - errno: %d", errno);
    } else {
        Debug::out(LOG_DEBUG, "sendToMounter: sent to mounter: %s", jsonString.c_str());
    }

    close(sockFd);
}

void Utils::createFloppyTestImage(void)
{
    // open the file and write to it
    FILE *f = fopen(FDD_TEST_IMAGE_PATH_AND_FILENAME.c_str(), "wb");

    if(!f) {
        Debug::out(LOG_ERROR, "Failed to create floppy test image!");
        printf("Failed to create floppy test image!\n");
        return;
    }

    // first fill the write buffer with simple counter
    uint8_t writeBfr[512];
    int i;
    for(i=0; i<512; i++) {
        writeBfr[i] = (uint8_t) i;
    }

    // write one sector after another...
    int sector, track, side;
    for(track=0; track<80; track++) {
        for(side=0; side<2; side++) {
            for(sector=1; sector<10; sector++) {
                // customize write data
                writeBfr[0] = track;
                writeBfr[1] = side;
                writeBfr[2] = sector;

                fwrite(writeBfr, 1, 512, f);
            }
        }
    }

    // close file and we're done
    fclose(f);
}

bool Utils::endsWith(std::string const& value, const char* ending)
{
    size_t lenEnding = strlen(ending);

    if (lenEnding > value.size()) {     // ending longer than value? surely doesn't end with ending
        return false;
    }

    // compare this value at its end with the ending
    return (value.compare(value.size() - lenEnding, lenEnding, ending) == 0);
}
