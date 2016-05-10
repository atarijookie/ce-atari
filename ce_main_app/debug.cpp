// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>

#include "global.h"
#include "debug.h"
#include "utils.h"

#define LOG_FILE        "/var/log/ce.log"
DWORD prevLogOut;

extern TFlags   flags;
       BYTE     g_outToConsole;

DebugVars dbgVars;

char Debug::logFilePath[128];

void Debug::setOutputToConsole(void)
{
    g_outToConsole = 1;
}

void Debug::setDefaultLogFile(void)
{
    setLogFile((char *) LOG_FILE);
}

void Debug::setLogFile(char *path)
{
    strcpy(Debug::logFilePath, path);
}

void Debug::printfLogLevelString(void)
{
    printf("\nLog level: ");

    switch(flags.logLevel) {
        case LOG_OFF:   printf("OFF"); break;
        case LOG_ERROR: printf("ERROR"); break;
        case LOG_INFO:  printf("INFO"); break;
        case LOG_DEBUG: printf("DEBUG"); break;
        default:        printf("unknown!"); break;
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
    } else {                                // log to file? open the file
        f = fopen(logFilePath, "a+t");
    }

    if(!f) {
        printf("%08d: ", Utils::getCurrentMs());
        vprintf(format, args);
        printf("\n");
        return;
    }

    DWORD now = Utils::getCurrentMs();
    DWORD diff = now - prevLogOut;
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
        fprintf(f, "%08d\t%08d\t(%s)\n", now, diff, humanTime); // CLOCK in ms, diff in ms, date/time in human readable format
        fprintf(f, "     LOG_ERROR occurred\n");
        fprintf(f, "     Time since beginning of ACSI command handling: %d\n", now - dbgVars.thisAcsiCmdTime);
        fprintf(f, "     Time between this and previous ACSI command  : %d\n", dbgVars.thisAcsiCmdTime - dbgVars.prevAcsiCmdTime);
    }
    
    fprintf(f, "%08d\t%08d\t(%s)\t", now, diff, humanTime); // CLOCK in ms, diff in ms, date/time in human readable format
    
    vfprintf(f, format, args);
    fprintf(f, "\n");
    fclose(f);

    va_end(args);
}

void Debug::outBfr(BYTE *bfr, int count)
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

