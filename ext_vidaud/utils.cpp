#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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
