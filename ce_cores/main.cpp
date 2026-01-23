// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <pty.h>
#include <sys/file.h>
#include <errno.h>
#include <limits.h>

#include "misc/settings.h"
#include "misc/global.h"
#include "hdd/corehdd.h"
#include "misc/debug.h"
#include "misc/version.h"
#include "misc/cmdsockthread.h"
#include "chipinterface/chipinterface.h"
#include "../libdospath/libdospath.h"
#include "discovery/discovery.h"
#include "ikbd/ikbd.h"

volatile sig_atomic_t sigintReceived = 0;
void sigint_handler(int sig);

void parseCmdLineArguments(int argc, char *argv[]);
void printfPossibleCmdLineArgs(void);

InterProcessEvents  events;
ExternalServices    externalServices;
bool justShowHelp = false;
int logLevel = LOG_ERROR;

void discoveryMain(void);
pthread_t discoveryThreadInfo;

int fddThreadCode(void);
pthread_t fddThreadInfo;

pthread_t cmdSockThreadInfo;
pthread_t ikbdThreadInfo;

bool otherInstanceIsRunning(void);
int runCoreHdd(void);

bool noCapture = false;

int main(int argc, char *argv[])
{
    printf("\033[H\033[2J\n");

    justShowHelp = false;
    Debug::setLogLevel(LOG_ERROR);      // init current log level to LOG_ERROR

    logHdd(LOG_INFO, "\n\n"); logHdd(LOG_INFO, "---------------------------------------------------");

    parseCmdLineArguments(argc, argv);                          // then parse cmd line arguments and set global variables
    Debug::printfLogLevelString();

    Utils::loadDotEnv();                                       // load dotEnv before setting default log file
    for(int i=LOGFILE_DISCOVERY; i<=LOGFILE_IKBD; i++) {
        Debug::getCoreLogFileName(true, i);    // call this with force=true to re-create the log file name
    }
    Debug::logLevelFromDotEnv();        // set log level from .env value of LOG_LEVEL

    ldp_setParam(1, (uint64_t) logLevel);                         // libDOSpath - set log level to file
    std::string logDir = Utils::dotEnvValue("LOG_DIR", LOG_DIR_DEFAULT);  // path to logs dir
    Utils::mergeHostPaths(logDir, "libdospath.log");                    // full path = logs dir + filename
    ldp_setParam(2, (uint64_t) logDir.c_str());                         // libDOSpath - set log file path
    logHdd(LOG_ERROR, "setting libdospath log file to: %s and log level to: %d", logDir.c_str(), logLevel);

    Utils::screenShotVblEnabled(false);                         // screenshot vbl not enabled by default
    preloadGlobalsFromDotEnv();

    //------------------------------------
    // if should only show help and quit
    if(justShowHelp) {
        printfPossibleCmdLineArgs();
        return 0;
    }

    //------------------------------------
    // before touching GPIO make sure that no other instance is running
    if(otherInstanceIsRunning()) {
        logHdd(LOG_ERROR, "Other instance of CosmosEx is running, terminate it before starting a new one!");
        printf("\nOther instance of CosmosEx is running, terminate it before starting a new one!\n\n\n");
        return 0;
    }

    logHdd(LOG_INFO, "logLevel: %d", logLevel);

    //------------------------------------
    // register signal handlers
    if(signal(SIGINT, sigint_handler) == SIG_ERR) {         // register SIGINT handler
        printf("Cannot register SIGINT handler!\n");
    }

    if(signal(SIGHUP, sigint_handler) == SIG_ERR) {         // register SIGHUP handler
        printf("Cannot register SIGHUP handler!\n");
    }

    handlePthreadCreate("discovery service", &discoveryThreadInfo, (void*) discoveryMain);
    handlePthreadCreate("command socket", &cmdSockThreadInfo, (void*) cmdSockThreadCode);
    handlePthreadCreate("floppy core", &fddThreadInfo, (void*) fddThreadCode);
    handlePthreadCreate("ikbd core", &ikbdThreadInfo, (void*) ikbdThreadCode);

    int ret = runCoreHdd();

    pthread_kill_join("ikbd core", ikbdThreadInfo);
    pthread_kill_join("floppy core", fddThreadInfo);
    pthread_kill_join("command socket", cmdSockThreadInfo);
    pthread_kill_join("discovery service", discoveryThreadInfo);

    return ret;
}

// return path and filename to pid file for this core's instance, include port to distinguish between instances
std::string pidFileName(void)
{
    std::string pidDir = Utils::dotEnvValue("PID_DIR", PID_DIR_DEFAULT);
    std::string pidFilePath = pidDir + std::string("/ce_cores.pid");
    return pidFilePath;
}

