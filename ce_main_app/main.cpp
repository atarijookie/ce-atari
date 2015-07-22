#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <pty.h>
 
#include "config/configstream.h"
#include "settings.h"
#include "global.h"
#include "ccorethread.h"
#include "gpio.h"
#include "debug.h"
#include "mounter.h"
#include "downloader.h"
#include "ikbd.h"
#include "timesync.h"
#include "version.h"
#include "ce_conf_on_rpi.h"
#include "network/netadapter.h"

#include "webserver/webserver.h"
#include "webserver/api/apimodule.h"
#include "webserver/app/appmodule.h"
#include "service/virtualkeyboardservice.h"
#include "service/virtualmouseservice.h"
#include "service/configservice.h"
#include "service/screencastservice.h"

volatile sig_atomic_t sigintReceived = 0;
void sigint_handler(int sig);

void handlePthreadCreate(int res, char *what);
void parseCmdLineArguments(int argc, char *argv[]);
void printfPossibleCmdLineArgs(void);

int     linuxConsole_fdMaster, linuxConsole_fdSlave;            // file descriptors for pty pair
pid_t   childPid;                                               // pid of forked child

void loadLastHwConfig(void);
THwConfig hwConfig;                                             // info about the current HW setup

TFlags flags;                                                   // global flags from command line
void initializeFlags(void);

