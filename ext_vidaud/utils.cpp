#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#include "utils.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

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
