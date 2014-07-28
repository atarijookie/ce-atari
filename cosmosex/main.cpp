#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <queue>          

#include "config/configstream.h"
#include "settings.h"
#include "global.h"
#include "ccorethread.h"
#include "gpio.h"
#include "debug.h"
#include "mounter.h"
#include "downloader.h"
#include "ikbd.h"

volatile sig_atomic_t sigintReceived = 0;
void sigint_handler(int sig);

void handlePthreadCreate(int res, char *what);
void parseCmdLineArguments(int argc, char *argv[]);
void printfPossibleCmdLineArgs(void);

BYTE g_logLevel     = LOG_ERROR;                                // init current log level to LOG_ERROR
bool g_justDoReset  = false;                                    // if shouldn't run the app, but just reset Hans and Franz (used with STM32 ST-Link JTAG)
bool g_noReset      = false;                                    // don't reset Hans and Franz on start - used with STM32 ST-Link JTAG
bool g_test         = false;                                    // if set to true, set ACSI ID 0 to translated, ACSI ID 1 to SD, and load floppy with some image
bool g_forceUpdate  = false;                                    // if set to true, won't check if the update components are newer, but will update all of them 

int main(int argc, char *argv[])
 {
	CCoreThread *core;
	pthread_t	mountThreadInfo;
    pthread_t	downloadThreadInfo;
    pthread_t	ikbdThreadInfo;
	pthread_t	floppyEncThreadInfo;

    parseCmdLineArguments(argc, argv);                          // parse cmd line arguments and set global variables

    printfPossibleCmdLineArgs();
    Debug::printfLogLevelString();

    Debug::out(LOG_ERROR, "\n\n---------------------------------------------------");
    Debug::out(LOG_ERROR, "CosmosEx starting...");

	if(signal(SIGINT, sigint_handler) == SIG_ERR) {		        // register SIGINT handler
		Debug::out(LOG_ERROR, "Cannot register SIGINT handler!");
	}

//	system("sudo echo none > /sys/class/leds/led0/trigger");	// disable usage of GPIO 23 (pin 16) by LED 
	
	if(!gpio_open()) {									        // try to open GPIO and SPI on RPi
		return 0;
	}

	if(g_justDoReset) {                                         // is this a reset command? (used with STM32 ST-Link debugger)
		Utils::resetHansAndFranz();
		gpio_close();
		
		printf("\nJust did reset and quit...\n");
		
		return 0;
	}

    Downloader::initBeforeThreads();

    core = new CCoreThread();

	int res = pthread_create( &mountThreadInfo, NULL, mountThreadCode, NULL);	// create mount thread and run it
	handlePthreadCreate(res, (char *) "mount");

    res = pthread_create(&downloadThreadInfo, NULL, downloadThreadCode, NULL);  // create download thread and run it
	handlePthreadCreate(res, (char *) "download");

    res = pthread_create(&ikbdThreadInfo, NULL, ikbdThreadCode, NULL);			// create the keyboard emulation thread and run it
	handlePthreadCreate(res, (char *) "ikbd");

    res = pthread_create(&floppyEncThreadInfo, NULL, floppyEncodeThreadCode, NULL);	// create the floppy encoding thread and run it
	handlePthreadCreate(res, (char *) "floppy encode");

	core->run();										// run the main thread

	delete core;
	gpio_close();										// close gpio and spi

	pthread_join(mountThreadInfo, NULL);				// wait until mount     thread finishes
    pthread_join(downloadThreadInfo, NULL);             // wait until download  thread finishes
    pthread_join(ikbdThreadInfo, NULL);                 // wait until ikbd      thread finishes
    pthread_join(floppyEncThreadInfo, NULL);            // wait until floppy encode thread finishes

    Downloader::cleanupBeforeQuit();

    Debug::out(LOG_INFO, "CosmosEx terminated.");
    return 0;
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

                g_logLevel = ll;                                    // store log level
            }
            
            continue;
        }
    
        // should we just reset Hans and Franz and quit? (used with STM32 ST-Link JTAG)
        if(strcmp(argv[i], "reset") == 0) {
            g_justDoReset = true;
            continue;
        }
    
        // don't resetHans and Franz on start (used with STM32 ST-Link JTAG)
        if(strcmp(argv[i], "noreset") == 0) {
            g_noReset = true;
            continue;
        }
        
        // for testing purposes: set ACSI ID 0 to translated, ACSI ID 1 to SD, and load floppy with some image
        if(strcmp(argv[i], "test") == 0) {
            printf("Testing setup active!\n");
            g_test = true;
            continue;
        }
        
        // for update testing purposes - don't check if we need update, but do update anyway
        if(strcmp(argv[i], "forceup") == 0) {
            printf("Forcing update all on update!\n");
            g_forceUpdate = true;
            continue;
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
    printf("forceup - force update - don't check component versions, update all\n");
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
    Debug::out(LOG_INFO, "SIGINT signal received, terminating.");
	sigintReceived = 1;
} 

