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
 
#include "config/configstream.h"
#include "settings.h"
#include "global.h"
#include "ccorethread.h"
#include "gpio.h"
#include "debug.h"
#include "mounter.h"
#include "downloader.h"
#include "ikbd/ikbd.h"
#include "update.h"
#include "version.h"
#include "ce_conf_on_rpi.h"
#include "network/netadapter.h"
#include "periodicthread.h"

#include "webserver/webserver.h"
#include "webserver/api/apimodule.h"
#include "webserver/app/appmodule.h"
#include "service/virtualkeyboardservice.h"
#include "service/virtualmouseservice.h"
#include "service/configservice.h"
#include "service/screencastservice.h"

volatile sig_atomic_t sigintReceived = 0;
void sigint_handler(int sig);

void handlePthreadCreate(int res, const char *threadName, pthread_t *pThread);
void parseCmdLineArguments(int argc, char *argv[]);
void printfPossibleCmdLineArgs(void);
void loadDefaultArgumentsFromFile(void);

int     linuxConsole_fdMaster;                                  // file descriptors for linux console
pid_t   childPid;                                               // pid of forked child

void loadLastHwConfig(void);
void initializeFlags(void);

THwConfig           hwConfig;                           // info about the current HW setup
TFlags              flags;                              // global flags from command line
RPiConfig           rpiConfig;                          // RPi model, revision, serial
InterProcessEvents  events;
SharedObjects       shared;

#ifdef DISTRO_YOCTO
const char *distroString = "Yocto";
#else
const char *distroString = "Raspbian";
#endif

bool otherInstanceIsRunning(void);
int  singleInstanceSocketFd;

int main(int argc, char *argv[])
{
    CCoreThread *core;
    pthread_t	mountThreadInfo;
    pthread_t	downloadThreadInfo;
#ifndef ONPC_NOTHING
    pthread_t	ikbdThreadInfo;
#endif
    pthread_t	floppyEncThreadInfo;
    pthread_t   networkThreadInfo;
    pthread_t   periodicThreadInfo;

    pthread_mutex_init(&shared.mtxScsi,             NULL);
    pthread_mutex_init(&shared.mtxTranslated,       NULL);
    pthread_mutex_init(&shared.mtxConfigStreams,    NULL);
    
    printf("\033[H\033[2J\n");

    initializeFlags();                                          // initialize flags 
    
    Debug::setDefaultLogFile();
    
    loadDefaultArgumentsFromFile();                             // first parse default arguments from file
    parseCmdLineArguments(argc, argv);                          // then parse cmd line arguments and set global variables

    if(flags.justShowHelp) {
        printfPossibleCmdLineArgs();
        return 0;
    }
    
    loadLastHwConfig();                                         // load last found HW IF, HW version, SCSI machine

    printf("CosmosEx main app starting on %s...\n", distroString);
    
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
        childPid = forkpty(&linuxConsole_fdMaster, NULL, NULL, NULL);

        if(childPid == 0) {                                         // code executed only by child
            const char *shell = "/bin/sh";  // default shell
            const char *term = "vt52";
            shell = getenv("SHELL");
            if(access("/etc/terminfo/a/atari", R_OK) == 0)
                term = "atari";
            if(setenv("TERM", term, 1) < 0) {
                fprintf(stderr, "Failed to setenv(\"TERM\", \"%s\"): %s\n", term, strerror(errno));
            }
            execlp(shell, shell, "-i", (char *) NULL);  // -i for interactive

            return 0;
        } 
        
        // parent (full app) continues here
    }
    
    //------------------------------------
    // if should run as ce_conf app, do this code instead
    if(flags.actAsCeConf) {                                         
        printf("CE_CONF tool - Raspberry Pi version.\nPress Ctrl+C to quit.\n");
        
        Debug::setLogFile("/var/log/ce_conf.log");
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

        Debug::out(LOG_INFO, ">>> Starting app as HW INFO tool <<<\n");

        core = new CCoreThread(NULL, NULL, NULL);               // create main thread
        core->run();										    // run the main thread
        
		gpio_close();
		return 0;
    }
    
    //------------------------------------
    // normal app run follows
    Debug::printfLogLevelString();

    char appVersion[16];
    Version::getAppVersion(appVersion);
    
    Debug::out(LOG_INFO, "\n\n---------------------------------------------------");
    Debug::out(LOG_INFO, "CosmosEx starting, version: %s", appVersion);

    Version::getRaspberryPiInfo();                                  // fetch model, revision, serial of RPi
    Update::createNewScripts();                                     // update the scripts if needed
    
