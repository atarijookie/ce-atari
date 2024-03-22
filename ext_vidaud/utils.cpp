#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "utils.h"

bool fileExists(const char* path)
{
    struct stat sb;

    // if can get stat and ISREG is in mode, it's a regular file
    if (stat(path, &sb) == 0 && S_ISREG(sb.st_mode)) {
        return true;
    }

    return false;
}

