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

#include "config/consoleappsstream.h"
#include "settings.h"
#include "global.h"
#include "ccorethread.h"
#include "debug.h"
#include "ikbd/ikbd.h"
#include "update.h"
#include "version.h"
#include "cmdsockthread.h"
#include "display/displaythread.h"
#include "floppy/floppyencoder.h"
#include "chipinterface_v1_v2/chipinterface12.h"
#include "chipinterface_v3/chipinterface3.h"
#include "chipinterface_network/chipinterfacenetwork.h"
#include "chipinterface_dummy/chipinterfacedummy.h"
#include "../libdospath/libdospath.h"

volatile sig_atomic_t sigintReceived = 0;
void sigint_handler(int sig);

void handlePthreadCreate(const char* threadName, pthread_t* pThreadInfo, void* threadCode);
void parseCmdLineArguments(int argc, char *argv[]);
void printfPossibleCmdLineArgs(void);
void loadDefaultArgumentsFromFile(void);

void loadLastHwConfig(void);
void initializeFlags(void);

THwConfig           hwConfig;                           // info about the current HW setup
TFlags              flags;                              // global flags from command line
RPiConfig           rpiConfig;                          // RPi model, revision, serial
InterProcessEvents  events;
SharedObjects       shared;
ChipInterface*      chipInterface;
ExternalServices    externalServices;

bool otherInstanceIsRunning(void);
int  singleInstanceSocketFd;

void showOnDisplay(int argc, char *argv[]);
int  runCore(int instanceNo, bool localNotNetwork);
void networkServerMain(void);

int main(int argc, char *argv[])
{
    pthread_mutex_init(&shared.mtxHdd,   NULL);
    pthread_mutex_init(&shared.mtxImages, NULL);

    printf("\033[H\033[2J\n");

    Debug::setDefaultLogFile();                                 // set some log file before .env is loaded
    initializeFlags();                                          // initialize flags
    Debug::out(LOG_INFO, "\n\n"); Debug::out(LOG_INFO, "---------------------------------------------------");

    loadDefaultArgumentsFromFile();                             // first parse default arguments from file
    parseCmdLineArguments(argc, argv);                          // then parse cmd line arguments and set global variables
    Debug::printfLogLevelString();

    Utils::loadDotEnv();                                        // load dotEnv before setting default log file
    Debug::setDefaultLogFileFromEnvValue();                     // set log after .env is loaded

    ldp_setParam(1, (uint64_t) flags.logLevel);                         // libDOSpath - set log level to file
    std::string logDir = Utils::dotEnvValue("LOG_DIR", "/var/log/ce");  // path to logs dir
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

    //------------------------------------------------------------
    // Opening of chip interface.

    bool good = false;
    chipInterface = NULL;

    if(flags.chipInterface == CHIPIF_UNKNOWN || flags.chipInterface == CHIPIF_V1_V2) {  // chip interface unknown or v1/v2
        Debug::out(LOG_INFO, "ChipInterface: v1/v2");
        chipInterface = new ChipInterface12();                  // create chip interface v1/v2
        hwConfig.version = 2;
    } else if(flags.chipInterface == CHIPIF_V3) {
        Debug::out(LOG_INFO, "ChipInterface: v3");
        chipInterface = new ChipInterface3();                   // create chip interface v3
        hwConfig.version = 2;
    } else if(flags.chipInterface == CHIPIF_DUMMY) {
        Debug::out(LOG_INFO, "ChipInterface: opening DUMMY chip interface");
        chipInterface = new ChipInterfaceDummy();
        hwConfig.version = 2;
        flags.noReset = true;
    } else if(flags.chipInterface == CHIPIF_NETWORK) {
        Debug::out(LOG_INFO, "ChipInterface: starting NETWORK server");
        networkServerMain();
        return 0;
    } else {
        Debug::out(LOG_INFO, "ChipInterface - unknown option, terminating.");
        printf("\nChipInterface - unknown option, terminating.\n");
        return 0;
    }

    good = chipInterface->ciOpen();                         // try to open chip interface v1/v2

    // after the previous lines the good flag should contain if we were able to open the chip interface
    if(!good) {
        printf("\nHW_VER: UNKNOWN\n");
        printf("\nHDD_IF: UNKNOWN\n");

        Debug::out(LOG_INFO, "ChipInterface - failed to open chip Interface %d, terminating.", flags.chipInterface);
        printf("\nChipInterface - failed to open chip Interface %d, terminating.\n", flags.chipInterface);
        return 0;
    }

    //------------------------------------------------------------
    loadLastHwConfig();                                     // load last found HW IF, HW version, SCSI machine

    //------------------------------------
    // register signal handlers
    if(signal(SIGINT, sigint_handler) == SIG_ERR) {         // register SIGINT handler
        printf("Cannot register SIGINT handler!\n");
    }

    if(signal(SIGHUP, sigint_handler) == SIG_ERR) {         // register SIGHUP handler
        printf("Cannot register SIGHUP handler!\n");
    }

    //------------------------------------------------------------
    if(flags.display || flags.getHwInfo || flags.justDoReset) {
        if(flags.display) {                             // if should show some string on front display
            showOnDisplay(argc, argv);

        } else if(flags.getHwInfo) {                    // if should just get HW info, do a shorter / simpler version of app run
            Debug::out(LOG_INFO, ">>> Starting app as HW INFO tool <<<\n");

            CCoreThread *core = new CCoreThread();   // create main thread
            core->run();                               // run the main thread

        } else if(flags.justDoReset) {                  // is this a reset command? (used with STM32 ST-Link debugger)
            chipInterface->resetHDDandFDD();
        }

        // close the chip interface
        chipInterface->ciClose();
        delete chipInterface;
        chipInterface = NULL;

        return 0;
    }

    // if came here, we should run this app as the main local core
    return runCore(0, true);
}

