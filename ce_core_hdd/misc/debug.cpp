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

#include "misc/global.h"
#include "misc/debug.h"
#include "misc/utils.h"
#include "../libdospath/libdospath.h"
#include "../hdd/native/scsi.h"

uint32_t prevLogOut;
uint8_t  g_outToConsole;

DebugVars dbgVars;

std::string coreLogFileName[3];

const char* Debug::getCoreLogFileName(bool forceCreate, int whichLog)
{
    if(coreLogFileName[whichLog].length() > 0 && !forceCreate) {
        return coreLogFileName[whichLog].c_str();
    }

    std::string logDir = Utils::dotEnvValue("LOG_DIR", LOG_DIR_DEFAULT, false);

    switch(whichLog) {
        case LOGFILE_HDD: coreLogFileName[whichLog] = logDir + std::string("/" CORE_HDD_LOG_FILENAME); break;
        case LOGFILE_FDD: coreLogFileName[whichLog] = logDir + std::string("/" CORE_FDD_LOG_FILENAME); break;
        case LOGFILE_IKBD: coreLogFileName[whichLog] = logDir + std::string("/" CORE_IKBD_LOG_FILENAME); break;
    }

    return coreLogFileName[whichLog].c_str();
}

void Debug::setOutputToConsole(void)
{
    g_outToConsole = 1;
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

void logHdd(int aLogLevel, const char *format, ...)
{
    if(aLogLevel > logLevel) {         // if this log is higher than allowed, don't do this
        return;
    }

    va_list args;
    va_start(args, format);
    Debug::outV(LOGFILE_HDD, aLogLevel, format, args);
    va_end(args);
}

void logFdd(int aLogLevel, const char *format, ...)
{
    if(aLogLevel > logLevel) {         // if this log is higher than allowed, don't do this
        return;
    }

    va_list args;
    va_start(args, format);
    Debug::outV(LOGFILE_FDD, aLogLevel, format, args);
    va_end(args);
}

void logIkbd(int aLogLevel, const char *format, ...)
{
    if(aLogLevel > logLevel) {         // if this log is higher than allowed, don't do this
        return;
    }

    va_list args;
    va_start(args, format);
    Debug::outV(LOGFILE_IKBD, aLogLevel, format, args);
    va_end(args);
}

void logHdd(int whichLog, int aLogLevel, const char *format, ...)
{
    if(aLogLevel > logLevel) {         // if this log is higher than allowed, don't do this
        return;
    }

    va_list args;
    va_start(args, format);
    Debug::outV(whichLog, aLogLevel, format, args);
    va_end(args);
}

void Debug::out(int whichLog, int aLogLevel, const char *format, ...)
{
    if(aLogLevel > logLevel) {         // if this log is higher than allowed, don't do this
        return;
    }

    va_list args;
    va_start(args, format);
    outV(whichLog, aLogLevel, format, args);
    va_end(args);
}

void Debug::outV(int whichLog, int aLogLevel, const char *format, va_list args)
{
    if(aLogLevel > logLevel) {         // if this log is higher than allowed, don't do this
        return;
    }

    FILE *f;

    if(g_outToConsole) {                    // should log to console? f is null
        f = NULL;
    } else {                                    // log to file? open the file
        f = logFileOpen(whichLog);
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

    const char* ll = logLevelString(aLogLevel);

    char humanTime[128];
    struct timeval tv;
    if(gettimeofday(&tv, NULL) < 0) {
        memset(&tv, 0, sizeof(tv)); // failure
    }
    struct tm tm = *localtime(&tv.tv_sec);
    sprintf(humanTime, "%02d:%02d:%02d.%06ld", tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec);

    if(aLogLevel == LOG_ERROR && dbgVars.isInHandleAcsiCommand) {    // it's an error, and we're debugging ACSI stuff
        fprintf(f, "%s %4d %s\n", humanTime, diff, ll); // diff in ms, date/time in human readable format
        fprintf(f, "     LOG_ERROR occurred\n");
        fprintf(f, "     Time since beginning of ACSI command handling: %d\n", now - dbgVars.thisAcsiCmdTime);
        fprintf(f, "     Time between this and previous ACSI command  : %d\n", dbgVars.thisAcsiCmdTime - dbgVars.prevAcsiCmdTime);
    }

    fprintf(f, "%s %4d %s\t", humanTime, diff, ll); // CLOCK in ms, diff in ms, date/time in human readable format

    vfprintf(f, format, args);
    fprintf(f, "\n");
    fclose(f);
}

void Debug::outBfr(int whichLog, uint8_t *bfr, int count)
{
    if(logLevel < LOG_DEBUG) {            // if we're not in debug log level, don't do this
        return;
    }

    FILE* f = logFileOpen(whichLog);

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

    logHdd(LOG_INFO, "Switching LOG LEVEL from %d to %d", logLevel, newLogLevel);
    logFdd(LOG_INFO, "Switching LOG LEVEL from %d to %d", logLevel, newLogLevel);
    logIkbd(LOG_INFO, "Switching LOG LEVEL from %d to %d", logLevel, newLogLevel);
    logLevel = newLogLevel;                               // new value to struct
    ldp_setParam(1, (uint64_t) logLevel);                 // libDOSpath - set new log level to file
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

FILE* Debug::logFileOpen(int whichLog)
{
    static std::string path;

    Debug::logRotateIfNeeded(getCoreLogFileName(false, whichLog));   // rotate log file if too big

    FILE *f = fopen(getCoreLogFileName(false, whichLog), "a+t");
    return f;
}

void Debug::logLevelFromDotEnv(void)
{
    std::string logLevelStr = Utils::dotEnvValue("LOG_LEVEL", "1");
    int ll;

    ll = (int) logLevelStr.c_str()[0];

    if(ll >= 48 && ll <= 57) {                              // if it's a number between 0 and 9
        ll = ll - 48;
        Debug::setLogLevel(ll);                             // store log level
    }
}
