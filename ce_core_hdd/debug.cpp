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
#include "../libdospath/libdospath.h"
#include "native/scsi.h"

uint32_t prevLogOut;

extern TFlags   flags;
       uint8_t  g_outToConsole;

DebugVars dbgVars;

std::string coreLogFileName;

const char* Debug::getCoreLogFileName(bool forceCreate)
{
    if(coreLogFileName.length() > 0 && !forceCreate) {
        return coreLogFileName.c_str();
    }

    std::string logDir = Utils::dotEnvValue("LOG_DIR", LOG_DIR_DEFAULT, false);
    coreLogFileName = logDir + std::string("/core_hdd_") + std::to_string(flags.portClient) + std::string(".log");
    return coreLogFileName.c_str();
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
    printf("%s", logLevelString(flags.logLevel));
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
        f = logFileOpen(CORE_HDD_LOG_FILENAME);
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
        fprintf(f, "%s %4d %s\n", humanTime, diff, ll); // diff in ms, date/time in human readable format
        fprintf(f, "     LOG_ERROR occurred\n");
        fprintf(f, "     Time since beginning of ACSI command handling: %d\n", now - dbgVars.thisAcsiCmdTime);
        fprintf(f, "     Time between this and previous ACSI command  : %d\n", dbgVars.thisAcsiCmdTime - dbgVars.prevAcsiCmdTime);
    }

    fprintf(f, "%s %4d %s\t", humanTime, diff, ll); // CLOCK in ms, diff in ms, date/time in human readable format

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

    FILE* f = logFileOpen(CORE_HDD_LOG_FILENAME);

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

    Utils::intToFileFromEnv(newLogLevel, "CORE_HDD_LOGLEVEL_FILE");        // new value to file
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

    Debug::logRotateIfNeeded(getCoreLogFileName());   // rotate log file if too big

    FILE *f = fopen(getCoreLogFileName(), "a+t");
    return f;
}