void pthread_kill_join(const char* threadName, pthread_t& threadInfo)
{
    printf("Stoping %s thread\n", threadName);
    pthread_kill(threadInfo, SIGINT);           // stop the select()
    pthread_join(threadInfo, NULL);             // wait until thread finishes
}

// instanceNo: number of core instance, used for separating folders and ports
// localNotNetwork: if true, will access local hardware for communication; if false then will use network interface
int runCore(int instanceNo, bool localNotNetwork)
{
    CCoreThread *core;
    pthread_t   ikbdThreadInfo;
    pthread_t   floppyEncThreadInfo;
    pthread_t   displayThreadInfo;
    pthread_t   cmdSockThreadInfo;

    flags.localNotNetwork = localNotNetwork;            // store if this core runs as local device or as network server
    flags.instanceNo = instanceNo;

    //------------------------------------
    // if should run this core as network server
    if(!localNotNetwork) {
        Debug::out(LOG_INFO, "runCore as network server instance # %d", instanceNo);

        hwConfig.version = 3;
        flags.noReset = true;

        chipInterface = new ChipInterfaceNetwork();     // create network chip interface
        chipInterface->setInstanceIndex(instanceNo);    // set index of this instance
        chipInterface->ciOpen();                        // try to open it
    }

    //------------------------------------
    // normal app run follows
    Debug::printfLogLevelString();

    char appVersion[16];
    Version::getAppVersion(appVersion);
    Debug::out(LOG_INFO, "CosmosEx core starting, version: %s", appVersion);
    printf("\nCosmosEx core starting, version: %s\n", appVersion);

    Version::getRaspberryPiInfo();                                  // fetch model, revision, serial of RPi

//  system("sudo echo none > /sys/class/leds/led0/trigger");        // disable usage of GPIO 23 (pin 16) by LED

    Utils::setTimezoneVariable_inThisContext();

    //-------------
    core = new CCoreThread();

    // display thread - only on RPi 2 or newer (RPi 1 doesn't have the extra GPIO pins and this then kills eth + usb on RPi1)
    bool hasDisplay = rpiConfig.revisionInt >= 0xa01041;

    handlePthreadCreate("ikbd", &ikbdThreadInfo, (void*) ikbdThreadCode);
    handlePthreadCreate("floppy encode", &floppyEncThreadInfo, (void*) floppyEncodeThreadCode);
    handlePthreadCreate("command socket", &cmdSockThreadInfo, (void*) cmdSockThreadCode);

    if(hasDisplay) {
        Debug::out(LOG_INFO, "Running on RPi 2 or newer (%x), starting displayThread.", rpiConfig.revisionInt);
        handlePthreadCreate("display", &displayThreadInfo, (void*) displayThreadCode);
    } else {
        Debug::out(LOG_INFO, "Running on RPi 1 (%x), not starting displayThread.", rpiConfig.revisionInt);
    }

    printf("Entering main loop...\n");

    core->run();                // run the main thread

    printf("\n\nExit from main loop\n");

    delete core;

    floppyEncoder_stop();
    pthread_kill_join("ikbd", ikbdThreadInfo);
    pthread_kill_join("floppy encoder", floppyEncThreadInfo);
    pthread_kill_join("command socket", cmdSockThreadInfo);

    if(hasDisplay) {             // kill display thread
        pthread_kill_join("display", displayThreadInfo);
    }

    //---------------------------------------------------
    // Closing of GPIO should be done after stopping IKBD thread and DISPLAY thread
    // as they also use some GPIO pins and we want them to be able to use them until the end.
    chipInterface->ciClose();                           // close gpio
    delete chipInterface;
    chipInterface = NULL;
    //---------------------------------------------------

    if(singleInstanceSocketFd > 0) {                    // if we got the single instance socket, close it
        close(singleInstanceSocketFd);
    }

    // remove PID file on termination
    std::string pidFilePath = Utils::dotEnvValue("CORE_PID_FILE", "/var/run/ce/core.pid");
    unlink(pidFilePath.c_str());

    Debug::out(LOG_INFO, "CosmosEx terminated.");
    printf("Terminated\n");
    return 0;
}

