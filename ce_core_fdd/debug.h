#ifndef _DEBUG_H_
#define _DEBUG_H_

#include <stdint.h>
#include <cstdio>

#define CORE_HDD_LOG_FILENAME   "core_hdd.log"
#define CORE_FDD_LOG_FILENAME   "core_fdd.log"
#define CORE_IKBD_LOG_FILENAME  "core_ikbd.log"

#define LOGFILE_HDD     0
#define LOGFILE_FDD     1
#define LOGFILE_IKBD    2

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

void logHdd(int logLevel, const char *format, ...);
void logFdd(int logLevel, const char *format, ...);
void logIkbd(int logLevel, const char *format, ...);

class Debug
{
public:
    static void logLevelFromDotEnv(void);

    static void out(int whichLog, int logLevel, const char *format, ...);
    static void outBfr(int whichLog, uint8_t *bfr, int count);
    static void outV(int whichLog, int logLevel, const char *format, va_list args);

    static const char* logLevelString(int ll);
    static void printfLogLevelString(void);

    static void setLogLevel(int newLogLevel);
    static void setOutputToConsole(void);
    static void setLogFile(const char *path);

    static void logRotateIfNeeded(const char *logFilePath);

    static FILE* logFileOpen(int whichLog);
    static const char* getCoreLogFileName(bool forceCreate = false, int whichLog = LOGFILE_HDD);
};

#endif

