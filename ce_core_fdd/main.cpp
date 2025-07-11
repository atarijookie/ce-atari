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

#include "settings.h"
#include "global.h"
#include "floppythread.h"
#include "debug.h"
#include "cmdsockthread.h"
#include "floppy/floppyencoder.h"
#include "chipinterface_network/chipinterfacenetwork.h"
#include "../libdospath/libdospath.h"

volatile sig_atomic_t sigintReceived = 0;
void sigint_handler(int sig);

void handlePthreadCreate(const char* threadName, pthread_t* pThreadInfo, void* threadCode);
void parseCmdLineArguments(int argc, char *argv[]);
void printfPossibleCmdLineArgs(void);

void initializeFlags(void);

TFlags              flags;                              // global flags from command line
SharedObjects       shared;
ChipInterfaceNetwork* chipInterface;

bool otherInstanceIsRunning(void);
std::string pidFileName(void);
int runCore(void);

int main(int argc, char *argv[])
{
    pthread_mutex_init(&shared.mtxImages, NULL);

    printf("\033[H\033[2J\n");

    initializeFlags();                                          // initialize flags
    logFdd(LOG_INFO, "\n\n"); logFdd(LOG_INFO, "---------------------------------------------------");

    parseCmdLineArguments(argc, argv);                          // then parse cmd line arguments and set global variables
    Debug::printfLogLevelString();

    Utils::loadDotEnv();                    // load dotEnv before setting default log file
    Debug::getCoreLogFileName(true);        // call this with force=true to re-create the log file name
    Debug::logLevelFromDotEnv();            // set log level from .env value of LOG_LEVEL

    preloadGlobalsFromDotEnv();

    // if should only show help and quit
    if(flags.justShowHelp) {
        printfPossibleCmdLineArgs();
        return 0;
    }

    // make sure that no other instance is running
    if(otherInstanceIsRunning()) {
        logFdd(LOG_ERROR, "Other instance of CosmosEx is running, terminate it before starting a new one!");
        printf("\nOther instance of CosmosEx is running, terminate it before starting a new one!\n\n\n");
        return 0;
    }

    logFdd(LOG_INFO, "logLevel: %d", flags.logLevel);

    //------------------------------------------------------------
    // Opening of chip interface.
    logFdd(LOG_INFO, "ChipInterface: starting NETWORK server");

    chipInterface = new ChipInterfaceNetwork();

    if(!chipInterface->ciOpen()) {
        logFdd(LOG_ERROR, "ChipInterface - failed to open chip Interface, terminating.");
        printf("\nChipInterface - failed to open chip Interface, terminating.\n");
        return 0;
    }

    //------------------------------------
    // register signal handlers
    if(signal(SIGINT, sigint_handler) == SIG_ERR) {         // register SIGINT handler
        printf("Cannot register SIGINT handler!\n");
    }

    if(signal(SIGHUP, sigint_handler) == SIG_ERR) {         // register SIGHUP handler
        printf("Cannot register SIGHUP handler!\n");
    }

    //------------------------------------------------------------
    // if came here, we should run this app as the main local core
    return runCore();
}

void pthread_kill_join(const char* threadName, pthread_t& threadInfo)
{
    printf("Stoping %s thread\n", threadName);
    pthread_kill(threadInfo, SIGINT);           // stop the select()
    pthread_join(threadInfo, NULL);             // wait until thread finishes
}

int runCore(void)
{
    FloppyThread *core;
    pthread_t floppyEncThreadInfo;
    pthread_t cmdSockThreadInfo;

    logFdd(LOG_INFO, "CosmosEx FDD core starting at port %d", SERVER_TCP_PORT_FDD);
    printf("\nCosmosEx FDD core starting at port %d\n", SERVER_TCP_PORT_FDD);
    printf("\nlog file: %s\n", Debug::getCoreLogFileName(false));

    Utils::setTimezoneVariable_inThisContext();

    //-------------
    core = new FloppyThread();

    handlePthreadCreate("floppy encoder", &floppyEncThreadInfo, (void*) floppyEncodeThreadCode);
    handlePthreadCreate("command socket", &cmdSockThreadInfo, (void*) cmdSockThreadCode);

    printf("Entering main loop...\n");

    core->run();                // run the main thread

    printf("\n\nExit from main loop\n");

    delete core;

    floppyEncoder_stop();
    pthread_kill_join("floppy encoder", floppyEncThreadInfo);
    pthread_kill_join("command socket", cmdSockThreadInfo);

    chipInterface->ciClose();
    delete chipInterface;
    chipInterface = NULL;

    // remove PID file on termination
    std::string pidFilePath = pidFileName();
    unlink(pidFilePath.c_str());

    logFdd(LOG_INFO, "CosmosEx terminated.");
    printf("Terminated\n");
    return 0;
}

void initializeFlags(void)
{
    flags.justShowHelp = false;
    Debug::setLogLevel(LOG_ERROR);      // init current log level to LOG_ERROR
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

        if(strcmp(argv[i], "help") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "/?") == 0 || strcmp(argv[i], "?") == 0) {
            isKnownTag          = true;                             // this is a known tag
            flags.justShowHelp  = true;
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

void handlePthreadCreate(const char* threadName, pthread_t* pThreadInfo, void* threadCode)
{
    int res = pthread_create(pThreadInfo, NULL, (void* (*)(void*)) threadCode, NULL);

    if(res != 0) {
        logFdd(LOG_ERROR, "Failed to create %s thread, %s won't work...", threadName, threadName);
    } else {
        logFdd(LOG_DEBUG, "%s thread created", threadName);
        pthread_setname_np(*pThreadInfo, threadName);
    }
}

void sigint_handler(int sig)
{
    logFdd(LOG_DEBUG, "Some SIGNAL received, terminating.");
    sigintReceived = 1;
}

std::string pidFileName(void)
{
    std::string pidDir = Utils::dotEnvValue("PID_DIR", PID_DIR_DEFAULT);
    std::string pidFilePath = pidDir + std::string("/ce_fdd.pid");
    return pidFilePath;
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
        logFdd(LOG_DEBUG, "otherInstanceIsRunning - couldn't open %s, returning false", pidFilePath.c_str());
    } else {
        int r = fscanf(f, "%d", &other_pid);
        fclose(f);
        if(r != 1) {
            logFdd(LOG_ERROR, "otherInstanceIsRunning - can't read pid in %s, returning false", pidFilePath.c_str());
        } else {
            logFdd(LOG_DEBUG, "otherInstanceIsRunning - %s pid=%d (own pid=%d)", pidFilePath.c_str(), other_pid, self_pid);
            snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", other_pid);
            if(readlink("/proc/self/exe", self_exe, sizeof(self_exe)) < 0) {
                logFdd(LOG_ERROR, "otherInstanceIsRunning readlink(%s): %s", "/proc/self/exe", strerror(errno));
            } else if(readlink(proc_path, other_exe, sizeof(other_exe)) < 0) {
                logFdd(LOG_ERROR, "otherInstanceIsRunning readlink(%s): %s", proc_path, strerror(errno));
            } else if(strcmp(other_exe, self_exe) == 0) {
                // NOTE: is it needed to check the process status to discard zombies ???
                logFdd(LOG_DEBUG, "otherInstanceIsRunning - found another instance of %s with pid %d", other_exe, other_pid);
                return true;
            }
        }
    }

    Utils::intToFile(self_pid, pidFilePath.c_str());
    logFdd(LOG_DEBUG, "otherInstanceIsRunning -- pid %d written to %s", self_pid, pidFilePath.c_str());

    return false;
}