void loadLastHwConfig(void)
{
    Settings s;

    hwConfig.version        = s.getInt("HW_VERSION",       1);
    hwConfig.hddIface       = s.getInt("HW_HDD_IFACE",     HDD_IF_ACSI);
    hwConfig.scsiMachine    = s.getInt("HW_SCSI_MACHINE",  SCSI_MACHINE_UNKNOWN);
    hwConfig.fwMismatch     = false;
    hwConfig.changed        = false;

    memset(hwConfig.hwSerial, 0, 13);
    hwConfig.hwLicenseValid = false;
}

void initializeFlags(void)
{
    flags.justShowHelp = false;
    Debug::setLogLevel(LOG_ERROR);      // init current log level to LOG_ERROR
    flags.chipInterface = CHIPIF_UNKNOWN; // start with unknown chip interface
    flags.justDoReset  = false;         // if shouldn't run the app, but just reset Hans and Franz (used with STM32 ST-Link JTAG)
    flags.noReset      = false;         // don't reset Hans and Franz on start - used with STM32 ST-Link JTAG
    flags.test         = false;         // if set to true, set ACSI ID 0 to translated, ACSI ID 1 to SD, and load floppy with some image
    flags.getHwInfo    = false;         // if set to true, wait for HW info from Hans, and then quit and report it
    flags.noFranz      = false;         // if set to true, won't communicate with Franz
    flags.ikbdLogs     = false;         // no ikbd logs by default
    flags.fakeOldApp   = false;         // don't fake old app by default
    flags.display      = false;         // if set to true, show string on front display, if possible
    flags.noCapture    = false;         // if true, don't do exclusive mouse and keyboard capture

    flags.localNotNetwork   = true;     // if true, this app runs handling localy connected device; if false then this core is part of the network server
    flags.instanceNo        = 0;

    flags.deviceGetLicense  = false;    // if true, device should get license again
    flags.deviceDoUpdate    = false;    // if true, device should download update and write it to flash

    flags.gotHansFwVersion  = false;
    flags.gotFranzFwVersion = false;
}

