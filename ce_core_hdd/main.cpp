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
#include "ccorethread.h"
#include "debug.h"
#include "update.h"
#include "version.h"
#include "cmdsockthread.h"
#include "extension/extensionrecvthread.h"
#include "chipinterface_network/chipinterfacenetwork.h"
#include "../libdospath/libdospath.h"

volatile sig_atomic_t sigintReceived = 0;
void sigint_handler(int sig);

void handlePthreadCreate(const char* threadName, pthread_t* pThreadInfo, void* threadCode);
void parseCmdLineArguments(int argc, char *argv[]);
void printfPossibleCmdLineArgs(void);

void loadLastHwConfig(void);
void initializeFlags(void);

THwConfig           hwConfig;                           // info about the current HW setup
TFlags              flags;                              // global flags from command line
InterProcessEvents  events;
SharedObjects       shared;
ChipInterface*      chipInterface;
ExternalServices    externalServices;

bool otherInstanceIsRunning(void);

void showOnDisplay(int argc, char *argv[]);
int  runCore(void);
void networkServerMain(void);

int main(int argc, char *argv[])
{
    pthread_mutex_init(&shared.mtxHdd,   NULL);
    pthread_mutex_init(&shared.mtxImages, NULL);

    printf("\033[H\033[2J\n");

    initializeFlags();                                          // initialize flags
    Debug::out(LOG_INFO, "\n\n"); Debug::out(LOG_INFO, "---------------------------------------------------");

    parseCmdLineArguments(argc, argv);                          // then parse cmd line arguments and set global variables
    Debug::printfLogLevelString();

    Utils::loadDotEnv();                                        // load dotEnv before setting default log file
    Debug::getCoreLogFileName(true);        // call this with force=true to re-create the log file name

    ldp_setParam(1, (uint64_t) flags.logLevel);                         // libDOSpath - set log level to file
    std::string logDir = Utils::dotEnvValue("LOG_DIR", LOG_DIR_DEFAULT);  // path to logs dir
    Utils::mergeHostPaths(logDir, "libdospath.log");                    // full path = logs dir + filename
    ldp_setParam(2, (uint64_t) logDir.c_str());                         // libDOSpath - set log file path
    Debug::out(LOG_ERROR, "setting libdospath log file to: %s and log level to: %d", logDir.c_str(), flags.logLevel);

    Utils::screenShotVblEnabled(false);                         // screenshot vbl not enabled by default
    preloadGlobalsFromDotEnv();

    //------------------------------------
    // if should only show help and quit
    if(flags.justShowHelp) {
        printfPossibleCmdLineArgs();
        return 0;
    }

    //------------------------------------
    // before touching GPIO make sure that no other instance is running
    if(otherInstanceIsRunning()) {
        Debug::out(LOG_ERROR, "Other instance of CosmosEx is running, terminate it before starting a new one!");
        printf("\nOther instance of CosmosEx is running, terminate it before starting a new one!\n\n\n");
        return 0;
    }

    Debug::out(LOG_INFO, "logLevel: %d", flags.logLevel);

    loadLastHwConfig();                                     // load last found HW IF, HW version, SCSI machine

    //------------------------------------
    // register signal handlers
    if(signal(SIGINT, sigint_handler) == SIG_ERR) {         // register SIGINT handler
        printf("Cannot register SIGINT handler!\n");
    }

    if(signal(SIGHUP, sigint_handler) == SIG_ERR) {         // register SIGHUP handler
        printf("Cannot register SIGHUP handler!\n");
    }

    return runCore();
}

void pthread_kill_join(const char* threadName, pthread_t& threadInfo)
{
    printf("Stoping %s thread\n", threadName);
    pthread_kill(threadInfo, SIGINT);           // stop the select()
    pthread_join(threadInfo, NULL);             // wait until thread finishes
}

// return path and filename to pid file for this core's instance, include port to distinguish between instances
std::string pidFileName(void)
{
    std::string dataDir = Utils::dotEnvValue("DATA_DIR", DATA_DIR_DEFAULT);
    std::string pidFilePath = dataDir + std::string("/ce_hdd_") + std::to_string(flags.portClient) + std::string(".pid");
    return pidFilePath;
}

