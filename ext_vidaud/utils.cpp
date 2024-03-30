#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <cstdio>
#include <string>
#include <cstdarg>

#include "main.h"
#include "utils.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOG_LEVEL   LOG_DEBUG

bool fileExists(const char* path)
{
    struct stat sb;

    // if can get stat and ISREG is in mode, it's a regular file
    if (stat(path, &sb) == 0 && S_ISREG(sb.st_mode)) {
        return true;
    }

    return false;
}

void mutexLock(void)
{
    pthread_mutex_lock(&mutex);
}

void mutexUnlock(void)
{
    pthread_mutex_unlock(&mutex);
}

uint32_t getCurrentMs(void)
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

uint32_t getEndTime(uint32_t offsetFromNow)
{
    uint32_t val;

    val = getCurrentMs() + offsetFromNow;

    return val;
}

void sleepMs(uint32_t ms)
{
    uint32_t us = ms * 1000;

    usleep(us);
}

void log(int logLevel, const char *format, ...)
{
    if(logLevel > LOG_LEVEL) {         // if this log is higher than allowed, don't do this
        return;
    }

    va_list args;
    va_start(args, format);

    logRotateIfNeeded(LOG_FILE_NAME);       // rotate log file if too big
    FILE* f = fopen(LOG_FILE_NAME, "a+t");  // open the file

    uint32_t now = getCurrentMs();

    if(!f) {    // if couldn't open file, write to console
        printf("%08d: ", now);
        vprintf(format, args);
        printf("\n");
        return;
    }

    // opened the file so write to file
    fprintf(f, "%08d    ", now);
    vfprintf(f, format, args);
    fprintf(f, "\n");
    fclose(f);

    va_end(args);
}

void logRotateIfNeeded(const char *logFilePath)
{
    struct stat attr;
    int res = stat(logFilePath, &attr);             // get file stat

    if(res == 0 && (attr.st_size >= (1024*1024))) {             // file too big?
        std::string logFilePathOld = std::string(logFilePath) + ".1";       // construct old log filename
        printf("will rotate log file: %s -> %s\n", logFilePath, logFilePathOld.c_str());
        unlink(logFilePathOld.c_str());                         // if some previous old file exist, remove it
        rename(logFilePath, logFilePathOld.c_str());            // rename current to old
    }
}