void loadDefaultArgumentsFromFile(void)
{
    FILE *f = fopen("/ce/default_args", "rt");  // try to open default args file

    if(!f) {
        printf("No default app arguments.\n");
        return;
    }

    char args[1024];
    memset(args, 0, 1024);
    fgets(args, 1024, f);                       // read line from file
    fclose(f);

    int argc = 0;
    char *argv[64];
    memset(argv, 0, 64 * 4);                    // clear the future argv field

    argv[0] = (char *) "fake.exe";              // 0th argument is skipped, as it's usualy file name (when passed to main())
    argc    = 1;
    argv[1] = &args[0];                         // 1st argument starts at the start of the line

    int len = strlen(args);
    for(int i=0; i<len; i++) {                  // go through the arguments line
        if(args[i] == ' ' || args[i]=='\n' || args[i] == '\r') {    // if found space or new line...
            args[i] = 0;                        // convert space to string terminator

            argc++;
            if(argc >= 64) {                    // would be out of bondaries? quit
                break;
            }

            argv[argc] = &args[i + 1];          // store pointer to next string, increment count
        }
    }

    if(len > 0) {                               // the alg above can't detect last argument, so just increment argument count
        argc++;
    }

    parseCmdLineArguments(argc, argv);
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

        // it's a CHIP INTERFACE command (ci)
        if(strncmp(argv[i], "ci", 2) == 0) {
            isKnownTag = true;                                      // this is a known tag
            int ci;

            ci = (int) argv[i][2];

            if(ci >= 48 && ci <= 57) {                              // if it's a number between 0 and 9
                ci = ci - 48;

                switch(ci) {
                    case 1:
                    case 2: flags.chipInterface = CHIPIF_V1_V2;     break;  // 1 or 2 means old SPI interface
                    case 3: flags.chipInterface = CHIPIF_V3;        break;  // 3 means new SPI interface

                    case 0: flags.chipInterface = CHIPIF_DUMMY;     break;  // 0 for dummy interface
                    case 9: flags.chipInterface = CHIPIF_NETWORK;   break;  // 9 for network server
                }
            }

            continue;
        }

        if(strcmp(argv[i], "help") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "/?") == 0 || strcmp(argv[i], "?") == 0) {
            isKnownTag          = true;                             // this is a known tag
            flags.justShowHelp  = true;
            continue;
        }

        // should we just reset Hans and Franz and quit? (used with STM32 ST-Link JTAG)
        if(strcmp(argv[i], "reset") == 0) {
            isKnownTag          = true;                             // this is a known tag
            flags.justDoReset   = true;
            continue;
        }

        // don't resetHans and Franz on start (used with STM32 ST-Link JTAG)
        if(strcmp(argv[i], "noreset") == 0) {
            isKnownTag          = true;                             // this is a known tag
            flags.noReset       = true;
            continue;
        }

        // for testing purposes: set ACSI ID 0 to translated, ACSI ID 1 to SD, and load floppy with some image
        if(strcmp(argv[i], "test") == 0) {
            printf("Testing setup active!\n");
            isKnownTag          = true;                             // this is a known tag
            flags.test          = true;
            continue;
        }

        // get hardware version and HDD interface type
        if(strcmp(argv[i], "hwinfo") == 0) {
            isKnownTag          = true;                             // this is a known tag
            flags.getHwInfo     = true;
        }

        // run the device without communicating with Franz
        if(strcmp(argv[i], "nofranz") == 0) {
            isKnownTag          = true;                             // this is a known tag
            flags.noFranz       = true;
        }

        // produce ikbd logs
        if(strcmp(argv[i], "ikbdlogs") == 0) {
            isKnownTag          = true;                             // this is a known tag
            flags.ikbdLogs      = true;
        }

        // should fake old app version? (for reinstall tests)
        if(strcmp(argv[i], "fakeold") == 0) {
            isKnownTag          = true;                             // this is a known tag
            flags.fakeOldApp    = true;
        }

        if(strcmp(argv[i], "display") == 0) {
            isKnownTag          = true;                             // this is a known tag
            flags.display       = true;
        }

        // don't capture USB mouse and keyboard
        if(strcmp(argv[i], "nocap") == 0) {
            isKnownTag          = true;                             // this is a known tag
            flags.noCapture     = true;
        }

        if(!isKnownTag) {                                           // if tag unknown, show warning
            printf(">>> UNKNOWN APP ARGUMENT: '%s' <<<\n", argv[i]);
        }
    }
}

void printfPossibleCmdLineArgs(void)
{
    printf("\nPossible command line args:\n");
    printf("reset    - reset Hans and Franz, release lines, quit\n");
    printf("noreset  - when starting, don't reset Hans and Franz\n");
    printf("llx      - set log level to x (default is 1, max is 4)\n");
    printf("cix      - set chip interface type to x (1 & 2 for old SPI, 3 for new SPI, 9 for network server, 0 for dummy)\n");
    printf("test     - some default config for device testing\n");
    printf("ce_conf  - use this app as ce_conf on RPi (the app must be running normally, too)\n");
    printf("hwinfo   - get HW version and HDD interface type\n");
    printf("ikbdlogs - write IKBD logs to file\n");
    printf("fakeold  - fake old app version for reinstall tests\n");
    printf("display  - show string on front display, if possible\n");
    printf("nocap    - don't do exclusive USB mouse and keyboard capture\n");
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
    std::string pidFilePath = Utils::dotEnvValue("CORE_PID_FILE", "/var/run/ce/core.pid");

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

void showOnDisplay(int argc, char *argv[])
{
    if(argc != 3) {
        printf("To use display command: %s display 'Your message'\n", argv[0]);
        return;
    }

    display_init();
    display_print_center(argv[2]);
    display_deinit();
}