//	system("sudo echo none > /sys/class/leds/led0/trigger");	    // disable usage of GPIO 23 (pin 16) by LED

    if(otherInstanceIsRunning()) {
        Debug::out(LOG_ERROR, "Other instance of CosmosEx is running, terminate it before starting a new one!");
        printf("\nOther instance of CosmosEx is running, terminate it before starting a new one!\n\n\n");
        return 0;
    }

	if(!gpio_open()) {									            // try to open GPIO and SPI on RPi
		return 0;
	}

	if(flags.justDoReset) {                                         // is this a reset command? (used with STM32 ST-Link debugger)
		Utils::resetHansAndFranz();
		gpio_close();

		printf("\nJust did reset and quit...\n");

		return 0;
	}

    printf("Starting threads\n");

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
    system("cp -r /ce/app/configdrive/* /tmp/configdrive"); // copy new content
    //-------------
    core = new CCoreThread(pxDateService,pxFloppyService,pxScreencastService);

	int res = pthread_create(&mountThreadInfo, NULL, mountThreadCode, NULL);	// create mount thread and run it
	handlePthreadCreate(res, "ce mount", &mountThreadInfo);

    res = pthread_create(&downloadThreadInfo, NULL, downloadThreadCode, NULL);  // create download thread and run it
	handlePthreadCreate(res, "ce download", &downloadThreadInfo);

#ifndef ONPC_NOTHING
    res = pthread_create(&ikbdThreadInfo, NULL, ikbdThreadCode, NULL);			// create the keyboard emulation thread and run it
	handlePthreadCreate(res, "ce ikbd", &ikbdThreadInfo);
#endif

    res = pthread_create(&floppyEncThreadInfo, NULL, floppyEncodeThreadCode, NULL);	// create the floppy encoding thread and run it
	handlePthreadCreate(res, "ce floppy encode", &floppyEncThreadInfo);

    res = pthread_create(&networkThreadInfo, NULL, networkThreadCode, NULL);    // create the network thread and run it
	handlePthreadCreate(res, "ce network", &networkThreadInfo);

    res = pthread_create(&periodicThreadInfo, NULL, periodicThreadCode, NULL);  // create the periodic thread and run it
	handlePthreadCreate(res, "periodic", &periodicThreadInfo);
    
    printf("Entering main loop...\n");
    
	core->run();										// run the main thread

    printf("\n\nExit from main loop\n");

    xServer.stop();

    printf("Stoping screecast service\n");
    pxScreencastService->stop();
    delete pxScreencastService;

    printf("Stoping floppy service\n");
    pxFloppyService->stop();
    delete pxFloppyService;

    printf("Stoping date service\n");
    pxDateService->stop();
    delete pxDateService;

    printf("Stoping virtual keyboard service\n");
    pxVKbdService->stop();
    delete pxVKbdService;
    
    printf("Stoping virtual mouse service\n");
    pxVMouseService->stop();
    delete pxVMouseService;

	delete core;
	gpio_close();										// close gpio and spi

    printf("Stoping mount thread\n");
    Mounter::stop();
	pthread_join(mountThreadInfo, NULL);				// wait until mount     thread finishes

    printf("Stoping download thread\n");
    Downloader::stop();
    pthread_join(downloadThreadInfo, NULL);             // wait until download  thread finishes

