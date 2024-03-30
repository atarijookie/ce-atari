#ifndef __UTILS_H__
#define __UTILS_H__

#ifndef MIN
    #define MIN(x, y)   (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
    #define MAX(x, y)   (((x) > (y)) ? (x) : (y))
#endif

#include <pthread.h>

extern pthread_mutex_t mutex;
void mutexLock(void);
void mutexUnlock(void);

bool fileExists(const char* path);

uint32_t getCurrentMs(void);
uint32_t getEndTime(uint32_t offsetFromNow);
void sleepMs(uint32_t ms);

#define LOG_OFF         0
#define LOG_INFO        1       // info         - info which can be displayed when running at user's place
#define LOG_ERROR       2       // errors       - should be always visible, even to users
#define LOG_WARNING     3       // warnings
#define LOG_DEBUG       4       // debug info   - useful only to developers

void log(int logLevel, const char *format, ...);
void logRotateIfNeeded(const char *logFilePath);

#endif
