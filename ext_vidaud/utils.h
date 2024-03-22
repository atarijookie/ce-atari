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

#endif