int main(int argc, char *argv[])
{
    CCoreThread *core;
    pthread_t	mountThreadInfo;
    pthread_t	downloadThreadInfo;
    pthread_t	ikbdThreadInfo;
    pthread_t	floppyEncThreadInfo;
	pthread_t	timesyncThreadInfo;
    pthread_t   networkThreadInfo;

    initializeFlags();                                          // initialize flags 
    
    Debug::setDefaultLogFile();
    parseCmdLineArguments(argc, argv);                          // parse cmd line arguments and set global variables

    loadLastHwConfig();                                         // load last found HW IF, HW version, SCSI machine
    //------------------------------------
    // if not running as ce_conf, register signal handlers
    if(!flags.actAsCeConf) {                                        
        if(signal(SIGINT, sigint_handler) == SIG_ERR) {		    // register SIGINT handler
            printf("Cannot register SIGINT handler!\n");
        }

        if(signal(SIGHUP, sigint_handler) == SIG_ERR) {		    // register SIGHUP handler
            printf("Cannot register SIGHUP handler!\n");
        }
    }

    //------------------------------------
    // if this is not just a reset command AND not a get HW info command
    if(!flags.justDoReset && !flags.getHwInfo) {                                
        int ires = openpty(&linuxConsole_fdMaster, &linuxConsole_fdSlave, NULL, NULL, NULL);    // open PTY pair

        if(ires != -1) {                                                // if openpty() was OK  
            childPid = fork();

            if(childPid == 0) {                                         // code executed only by child
                dup2(linuxConsole_fdSlave, 0);
                dup2(linuxConsole_fdSlave, 1);        
                dup2(linuxConsole_fdSlave, 2);

                char *shell = (char *) "/bin/sh";
                execlp(shell, shell, (char *) NULL);
    
                return 0;
            }
        }
    }
    
    //------------------------------------
    // if should run as ce_conf app, do this code instead
    if(flags.actAsCeConf) {                                         
        printf("CE_CONF tool - Raspberry Pi version.\nPress Ctrl+C to quit.\n");
        
        Debug::setLogFile((char *) "/var/log/ce_conf.log");
        ce_conf_mainLoop();
        return 0;
    }
    
    //------------------------------------
    // if should just get HW info, do a shorter / simpler version of app run
    if(flags.getHwInfo) {                                           
    	if(!gpio_open()) {									    // try to open GPIO and SPI on RPi
            printf("\nHW_VER: UNKNOWN\n");
            printf("\nHDD_IF: UNKNOWN\n");
            return 0;
        }

        Debug::out(LOG_ERROR, ">>> Starting app as HW INFO tool <<<\n");

        core = new CCoreThread(NULL, NULL, NULL);               // create main thread
        core->run();										    // run the main thread
        
		gpio_close();
		return 0;
    }
    
    //------------------------------------
    // normal app run follows
    printfPossibleCmdLineArgs();
    Debug::printfLogLevelString();

    char appVersion[16];
    Version::getAppVersion(appVersion);
    
    Debug::out(LOG_ERROR, "\n\n---------------------------------------------------");
    Debug::out(LOG_ERROR, "CosmosEx starting, version: %s", appVersion);

//	system("sudo echo none > /sys/class/leds/led0/trigger");	// disable usage of GPIO 23 (pin 16) by LED

    ce_conf_createFifos();                                      // if should run normally, create the ce_conf FIFOs

	if(!gpio_open()) {									        // try to open GPIO and SPI on RPi
		return 0;
	}

	if(flags.justDoReset) {                                         // is this a reset command? (used with STM32 ST-Link debugger)
		Utils::resetHansAndFranz();
		gpio_close();

		printf("\nJust did reset and quit...\n");

		return 0;
	}

    Utils::setTimezoneVariable_inThisContext();
    
    Downloader::initBeforeThreads();

    //start up virtual devices
    VirtualKeyboardService* pxVKbdService=new VirtualKeyboardService();
    VirtualMouseService* pxVMouseService=new VirtualMouseService();
    pxVKbdService->start();
    pxVMouseService->start();

    //start up date service
    ConfigService* pxDateService=new ConfigService();
    pxDateService->start();

    //start up floppy service
    FloppyService* pxFloppyService=new FloppyService();
    pxFloppyService->start();

    //start up screencast service
    ScreencastService* pxScreencastService=new ScreencastService();
    pxScreencastService->start();

    //this runs its own thread
    WebServer xServer;
    xServer.addModule(new ApiModule(pxVKbdService,pxVMouseService,pxFloppyService));
    xServer.addModule(new AppModule(pxDateService,pxFloppyService,pxScreencastService));
    xServer.start();

    //-------------
    // Copy the configdrive to /tmp so we can change the content as needed.
    // This must be done before new CCoreThread because it reads the data from /tmp/configdrive 
    system("rm -rf /tmp/configdrive");                      // remove any old content
    system("mkdir /tmp/configdrive");                       // create dir
    system("cp /ce/app/configdrive/* /tmp/configdrive");    // copy new content
    //-------------

    core = new CCoreThread(pxDateService,pxFloppyService,pxScreencastService);

	int res = pthread_create( &mountThreadInfo, NULL, mountThreadCode, NULL);	// create mount thread and run it
	handlePthreadCreate(res, (char *) "mount");

    res = pthread_create(&downloadThreadInfo, NULL, downloadThreadCode, NULL);  // create download thread and run it
	handlePthreadCreate(res, (char *) "download");

    res = pthread_create(&ikbdThreadInfo, NULL, ikbdThreadCode, NULL);			// create the keyboard emulation thread and run it
	handlePthreadCreate(res, (char *) "ikbd");

    res = pthread_create(&floppyEncThreadInfo, NULL, floppyEncodeThreadCode, NULL);	// create the floppy encoding thread and run it
	handlePthreadCreate(res, (char *) "floppy encode");

    res = pthread_create(&timesyncThreadInfo, NULL, timesyncThreadCode, NULL);  // create the timesync thread and run it
	handlePthreadCreate(res, (char *) "time sync");

    res = pthread_create(&networkThreadInfo, NULL, networkThreadCode, NULL);    // create the network thread and run it
	handlePthreadCreate(res, (char *) "network");

	core->run();										// run the main thread

    xServer.stop();

    pxScreencastService->stop();
    delete pxScreencastService;

    pxFloppyService->stop();
    delete pxFloppyService;

    pxDateService->stop();
    delete pxDateService;

    pxVKbdService->stop();
    pxVMouseService->stop();
    delete pxVKbdService;
    delete pxVMouseService;

	delete core;
	gpio_close();										// close gpio and spi

	pthread_join(mountThreadInfo, NULL);				// wait until mount     thread finishes
    pthread_join(downloadThreadInfo, NULL);             // wait until download  thread finishes
    pthread_join(ikbdThreadInfo, NULL);                 // wait until ikbd      thread finishes
    pthread_join(floppyEncThreadInfo, NULL);            // wait until floppy encode thread finishes
    pthread_join(timesyncThreadInfo, NULL);             // wait until timesync  thread finishes
    pthread_join(networkThreadInfo, NULL);              // wait until network   thread finishes

    Downloader::cleanupBeforeQuit();

    Debug::out(LOG_INFO, "CosmosEx terminated.");
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
    flags.logLevel     = LOG_ERROR;     // init current log level to LOG_ERROR
    flags.justDoReset  = false;         // if shouldn't run the app, but just reset Hans and Franz (used with STM32 ST-Link JTAG)
    flags.noReset      = false;         // don't reset Hans and Franz on start - used with STM32 ST-Link JTAG
    flags.test         = false;         // if set to true, set ACSI ID 0 to translated, ACSI ID 1 to SD, and load floppy with some image
    flags.actAsCeConf  = false;         // if set to true, this app will behave as ce_conf app instead of CosmosEx app
    flags.getHwInfo    = false;         // if set to true, wait for HW info from Hans, and then quit and report it
    flags.noFranz      = false;         // if set to true, won't communicate with Franz
    
    flags.gotHansFwVersion  = false;
    flags.gotFranzFwVersion = false;
}

