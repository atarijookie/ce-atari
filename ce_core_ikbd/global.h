#ifndef GLOBAL_H
#define GLOBAL_H

#include <string>
#include <stdint.h>
#include <pthread.h>

#define SERVER_TCP_PORT_FDD         7400        // port used by FDD core
#define SERVER_TCP_PORT_IKBD        7401        // port used by IKBD core

typedef struct {
    bool justShowHelp;          // show possible command line arguments and quit
    int  logLevel;              // init current log level to LOG_ERROR
    bool noCapture;
    int  portClient;
} TFlags;

void preloadGlobalsFromDotEnv(void);

// These were global const string constants, but now they depend on .env content, so they are now
// loaded on app start and used when needed.
extern std::string corePath;

#define LOG_DIR_DEFAULT     "/tmp/ce/log"
#define DATA_DIR_DEFAULT    "/tmp/ce/data"
#define PID_DIR_DEFAULT     "/tmp/ce/pid"

#endif // GLOBAL_H
