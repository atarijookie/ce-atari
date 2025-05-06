#ifndef GLOBAL_H
#define GLOBAL_H

#include <string>
#include <stdint.h>
#include <pthread.h>

//////////////////////////////////////////////////////

void preloadGlobalsFromDotEnv(void);

#define LOG_DIR_DEFAULT     "/tmp/ce/log"
#define DATA_DIR_DEFAULT    "/tmp/ce/data"

#endif // GLOBAL_H
