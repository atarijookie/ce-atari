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

#include "config/configstream.h"
#include "settings.h"
#include "global.h"
#include "ccorethread.h"
#include "debug.h"
#include "ikbd/ikbd.h"
#include "update.h"
#include "version.h"
#include "periodicthread.h"
#include "display/displaythread.h"
#include "floppy/floppyencoder.h"
#include "chipinterface_v1_v2/chipinterface12.h"
#include "chipinterface_network/chipinterfacenetwork.h"

volatile sig_atomic_t sigintReceived = 0;
void sigint_handler(int sig);

void handlePthreadCreate(int res, const char *threadName, pthread_t *pThread);
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

const char *distroString = "Raspbian";

bool otherInstanceIsRunning(void);
int  singleInstanceSocketFd;

void showOnDisplay(int argc, char *argv[]);
int  runCore(int instanceNo, bool localNotNetwork);
void networkServerMain(void);

int main(int argc, char *argv[])
{
    pthread_mutex_init(&shared.mtxScsi,   NULL);
    pthread_mutex_init(&shared.mtxImages, NULL);

    printf("\033[H\033[2J\n");

    initializeFlags();                                          // initialize flags

    Utils::loadDotEnv();                                        // load dotEnv before setting default log file
    Debug::setDefaultLogFile();

    loadDefaultArgumentsFromFile();                             // first parse default arguments from file
    parseCmdLineArguments(argc, argv);                          // then parse cmd line arguments and set global variables

    Debug::printfLogLevelString();

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

    if(flags.chipInterface == CHIPIF_UNKNOWN) {                 // unknown chip interface? do the auto detection
        Debug::out(LOG_INFO, "ChipInterface auto-detect: trying v1/v2");

        chipInterface = new ChipInterface12();                  // create chip interface v1/v2
        hwConfig.version = 2;

        good = chipInterface->ciOpen();                         // try to open chip interface v1/v2

        if(!good) {
            Debug::out(LOG_INFO, "ChipInterface auto-detect: v1/v2 failed, terminating");
        } else {
            Debug::out(LOG_INFO, "ChipInterface auto-detect: v1/v2 opened, continuing.");
            printf("\nChipInterface auto-detect: v1/v2 opened\n");
        }
    } else if(flags.chipInterface == CHIPIF_NETWORK) {
        Debug::out(LOG_INFO, "ChipInterface: starting NETWORK server");
        networkServerMain();
        return 0;
    } else {
        Debug::out(LOG_INFO, "ChipInterface - unknown option, terminating.");
        printf("\nChipInterface - unknown option, terminating.\n");
        good = false;
    }

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

// instanceNo: number of core instance, used for separating folders and ports
// localNotNetwork: if true, will access local hardware for communication; if false then will use network interface
int runCore(int instanceNo, bool localNotNetwork)
{
    CCoreThread *core;
    pthread_t   ikbdThreadInfo;
    pthread_t   floppyEncThreadInfo;
    pthread_t   periodicThreadInfo;
    pthread_t   displayThreadInfo;

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
    printf("\nCosmosEx main app starting on %s...\n", distroString);

    char appVersion[16];
    Version::getAppVersion(appVersion);

    Debug::out(LOG_INFO, "\n\n---------------------------------------------------");
    Debug::out(LOG_INFO, "CosmosEx starting, version: %s", appVersion);

    Version::getRaspberryPiInfo();                                  // fetch model, revision, serial of RPi

    if(localNotNetwork) {
        Update::createNewScripts();                                 // update the scripts if needed
    }

//  system("sudo echo none > /sys/class/leds/led0/trigger");        // disable usage of GPIO 23 (pin 16) by LED

    printf("Starting threads\n");

    Utils::setTimezoneVariable_inThisContext();

    //-------------
    // Copy the configdrive to /tmp so we can change the content as needed.
    // This must be done before new CCoreThread because it reads the data from /tmp/configdrive
    system("rm -rf /tmp/configdrive");                      // remove any old content
    system("mkdir /tmp/configdrive");                       // create dir
    system("cp -r /ce/app/configdrive/* /tmp/configdrive"); // copy new content
    //-------------
    core = new CCoreThread();
    int res;

    res = pthread_create(&ikbdThreadInfo, NULL, ikbdThreadCode, NULL);          // create the keyboard emulation thread and run it
    handlePthreadCreate(res, "ce ikbd", &ikbdThreadInfo);

    res = pthread_create(&floppyEncThreadInfo, NULL, floppyEncodeThreadCode, NULL); // create the floppy encoding thread and run it
    handlePthreadCreate(res, "ce floppy encode", &floppyEncThreadInfo);

    res = pthread_create(&periodicThreadInfo, NULL, periodicThreadCode, NULL);  // create the periodic thread and run it
    handlePthreadCreate(res, "periodic", &periodicThreadInfo);

    if(rpiConfig.revisionInt >= 0xa01041) {   // display thread - only on RPi 2 or newer (RPi 1 doesn't have the extra GPIO pins and this then kills eth + usb on RPi1)
        Debug::out(LOG_INFO, "Running on RPi 2 or newer (%x), starting displayThread.", rpiConfig.revisionInt);

        res = pthread_create(&displayThreadInfo, NULL, displayThreadCode, NULL);  // create the display thread and run it
        handlePthreadCreate(res, "display", &displayThreadInfo);
    } else {
        Debug::out(LOG_INFO, "Running on RPi 1 (%x), not starting displayThread.", rpiConfig.revisionInt);
    }

    printf("Entering main loop...\n");

    core->run();                // run the main thread

    printf("\n\nExit from main loop\n");

    delete core;

    printf("Stoping ikbd thread\n");
    pthread_kill(ikbdThreadInfo, SIGINT);               // stop the select()
    pthread_join(ikbdThreadInfo, NULL);                 // wait until ikbd      thread finishes

    printf("Stoping floppy encoder thread\n");
    floppyEncoder_stop();
    pthread_join(floppyEncThreadInfo, NULL);            // wait until floppy encode thread finishes

    printf("Stoping periodic thread\n");
    pthread_kill(periodicThreadInfo, SIGINT);           // stop the select()
    pthread_join(periodicThreadInfo, NULL);             // wait until periodic  thread finishes

    if(rpiConfig.revisionInt >= 0xa01041) {             // kill display thread - only on RPi 2 or newer
        printf("Stopping display thread\n");
        pthread_kill(displayThreadInfo, SIGINT);        // stop the select()
        pthread_join(displayThreadInfo, NULL);          // wait until display thread finishes
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
    flags.chipInterface = CHIPIF_UNKNOWN; // start with unknown chip interface, this will do the auto-detect atttempt
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
                    case 2: flags.chipInterface = CHIPIF_V1_V2;     break;  // 1 or 2 means SPI interface

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
    printf("llx      - set log level to x (default is 1, max is 3)\n");
    printf("cix      - set chip interface type to x (1 & 2 mean SPI, 9 means network server)\n");
    printf("test     - some default config for device testing\n");
    printf("ce_conf  - use this app as ce_conf on RPi (the app must be running normally, too)\n");
    printf("hwinfo   - get HW version and HDD interface type\n");
    printf("ikbdlogs - write IKBD logs to /var/log/ikbdlog.txt\n");
    printf("fakeold  - fake old app version for reinstall tests\n");
    printf("display  - show string on front display, if possible\n");
    printf("nocap    - don't do exclusive USB mouse and keyboard capture\n");
}

void handlePthreadCreate(int res, const char *threadName, pthread_t *pThread)
{
    if(res != 0) {
        Debug::out(LOG_ERROR, "Failed to create %s thread, %s won't work...", threadName, threadName);
    } else {
        Debug::out(LOG_DEBUG, "%s thread created", threadName);
        pthread_setname_np(*pThread, threadName);
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
