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

extern BYTE g_logLevel;

int main(int argc, char *argv[])
 {
	CCoreThread *core;
	pthread_t	mountThreadInfo;
    pthread_t	downloadThreadInfo;
    pthread_t	ikbdThreadInfo;
	pthread_t	floppyEncThreadInfo;

    if(argc == 2 && strncmp(argv[1], "ll", 2) == 0) {           // is this the log level setting? 
        int ll;
        
        ll = (int) argv[1][2];

        if(ll >= 48 && ll <= 57) {                              // if it's a number between 0 and 9
            ll = ll - 48;
    
            if(ll > LOG_DEBUG) {                                // would be higher than highest log level? fix it
                ll = LOG_DEBUG;
            }

            g_logLevel = ll;                                    // store log level
        }
    }

    Debug::printfLogLevelString();

    Debug::out(LOG_ERROR, "\n\n---------------------------------------------------");
    Debug::out(LOG_ERROR, "CosmosEx starting...");

	if(signal(SIGINT, sigint_handler) == SIG_ERR) {		        // register SIGINT handler
		Debug::out(LOG_ERROR, "Cannot register SIGINT handler!");
	}

	system("sudo echo none > /sys/class/leds/led0/trigger");	// disable usage of GPIO 23 (pin 16) by LED 
	
	if(!gpio_open()) {									        // try to open GPIO and SPI on RPi
		return 0;
	}

	if(argc == 2 && strcmp(argv[1], "reset") == 0) {            // is this a reset command? (used with STM32 ST-Link debugger)
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

    #ifndef ONPC	
	core->run();										// run the main thread
    #else
    core->runOnPc();
    #endif

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

