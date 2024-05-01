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

std::map<std::string, std::string> logPaths;

void Debug::setOutputToConsole(void)
{
    g_outToConsole = 1;
}

void Debug::setDefaultLogFile(void)
{
    std::string filename = CORE_LOG_FILENAME;
    std::map<std::string, std::string>::iterator i = logPaths.find(filename);

    if(i != logPaths.end()) {   // got this log file? erase it from map
        logPaths.erase(i);
    }

    FILE* f = logFileOpen(CORE_LOG_FILENAME);   // call this to update map, then just close the file
    fclose(f);
}

void Debug::setLogFile(const char *path)
{
    std::string pathStr = path;
    logPaths[CORE_LOG_FILENAME] = pathStr;
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
        f = logFileOpen(CORE_LOG_FILENAME);
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
    if(flags.logLevel < LOG_DEBUG) {            // if we're not in debug log level, don't do this
        return;
    }

    FILE* f = logFileOpen(CORE_LOG_FILENAME);

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

    uint32_t now = Utils::getCurrentMs();
    uint32_t diff = now - prevLogOutChips;
    prevLogOutChips = now;

    FILE* f = logFileOpen(CHIP_LOG_FILENAME);

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

uint32_t cmdStartTime;
bool cmdEndCalled = false;
uint32_t cmdBytesCount = 0;

void Debug::cmdMarkStartTime(void)
{
    cmdStartTime = Utils::getCurrentMs();
}

void Debug::cmdStart(uint8_t* cmd, const char* tag)
{
    if(flags.logLevel < LOG_DEBUG) {    // log level is lower than debug? don't write
        return;
    }

    FILE* f = logFileOpen(HDD_LOG_FILENAME);

    if(!f) {
        return;
    }

    uint8_t cmdLen = Scsi::getCmdLengthFromCmdBytesAcsi(cmd);

    if(!cmdEndCalled) {     // if cmdEnd() wasn't called, terminate the previous line
        fprintf(f, "\n");
        cmdEndCalled = true;
    }

    switch(cmdLen) {
        case 6:     fprintf(f, "%08d - cmd( 6): %02x %02x %02x %02x %02x %02x                      - %-8s - ", cmdStartTime, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], tag); break;
        case 7:     fprintf(f, "%08d - cmd( 7): %02x %02x %02x %02x %02x %02x %02x                   - %-8s - ", cmdStartTime, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], tag); break;
        case 11:    fprintf(f, "%08d - cmd(11): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x       - %-8s - ", cmdStartTime, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9], cmd[10], tag); break;
        case 13:
        default:    fprintf(f, "%08d - cmd(%2d): %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x - %-8s - ", cmdStartTime, cmdLen, cmd[0], cmd[1], cmd[2], cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], cmd[9], cmd[10], cmd[11], cmd[12], tag); break;
    }

    fclose(f);
}

void Debug::cmdMid(bool readNotWrite, uint32_t count)
{
    cmdBytesCount = count;

    if(flags.logLevel < LOG_DEBUG) {    // log level is lower than debug? don't write
        return;
    }

    FILE* f = logFileOpen(HDD_LOG_FILENAME);

    if(!f) {
        return;
    }

    fprintf(f, "%s - len: %8d B - ", readNotWrite ? "READ " : "WRITE", count);
    fclose(f);
}

void Debug::cmdEnd(uint8_t statusByte, bool succeeded)
{
    cmdEndCalled = true;                // this cmdEnd() was called

    if(flags.logLevel < LOG_DEBUG) {    // log level is lower than debug? don't write
        return;
    }

    FILE* f = logFileOpen(HDD_LOG_FILENAME);

    if(!f) {
        return;
    }

    uint32_t now = Utils::getCurrentMs();
    uint32_t duration = now - cmdStartTime;

    fprintf(f, "status: %02x - dur: %4d ms", statusByte, duration);
    const char* ok = succeeded ? "OK" : "FAIL";

    if(cmdBytesCount >= 1024) { // if at least 1kB transfered, calc and log speed
        uint32_t kBps = ((cmdBytesCount/1024)*1000) / duration;
        fprintf(f, ", %4d kB/s, %s\n", kBps, ok);
    } else {                    // 
        fprintf(f, ",          , %s\n", ok);
    }

    fclose(f);
}
