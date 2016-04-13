#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <pty.h>
 
#include "settings.h"
#include "global.h"
#include "ccorethread.h"
#include "gpio.h"
#include "debug.h"
#include "version.h"

volatile sig_atomic_t sigintReceived = 0;
void sigint_handler(int sig);

void parseCmdLineArguments(int argc, char *argv[]);
void printfPossibleCmdLineArgs(void);
void loadDefaultArgumentsFromFile(void);

void loadLastHwConfig(void);
THwConfig hwConfig;                                             // info about the current HW setup

TFlags flags;                                                   // global flags from command line
void initializeFlags(void);

InterProcessEvents events;

DWORD appStartMs;

// if needed, replace with  LOG_DEBUG
#define DEFAULT_LOG_LEVEL   LOG_INFO

int main(int argc, char *argv[])
{
    CCoreThread *core;

    appStartMs = Utils::getCurrentMs();
    
    initializeFlags();                                          // initialize flags 
    Debug::setDefaultLogFile();
    loadDefaultArgumentsFromFile();                             // first parse default arguments from file
    loadLastHwConfig();                                         // load last found HW IF, HW version, SCSI machine
    
    printf("CosmosEx preinit app starting...\n");
    //------------------------------------
    if(signal(SIGINT, sigint_handler) == SIG_ERR) {		    // register SIGINT handler
        printf("Cannot register SIGINT handler!\n");
    }

    if(signal(SIGHUP, sigint_handler) == SIG_ERR) {		    // register SIGHUP handler
        printf("Cannot register SIGHUP handler!\n");
    }

    //------------------------------------
    // normal app run follows
    Debug::printfLogLevelString();

    char appVersion[16];
    Version::getAppVersion(appVersion);
    
    Debug::out(LOG_INFO, "\n\n---------------------------------------------------");
    Debug::out(LOG_INFO, "CosmosEx preinit starting, version: %s", appVersion);

//	system("sudo echo none > /sys/class/leds/led0/trigger");	    // disable usage of GPIO 23 (pin 16) by LED

	if(!gpio_open()) {									            // try to open GPIO and SPI on RPi
		return 0;
	}

    //-------------
    // Copy the configdrive to /tmp so we can change the content as needed.
    // This must be done before new CCoreThread because it reads the data from /tmp/configdrive 
    system("rm -rf /tmp/configdrive");                      // remove any old content
    system("mkdir /tmp/configdrive");                       // create dir
    system("cp -r /ce/app/configdrive/* /tmp/configdrive"); // copy new content
    //-------------
    core = new CCoreThread();
    
	core->run();										// run the main thread

	delete core;
	gpio_close();										// close gpio and spi

    Debug::out(LOG_INFO, "CosmosEx preinit terminated.");
    printf("Preinit terminated\n");
    return 0;
}

void loadLastHwConfig(void)
{
    Settings s;
    
    hwConfig.version        = s.getInt((char *) "HW_VERSION",       1);
    hwConfig.hddIface       = s.getInt((char *) "HW_HDD_IFACE",     HDD_IF_ACSI);
    hwConfig.scsiMachine    = s.getInt((char *) "HW_SCSI_MACHINE",  SCSI_MACHINE_UNKNOWN);
    hwConfig.fwMismatch     = false;
}

void initializeFlags(void)
{
    flags.justShowHelp = false;
    flags.logLevel     = DEFAULT_LOG_LEVEL;
    flags.justDoReset  = false;         // if shouldn't run the app, but just reset Hans and Franz (used with STM32 ST-Link JTAG)
    flags.noReset      = false;         // don't reset Hans and Franz on start - used with STM32 ST-Link JTAG
    flags.test         = false;         // if set to true, set ACSI ID 0 to translated, ACSI ID 1 to SD, and load floppy with some image
    flags.actAsCeConf  = false;         // if set to true, this app will behave as ce_conf app instead of CosmosEx app
    flags.getHwInfo    = false;         // if set to true, wait for HW info from Hans, and then quit and report it
    flags.noFranz      = false;         // if set to true, won't communicate with Franz
    flags.ikbdLogs     = false;         // no ikbd logs by default
    
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

                if(ll > LOG_DEBUG) {                                // would be higher than highest log level? fix it
                    ll = LOG_DEBUG;
                }

                flags.logLevel = ll;                                // store log level
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
        
        // for running this app as ce_conf terminal
        if(strcmp(argv[i], "ce_conf") == 0) {
            isKnownTag          = true;                             // this is a known tag
            flags.actAsCeConf   = true;
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
        
        // run the device without communicating with Franz
        if(strcmp(argv[i], "ikbdlogs") == 0) {
            isKnownTag          = true;                             // this is a known tag
            flags.ikbdLogs      = true;
        }
        
        if(!isKnownTag) {                                           // if tag unknown, show warning
            printf(">>> UNKNOWN APP ARGUMENT: '%s' <<<\n", argv[i]);
        }
    }
}

void sigint_handler(int sig)
{
    Debug::out(LOG_DEBUG, "Some SIGNAL received, terminating.");
	sigintReceived = 1;
}
