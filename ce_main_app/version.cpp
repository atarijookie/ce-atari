#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "global.h"
#include "version.h"
#include "debug.h"

extern TFlags flags;        // global flags from command line
extern RPiConfig rpiConfig;

Version::Version()
{
    clear();
}

void Version::fromString(char *str)
{
    int y,m,d;

    clear();

    // read it
    int res = sscanf(str, "%d-%d-%d", &y, &m, &d);

    // if failed to read
    if(res != 3) {
        return;
    }

    fromInts(y, m, d);
}

void Version::fromStringWithoutDashes(char *str)
{
    int y,m,d, all, rest;

    clear();

    // read it
    int res = sscanf(str, "%d", &all);

    // if failed to read
    if(res != 1) {
        return;
    }

    y    = all / 10000; // get year part
    rest = all % 10000;
    m = rest / 100;     // get month
    d = rest % 100;     // get day

    fromInts(y, m, d);
}

void Version::toString(char *str)
{
    sprintf(str, "%04d-%02d-%02d", year, month, day);
}

void Version::fromInts(int y, int m, int d)
{
    clear();

    // if out of range
    if(y < 2013 || y > 2050 || m < 1 || m > 12 || d < 1 || d > 31) {
        return;
    }

    // store it
    year    = y;
    month   = m;
    day     = d;
}

void Version::clear(void)
{
    year    = 0;
    month   = 0;
    day     = 0;

    url.clear();
    checksum = 0;
}

void Version::fromFirstLineOfFile(char *filePath, bool withDashes)
{
    clear();

    FILE *f = fopen(filePath, "rt");            // try to open the file

    if(!f) {                                    // failed?
        return;
    }

    char line[1024], *r;
    r = fgets(line, 1024, f);                   // try to read a line

    if(!r) {                                    // failed?
        return;
    }

    fclose(f);                                  // close the file

    if(withDashes) {                            // with dashes
        fromString(line);                       // try to parse the line
    } else {                                    // without dashes
        fromStringWithoutDashes(line);
    }
}

void Version::setUrlAndChecksum(char *pUrl, char *chs)
{
    url = pUrl;

    int res, val;
    res = sscanf(chs, "0x%x", &val);

    if(res == 1) {
        checksum = val;
    } else {
        checksum = 0;
    }
}

std::string Version::getUrl(void)
{
    return url;
}

WORD Version::getChecksum(void)
{
    return checksum;
}

void Version::getAppVersion(char *bfr)
{
    char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char const *buildDate = __DATE__;

    int year = 0, month = 0, day = 0;
    int i;
    for(i=0; i<12; i++) {
        if(strncmp(months[i], buildDate, 3) == 0) {
            month = i + 1;
            break;
        }
    }

    i = sscanf(buildDate + 4, "%d %d", &day, &year);

    if(flags.fakeOldApp) {      // if should fake that the app is old, subtract 2 years from app year
        year -= 2;
    }

    if(i == 2 && month > 0) {
        sprintf(bfr, "%04d-%02d-%02d", year, month, day);
    } else {
        strcpy(bfr, "YYYY-MM-DD");
    }
}

void Version::getRaspberryPiInfo(void)
{
    // first parse the files, so we won't have to do this in C
    system("cat /proc/cpuinfo | grep 'Serial' | tr -d ' ' | awk -F ':' '{print $2}' > /tmp/rpiserial.txt");
    system("cat /proc/cpuinfo | grep 'Revision' | tr -d ' ' | awk -F ':' '{print $2}' > /tmp/rpirevision.txt");

    #ifdef DISTRO_STRETCH
    system("dmesg | grep 'Machine model' | awk -F 'model: ' '{print $2}' > /tmp/rpimodel.txt");
    #else
    system("dmesg | grep 'Machine model' | awk -F ': ' '{print $2}' > /tmp/rpimodel.txt");
    #endif

    // read in the data
    readLineFromFile("/tmp/rpiserial.txt",      rpiConfig.serial,   20, "unknown");
    readLineFromFile("/tmp/rpirevision.txt",    rpiConfig.revision,  8, "unknown");
    readLineFromFile("/tmp/rpimodel.txt",       rpiConfig.model,    40, "Raspberry Pi unknown model");

    // print to log file in debug mode
    Debug::out(LOG_DEBUG, "RPi serial  : %s", rpiConfig.serial);
    Debug::out(LOG_DEBUG, "RPi revision: %s", rpiConfig.revision);
    Debug::out(LOG_DEBUG, "RPi model   : %s", rpiConfig.model);

    // now try to convert revision from hex string to int, for easy comparing
    int res = sscanf(rpiConfig.revision, "%x", &rpiConfig.revisionInt);

    if(res != 1) {      // failed to get int from string? set zero
        rpiConfig.revisionInt = 0;
    }
}

void Version::readLineFromFile(const char *filename, char *buffer, int maxLen, const char *defValue)
{
    memset(buffer, 0, maxLen);                  // clear the buffer where the value should be stored

    FILE *f = fopen(filename, "rt");            // open the file

    if(!f) {                                    // if failed to open, copy in the default value
        strncpy(buffer, defValue, maxLen - 1);
        return;
    }

    char *res = fgets(buffer, maxLen - 1, f);   // try to read the line

    if(res == NULL) {
        strncpy(buffer, defValue, maxLen - 1);  // failed to read the line? use default value
        return;
    }

    int len = strlen(buffer);

    if(len > 0) {                                                   // got at least 1 character?
        if(buffer[len - 1] == '\n' || buffer[len - 1] == '\r') {    // if it's new line, remove it
            buffer[len - 1] = 0;
        }
    }

    if(len > 1) {                                                   // got at least 2 characters?
        if(buffer[len - 2] == '\n' || buffer[len - 2] == '\r') {    // if it's new line, remove it
            buffer[len - 2] = 0;
        }
    }

    fclose(f);                                  // close the file
}
