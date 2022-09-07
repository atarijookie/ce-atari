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

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdarg.h>

#include "defs.h"

bool logsEnabled = false;

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

void Utils::splitString(const std::string &s, char delim, std::vector<std::string> &elems)
{
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        if(item.empty()) {              // skip empty items, e.g. start of path
            continue;
        }

        elems.push_back(item);
    }
}

void Utils::joinStrings(std::vector<std::string>& elems, std::string& output, int count)
{
    // join string using path delimiter, but only 'count' parts of the input
    // if count == -1, join all parts
    // if count >= 0, join only that many parts

    output = "";                    // start with empty string

    int len = elems.size();         // get count of parts we were supplied with

    if(count >= 0 && count < len) { // if count is positive number and it's less than all the parts count, use it
        len = count;
    }

    if(count == 0) {                // if should join 0 elements, return root path only
        output = "/";
        return;
    }

    for(int i=0; i<len; i++) {      // join all or just requested count of parts
        output.append("/");         // append delimiter and element
        output.append(elems[i]);
    }
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

bool Utils::fileExists(std::string &hostPath)
{
    int res = access(hostPath.c_str(), F_OK);
    return (res != -1);     // if not error, file exists
}

void Utils::out(int logLevel, const char *format, ...)
{
    if(!logsEnabled) {
        return;
    }

    va_list args;
    va_start(args, format);

    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void Utils::toHostSeparators(std::string &path)
{
    int len = path.length();

    for(int i=0; i<len; i++) {
        if(path[i] == '\\') {
            path[i] = '/';
        }
    }
}