int runCore(void)
{
    CCoreThread *core;
    pthread_t cmdSockThreadInfo;
    pthread_t extensionThreadInfo;

    Debug::out(LOG_INFO, "runCore as network server");
    hwConfig.version = 3;
    chipInterface = new ChipInterfaceNetwork();     // create network chip interface
    chipInterface->ciOpen();                        // try to open it

    //------------------------------------
    // normal app run follows
    Debug::printfLogLevelString();

    char appVersion[16];
    Version::getAppVersion(appVersion);
    Debug::out(LOG_INFO, "CosmosEx HDD core starting at port %d, version: %s", flags.portClient, appVersion);
    printf("\nCosmosEx HDD core starting at port %d, version: %s\n", flags.portClient, appVersion);

    Utils::setTimezoneVariable_inThisContext();

    //-------------
    core = new CCoreThread();

    handlePthreadCreate("command socket", &cmdSockThreadInfo, (void*) cmdSockThreadCode);
    handlePthreadCreate("extension", &extensionThreadInfo, (void*) extensionThreadCode);

    printf("Entering main loop...\n");

    core->run();                // run the main thread

    printf("\n\nExit from main loop\n");

    delete core;

    pthread_kill_join("command socket", cmdSockThreadInfo);
    pthread_kill_join("extension", extensionThreadInfo);

    //---------------------------------------------------
    // Closing of GPIO should be done after stopping IKBD thread and DISPLAY thread
    // as they also use some GPIO pins and we want them to be able to use them until the end.
    chipInterface->ciClose();                           // close gpio
    delete chipInterface;
    chipInterface = NULL;
    //---------------------------------------------------

    // remove PID file on termination
    std::string pidFilePath = pidFileName();
    unlink(pidFilePath.c_str());

    Debug::out(LOG_INFO, "CosmosEx terminated.");
    printf("Terminated\n");
    return 0;
}

void loadLastHwConfig(void)
{
    Settings s;

    hwConfig.changed        = false;
    memset(hwConfig.hwSerial, 0, 13);
}

void initializeFlags(void)
{
    flags.justShowHelp = false;
    Debug::setLogLevel(LOG_ERROR);      // init current log level to LOG_ERROR
    flags.portServerReport = 7200;
    flags.portClient = 7300;
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

        if(argv[i][0] == 'p') {
            isKnownTag = true;                                      // this is a known tag
            int res = sscanf(argv[i] + 1, "%d", &flags.portClient);
            if(res != 1) {
                printf(">>> BAD CLIENT PORT VALUE: '%s' <<<\n", argv[i] + 1);
                Debug::out(LOG_ERROR, ">>> BAD CLIENT PORT VALUE: '%s' <<<\n", argv[i] + 1);
            }
        }

        if(argv[i][0] == 'r') {
            isKnownTag = true;                                      // this is a known tag
            int res = sscanf(argv[i] + 1, "%d", &flags.portServerReport);
            if(res != 1) {
                printf(">>> BAD REPORT PORT VALUE: '%s' <<<\n", argv[i] + 1);
                Debug::out(LOG_ERROR, ">>> BAD REPORT PORT VALUE: '%s' <<<\n", argv[i] + 1);
            }
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
    printf("pXXXX    - set listening port to XXXX\n");
    printf("rXXXX    - set port for status reporting to XXXX\n");
}

void handlePthreadCreate(const char* threadName, pthread_t* pThreadInfo, void* threadCode)
{
    int res = pthread_create(pThreadInfo, NULL, (void* (*)(void*)) threadCode, NULL);

    if(res != 0) {
        Debug::out(LOG_ERROR, "Failed to create %s thread, %s won't work...", threadName, threadName);
    } else {
        Debug::out(LOG_DEBUG, "%s thread created", threadName);
        pthread_setname_np(*pThreadInfo, threadName);
    }
}

void sigint_handler(int sig)
{
    Debug::out(LOG_DEBUG, "Some SIGNAL received, terminating.");
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
        Debug::out(LOG_DEBUG, "otherInstanceIsRunning - couldn't open %s, returning false", pidFilePath.c_str());
    } else {
        int r = fscanf(f, "%d", &other_pid);
        fclose(f);
        if(r != 1) {
            Debug::out(LOG_ERROR, "otherInstanceIsRunning - can't read pid in %s, returning false", pidFilePath.c_str());
        } else {
            Debug::out(LOG_DEBUG, "otherInstanceIsRunning - %s pid=%d (own pid=%d)", pidFilePath.c_str(), other_pid, self_pid);
            snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", other_pid);
            if(readlink("/proc/self/exe", self_exe, sizeof(self_exe)) < 0) {
                Debug::out(LOG_ERROR, "otherInstanceIsRunning readlink(%s): %s", "/proc/self/exe", strerror(errno));
            } else if(readlink(proc_path, other_exe, sizeof(other_exe)) < 0) {
                Debug::out(LOG_ERROR, "otherInstanceIsRunning readlink(%s): %s", proc_path, strerror(errno));
            } else if(strcmp(other_exe, self_exe) == 0) {
                // NOTE: is it needed to check the process status to discard zombies ???
                Debug::out(LOG_DEBUG, "otherInstanceIsRunning - found another instance of %s with pid %d", other_exe, other_pid);
                return true;
            }
        }
    }

    Utils::intToFile(self_pid, pidFilePath.c_str());
    Debug::out(LOG_DEBUG, "otherInstanceIsRunning -- pid %d written to %s", self_pid, pidFilePath.c_str());

    return false;
}