void parseCmdLineArguments(int argc, char *argv[])
{
    int i;

    for(i=1; i<argc; i++) {                                         // go through all params (when i=0, it's app name, not param)
        // it's a LOG LEVEL change command (ll)
        if(strncmp(argv[i], "ll", 2) == 0) {
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

        // should we just reset Hans and Franz and quit? (used with STM32 ST-Link JTAG)
        if(strcmp(argv[i], "reset") == 0) {
            flags.justDoReset = true;
            continue;
        }

        // don't resetHans and Franz on start (used with STM32 ST-Link JTAG)
        if(strcmp(argv[i], "noreset") == 0) {
            flags.noReset = true;
            continue;
        }

        // for testing purposes: set ACSI ID 0 to translated, ACSI ID 1 to SD, and load floppy with some image
        if(strcmp(argv[i], "test") == 0) {
            printf("Testing setup active!\n");
            flags.test = true;
            continue;
        }
        
        // for running this app as ce_conf terminal
        if(strcmp(argv[i], "ce_conf") == 0) {
            flags.actAsCeConf = true;
        }

        // get hardware version and HDD interface type
        if(strcmp(argv[i], "hwinfo") == 0) {
            flags.getHwInfo = true;
        }

        // run the device without communicating with Franz
        if(strcmp(argv[i], "nofranz") == 0) {
            flags.noFranz = true;
        }
    }
}

void printfPossibleCmdLineArgs(void)
{
    printf("\nPossible command line args:\n");
    printf("reset   - reset Hans and Franz, release lines, quit\n");
    printf("noreset - when starting, don't reset Hans and Franz\n");
    printf("llx     - set log level to x (default is 1, max is 3)\n");
    printf("test    - some default config for device testing\n");
    printf("ce_conf - use this app as ce_conf on RPi (the app must be running normally, too)\n");
    printf("hwinfo  - get HW version and HDD interface type\n");
}

void handlePthreadCreate(int res, char *what)
{
    if(res != 0) {
        Debug::out(LOG_ERROR, "Failed to create %s thread, %s won't work...", what, what);
	} else {
		Debug::out(LOG_INFO, "%s thread created", what);
	}
}

void sigint_handler(int sig)
{
    Debug::out(LOG_INFO, "Some SIGNAL received, terminating.");
	sigintReceived = 1;
    
    if(childPid != 0) {             // in case we fork()ed, kill the child
        Debug::out(LOG_INFO, "Killing child with pid %d\n", childPid);
        kill(childPid, SIGKILL);
    }
}

