#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdint.h>
#include <cstdio>

#define CORE_IKBD_LOG_FILENAME   "core_ikbd.log"
#define IKBD_LOG_FILENAME        "ikbd.log"

#define LOG_OFF         0
#define LOG_INFO        1       // info         - info which can be displayed when running at user's place
#define LOG_ERROR       2       // errors       - should be always visible, even to users
#define LOG_WARNING     3       // warnings
#define LOG_DEBUG       4       // debug info   - useful only to developers

typedef struct {
    uint8_t    isInHandleAcsiCommand;
    uint32_t   prevAcsiCmdTime;
    uint32_t   thisAcsiCmdTime;   
} DebugVars;

class Debug
{
public:
    static const char* getCoreLogFileName(bool forceCreate=false);
    static void logLevelFromDotEnv(void);

    static void out(int logLevel, const char *format, ...);
    static void outBfr(uint8_t *bfr, int count);

    static const char* logLevelString(int ll);
    static void printfLogLevelString(void);

    static void setLogLevel(int newLogLevel);
    static void setOutputToConsole(void);

    static void logRotateIfNeeded(const char *logFilePath);

    static FILE* logFileOpen(const char* logFileName);
};

#endif

