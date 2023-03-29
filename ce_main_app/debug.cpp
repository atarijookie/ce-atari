// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "global.h"
#include "debug.h"
#include "utils.h"
#include "../libdospath/libdospath.h"

uint32_t prevLogOut;

extern TFlags   flags;
       uint8_t  g_outToConsole;

DebugVars dbgVars;

char Debug::logFilePath[128];

void Debug::setOutputToConsole(void)
{
    g_outToConsole = 1;
}

void Debug::setDefaultLogFile(void)
{
    // set default log file, without the need of dotEnv being loaded, so we can start logging as soon as possible,
    // even during the the loading of dotEnv file
    setLogFile("/var/log/ce/core.log");
}

void Debug::setDefaultLogFileFromEnvValue(void)
{
    std::string logFilePath = Utils::dotEnvValue("LOG_DIR", "/var/log/ce");     // path to logs dir
    Utils::mergeHostPaths(logFilePath, "core.log");             // full path = dir + filename
    setLogFile(logFilePath.c_str());                            // use full path here
}

void Debug::setLogFile(const char *path)
{
    strcpy(Debug::logFilePath, path);
}

void Debug::printfLogLevelString(void)
{
    printf("\nLog level: ");

    switch(flags.logLevel) {
        case LOG_OFF:       printf("OFF"); break;
        case LOG_ERROR:     printf("ERROR"); break;
        case LOG_WARNING:   printf("WARNING"); break;
        case LOG_INFO:      printf("INFO"); break;
        case LOG_DEBUG:     printf("DEBUG"); break;
        default:            printf("unknown!"); break;
    }

    printf("\n\n");
}

void Debug::out(int logLevel, const char *format, ...)
{
    if(logLevel > flags.logLevel) {         // if this log is higher than allowed, don't do this
        return;
    }

    va_list args;
    va_start(args, format);

    FILE *f;

    if(g_outToConsole) {                    // should log to console? f is null
        f = NULL;
    } else {                                    // log to file? open the file
        Debug::logRotateIfNeeded(logFilePath);  // rotate log file if too big
        f = fopen(logFilePath, "a+t");          // open the file
    }

    if(!f) {
        printf("%08d: ", Utils::getCurrentMs());
        vprintf(format, args);
        printf("\n");
        return;
    }

    uint32_t now = Utils::getCurrentMs();
    uint32_t diff = now - prevLogOut;
    prevLogOut = now;

    char humanTime[128];
    struct timeval tv;
    if(gettimeofday(&tv, NULL) < 0) {
        memset(&tv, 0, sizeof(tv)); // failure
    }
    struct tm tm = *localtime(&tv.tv_sec);
    sprintf(humanTime, "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec);

    if(logLevel == LOG_ERROR && dbgVars.isInHandleAcsiCommand) {    // it's an error, and we're debugging ACSI stuff
        fprintf(f, "%08d %08d (%s)\n", now, diff, humanTime); // CLOCK in ms, diff in ms, date/time in human readable format
        fprintf(f, "     LOG_ERROR occurred\n");
        fprintf(f, "     Time since beginning of ACSI command handling: %d\n", now - dbgVars.thisAcsiCmdTime);
        fprintf(f, "     Time between this and previous ACSI command  : %d\n", dbgVars.thisAcsiCmdTime - dbgVars.prevAcsiCmdTime);
    }

    fprintf(f, "%08d %08d (%s)\t", now, diff, humanTime); // CLOCK in ms, diff in ms, date/time in human readable format

    vfprintf(f, format, args);
    fprintf(f, "\n");
    fclose(f);

    va_end(args);
}

void Debug::outBfr(uint8_t *bfr, int count)
{
    if(flags.logLevel < LOG_DEBUG) {            // if we're not in debug log level, don't do this
        return;
    }

    FILE *f = fopen(logFilePath, "a+t");

    if(!f) {
        return;
    }

    fprintf(f, "%08d: outBfr - %d bytes\n", Utils::getCurrentMs(), count);

    int i, j;

    int rows = (count / 16) + (((count % 16) == 0) ? 0 : 1);

    for(i=0; i<rows; i++) {
        int ofs = i * 16;
        fprintf(f, "        ");//some indentation

        for(j=0; j<16; j++) {
            if((ofs + j) < count) {
                fprintf(f, "%02x ", bfr[ofs + j]);
            } else {
                fprintf(f, "   ");
            }
        }

        fprintf(f, "| ");

        for(j=0; j<16; j++) {
            char v = bfr[ofs + j];
            v = (v >= 32 && v <= 126) ? v : '.';

            if((ofs + j) < count) {
                fprintf(f, "%c", v);
            } else {
                fprintf(f, " ");
            }
        }

        fprintf(f, "\n");
    }

    //fprintf(f, "\n");
    fclose(f);
}

void Debug::setLogLevel(int newLogLevel)
{
    if(newLogLevel < LOG_OFF) {         // too low? fix it
        newLogLevel = LOG_OFF;
    }

    if(newLogLevel > LOG_DEBUG) {       // would be higher than highest log level? fix it
        newLogLevel = LOG_DEBUG;
    }

    Debug::out(LOG_INFO, "Switching LOG LEVEL from %d to %d", flags.logLevel, newLogLevel);
    flags.logLevel = newLogLevel;                               // new value to struct
    ldp_setParam(1, (uint64_t) flags.logLevel);                 // libDOSpath - set new log level to file

    Utils::intToFileFromEnv(newLogLevel, "CORE_LOGLEVEL_FILE");        // new value to file
}

void Debug::logRotateIfNeeded(const char *logFilePath)
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

void Debug::chipLog(const char* bfr)
{
    Debug::chipLog(strlen(bfr), (char*) bfr);
}

// chipLog() will receive incomplete lines stored in bfr, terminated by '\n'.
// It should write only complete lines to file, so it will try to gather chars until '\n' char and do write to file then.
void Debug::chipLog(uint16_t cnt, char* bfr)
{
    static std::string oneLine;
    static uint32_t prevLogOutChips = 0;
    static std::string chipLogFilePath;

    uint32_t now = Utils::getCurrentMs();
    uint32_t diff = now - prevLogOutChips;
    prevLogOutChips = now;

    if(chipLogFilePath.empty()) {
        chipLogFilePath = Utils::dotEnvValue("LOG_DIR", "/var/log/ce");     // path to logs dir
        Utils::mergeHostPaths(chipLogFilePath, "chip.log");             // full path = dir + filename
    }

    FILE *f = fopen(chipLogFilePath.c_str(), "a+t");

    if(!f) {                    // no file? quit
        return;
    }

    for(int i=0; i<cnt; i++) {  // for cnt of characters
        char val = bfr[i];      // get from buffer
        oneLine += val;         // append to string

        if(val == '\n') {       // if last char was new line, dump it to file
            fprintf(f, "%08d\t%08d\t ", now, diff);
            fputs(oneLine.c_str(), f);
            oneLine.clear();    // clear gathered line
        }
    }

    fclose(f);      // close file at the end
}