int runCoreHdd(void)
{
    CoreHdd *coreHdd;

    Debug::printfLogLevelString();

    char appVersion[16];
    Version::getAppVersion(appVersion);
    logHdd(LOG_INFO, "CosmosEx HDD core starting at port %d, version: %s", SERVER_TCP_PORT_HDD, appVersion);
    printf("\nCosmosEx HDD core starting at port %d, version: %s\n", SERVER_TCP_PORT_HDD, appVersion);

    Utils::setTimezoneVariable_inThisContext();

    //-------------
    coreHdd = new CoreHdd();

    printf("Entering main loop...\n");
    coreHdd->run();                // run the main thread
    printf("\n\nExit from main loop\n");

    delete coreHdd;

    // remove PID file on termination
    std::string pidFilePath = pidFileName();
    unlink(pidFilePath.c_str());

    logHdd(LOG_INFO, "CosmosEx terminated.");
    printf("Terminated\n");
    return 0;
}

void parseCmdLineArguments(int argc, char *argv[])
{
    int i;

    for(i=1; i<argc; i++) {                                         // go through all params (when i=0, it's app name, not param)
        int len = strlen(argv[i]);
        if(len < 1) {                                               // argument too short? skip it
            continue;
        }

        bool isKnownTag = false;

        // it's a LOG LEVEL change command (ll)
        if(strncmp(argv[i], "ll", 2) == 0) {
            isKnownTag = true;                                      // this is a known tag
            int ll;

            ll = (int) argv[i][2];

            if(ll >= 48 && ll <= 57) {                              // if it's a number between 0 and 9
                ll = ll - 48;
                Debug::setLogLevel(ll);                             // store log level
            }

            continue;
        }

        // don't capture USB mouse and keyboard
        if(strcmp(argv[i], "nocap") == 0) {
            isKnownTag = true;
            noCapture = true;
        }

        if(strcmp(argv[i], "help") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "/?") == 0 || strcmp(argv[i], "?") == 0) {
            isKnownTag = true;                             // this is a known tag
            justShowHelp  = true;
            continue;
        }

        if(!isKnownTag) {                                           // if tag unknown, show warning
            printf(">>> UNKNOWN APP ARGUMENT: '%s' <<<\n", argv[i]);
        }
    }
}

void printfPossibleCmdLineArgs(void)
{
    printf("\nPossible command line args:\n");
    printf("llx      - set log level to x (default is 1, max is 4)\n");
}

void sigint_handler(int sig)
{
    logHdd(LOG_DEBUG, "Some SIGNAL received, terminating.");
    sigintReceived = 1;
}

bool otherInstanceIsRunning(void)
{
    FILE * f;
    int other_pid = 0;
    int self_pid = 0;
    char proc_path[256];
    char other_exe[PATH_MAX];
    char self_exe[PATH_MAX];

    self_pid = getpid();
    std::string pidFilePath = pidFileName();

    f = fopen(pidFilePath.c_str(), "r");
    if(!f) {    // can't open file? other instance probably not running (or is, but can't figure out, so screw it)
        logHdd(LOG_DEBUG, "otherInstanceIsRunning - couldn't open %s, returning false", pidFilePath.c_str());
    } else {
        int r = fscanf(f, "%d", &other_pid);
        fclose(f);
        if(r != 1) {
            logHdd(LOG_ERROR, "otherInstanceIsRunning - can't read pid in %s, returning false", pidFilePath.c_str());
        } else {
            logHdd(LOG_DEBUG, "otherInstanceIsRunning - %s pid=%d (own pid=%d)", pidFilePath.c_str(), other_pid, self_pid);
            snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", other_pid);
            if(readlink("/proc/self/exe", self_exe, sizeof(self_exe)) < 0) {
                logHdd(LOG_ERROR, "otherInstanceIsRunning readlink(%s): %s", "/proc/self/exe", strerror(errno));
            } else if(readlink(proc_path, other_exe, sizeof(other_exe)) < 0) {
                logHdd(LOG_ERROR, "otherInstanceIsRunning readlink(%s): %s", proc_path, strerror(errno));
            } else if(strcmp(other_exe, self_exe) == 0) {
                // NOTE: is it needed to check the process status to discard zombies ???
                logHdd(LOG_DEBUG, "otherInstanceIsRunning - found another instance of %s with pid %d", other_exe, other_pid);
                return true;
            }
        }
    }

    Utils::intToFile(self_pid, pidFilePath.c_str());
    logHdd(LOG_DEBUG, "otherInstanceIsRunning -- pid %d written to %s", self_pid, pidFilePath.c_str());

    return false;
}