#ifndef ONPC_NOTHING
    printf("Stoping ikbd thread\n");
	pthread_kill(ikbdThreadInfo, SIGINT);               // stop the select()
    pthread_join(ikbdThreadInfo, NULL);                 // wait until ikbd      thread finishes
#endif

    printf("Stoping floppy encoder thread\n");
    ImageSilo::stop();
    pthread_join(floppyEncThreadInfo, NULL);            // wait until floppy encode thread finishes

    printf("Stoping network thread\n");
    pthread_join(networkThreadInfo, NULL);              // wait until network   thread finishes

    printf("Stoping periodic thread\n");
	pthread_kill(periodicThreadInfo, SIGINT);               // stop the select()
    pthread_join(periodicThreadInfo, NULL);             // wait until periodic  thread finishes

    printf("Downloader clean up before quit\n");
    Downloader::cleanupBeforeQuit();

    if(singleInstanceSocketFd > 0) {                    // if we got the single instance socket, close it
        close(singleInstanceSocketFd);
    }
    
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
}

void initializeFlags(void)
{
    flags.justShowHelp = false;
    flags.logLevel     = LOG_ERROR;     // init current log level to LOG_ERROR
    flags.justDoReset  = false;         // if shouldn't run the app, but just reset Hans and Franz (used with STM32 ST-Link JTAG)
    flags.noReset      = false;         // don't reset Hans and Franz on start - used with STM32 ST-Link JTAG
    flags.test         = false;         // if set to true, set ACSI ID 0 to translated, ACSI ID 1 to SD, and load floppy with some image
    flags.actAsCeConf  = false;         // if set to true, this app will behave as ce_conf app instead of CosmosEx app
    flags.getHwInfo    = false;         // if set to true, wait for HW info from Hans, and then quit and report it
    flags.noFranz      = false;         // if set to true, won't communicate with Franz
    flags.ikbdLogs     = false;         // no ikbd logs by default
    flags.fakeOldApp   = false;         // don't fake old app by default
    
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
    printf("test     - some default config for device testing\n");
    printf("ce_conf  - use this app as ce_conf on RPi (the app must be running normally, too)\n");
    printf("hwinfo   - get HW version and HDD interface type\n");
    printf("ikbdlogs - write IKBD logs to /var/log/ikbdlog.txt\n");
    printf("fakeold  - fake old app version for reinstall tests\n");
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
    
    if(childPid != 0) {             // in case we fork()ed, kill the child
        Debug::out(LOG_DEBUG, "Killing child with pid %d\n", childPid);
        kill(childPid, SIGKILL);
    }
}

bool otherInstanceIsRunning(void)
{
#ifdef DISTRO_YOCTO
    const char *countCosmosExCmd = "ps | grep cosmos | grep -v grep | wc -l > /tmp/cosmoscount";
#else
    const char *countCosmosExCmd = "ps -A | grep cosmos | wc -l > /tmp/cosmoscount";
#endif

    // count how many cosmos processes are running, store it in /tmp/cosmoscount
    system(countCosmosExCmd);
    
    FILE *f = fopen("/tmp/cosmoscount", "rt");
    if(!f) {                                    // can't open file? other instance probably not running (or is, but can't figure out, so screw it)
        Debug::out(LOG_DEBUG, "otherInstanceIsRunning - couldn't open /tmp/cosmoscount, returning false");
        return false;
    }
    
    int res, cnt;
    res = fscanf(f, "%d", &cnt);                // try to read the number
    fclose(f);
    
    if(res != 1) {                              // couldn't read the number? say we're not running more than once
        Debug::out(LOG_DEBUG, "otherInstanceIsRunning - fscanf failed, returning false");
        return false;
    }
    
    bool isOtherRunning = cnt > 1;              // more than 1 instance? other instance is running

    Debug::out(LOG_DEBUG, "otherInstanceIsRunning - count of instances: %d, returning %s", cnt, isOtherRunning ? "true" : "false");
    return isOtherRunning;
}

