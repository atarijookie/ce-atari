// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <map>

#include "global.h"
#include "debug.h"
#include "utils.h"

uint32_t prevLogOut;
extern int logLevel;

uint8_t  g_outToConsole;

DebugVars dbgVars;

std::map<std::string, std::string> logPaths;

void Debug::setOutputToConsole(void)
{
    g_outToConsole = 1;
}

void Debug::setDefaultLogFile(void)
{
    std::string filename = DISCOVERY_LOG_FILENAME;
    std::map<std::string, std::string>::iterator i = logPaths.find(filename);

    if(i != logPaths.end()) {   // got this log file? erase it from map
        logPaths.erase(i);
    }

    FILE* f = logFileOpen(DISCOVERY_LOG_FILENAME);   // call this to update map, then just close the file
    if(f) {
        fclose(f);
    }
}

void Debug::setLogFile(const char *path)
{
    std::string pathStr = path;
    logPaths[DISCOVERY_LOG_FILENAME] = pathStr;
}

const char* Debug::logLevelString(int ll)
{
    switch(ll) {
        case LOG_OFF:       return "OFF  ";
        case LOG_ERROR:     return "ERROR";
        case LOG_WARNING:   return "WARN ";
        case LOG_INFO:      return "INFO ";
        case LOG_DEBUG:     return "DEBUG";
        default:            return "???  ";
    }
}

void Debug::printfLogLevelString(void)
{
    printf("\nLog level: ");
    printf("%s", logLevelString(logLevel));
    printf("\n\n");
}

void Debug::out(int inLogLevel, const char *format, ...)
{
    if(inLogLevel > logLevel) {         // if this log is higher than allowed, don't do this
        return;
    }

    va_list args;
    va_start(args, format);

    FILE *f;

    if(g_outToConsole) {                    // should log to console? f is null
        f = NULL;
    } else {                                    // log to file? open the file
        f = logFileOpen(DISCOVERY_LOG_FILENAME);
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

    const char* ll = logLevelString(logLevel);

    char humanTime[128];
    struct timeval tv;
    if(gettimeofday(&tv, NULL) < 0) {
        memset(&tv, 0, sizeof(tv)); // failure
    }
    struct tm tm = *localtime(&tv.tv_sec);
    sprintf(humanTime, "%02d:%02d:%02d.%06ld", tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec);

    if(logLevel == LOG_ERROR && dbgVars.isInHandleAcsiCommand) {    // it's an error, and we're debugging ACSI stuff
        fprintf(f, "%08d %4d (%s) %s\n", now, diff, humanTime, ll); // CLOCK in ms, diff in ms, date/time in human readable format
        fprintf(f, "     LOG_ERROR occurred\n");
        fprintf(f, "     Time since beginning of ACSI command handling: %d\n", now - dbgVars.thisAcsiCmdTime);
        fprintf(f, "     Time between this and previous ACSI command  : %d\n", dbgVars.thisAcsiCmdTime - dbgVars.prevAcsiCmdTime);
    }

    fprintf(f, "%08d %4d (%s) %s\t", now, diff, humanTime, ll); // CLOCK in ms, diff in ms, date/time in human readable format

    vfprintf(f, format, args);
    fprintf(f, "\n");
    fclose(f);

    va_end(args);
}

void Debug::outBfr(uint8_t *bfr, int count)
{
    if(logLevel < LOG_DEBUG) {            // if we're not in debug log level, don't do this
        return;
    }

    FILE* f = logFileOpen(DISCOVERY_LOG_FILENAME);

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

    Debug::out(LOG_INFO, "Switching LOG LEVEL from %d to %d", logLevel, newLogLevel);
    logLevel = newLogLevel;                               // new value to struct
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

FILE* Debug::logFileOpen(const char* logFileName)
{
    static std::string path;

    std::string logFileStdStr = logFileName;
    std::map<std::string, std::string>::iterator i = logPaths.find(logFileStdStr);
    std::string logPath;

    if(i == logPaths.end()) {   // don't have path for this file, create it, store it
        logPath = Utils::dotEnvValue("LOG_DIR", LOG_DIR_DEFAULT);   // path to logs dir
        Utils::mergeHostPaths(logPath, logFileStdStr);              // merge filename into path
        logPaths[logFileStdStr] = logPath;                          // store to map for next time
    } else {                    // have path, use value
        logPath = i->second;
    }

    Debug::logRotateIfNeeded(logPath.c_str());   // rotate log file if too big

    FILE *f = fopen(logPath.c_str(), "a+t");
    return f;
}
