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

volatile sig_atomic_t sigintReceived = 0;
void sigint_handler(int sig);

int main(int argc, char *argv[])
 {
	CCoreThread *core;
	pthread_t	mountThreadInfo;
    pthread_t	downloadThreadInfo;

    Debug::out("\n\n---------------------------------------------------");
    Debug::out("CosmosEx starting...");

	if(signal(SIGINT, sigint_handler) == SIG_ERR) {		// register SIGINT handler
		Debug::out("Cannot register SIGINT handler!");
	}

	system("sudo echo none > /sys/class/leds/led0/trigger");	// disable usage of GPIO 23 (pin 16) by LED 
	
	if(!gpio_open()) {									// try to open GPIO and SPI on RPi
		return 0;
	}

	if(argc == 2 && strcmp(argv[1], "reset") == 0) {
		Utils::resetHansAndFranz();
		gpio_close();
		
		printf("\nJust did reset and quit...\n");
		
		return 0;
	}

    downloadInitBeforeThreads();

    core = new CCoreThread();

	int res = pthread_create( &mountThreadInfo, NULL, mountThreadCode, NULL);	// create mount thread and run it
	
	if(res != 0) {
		Debug::out("Failed to create mount thread, mounting won't work");
	} else {
		Debug::out("Mount thread created");
	}

    res = pthread_create(&downloadThreadInfo, NULL, downloadThreadCode, NULL);  // create download thread and run it
	if(res != 0) {
		Debug::out("Failed to create download thread, downloading won't work");
	} else {
		Debug::out("Download thread created");
	}
	
	core->run();										// run the main thread

	delete core;
	gpio_close();										// close gpio and spi

	pthread_join(mountThreadInfo, NULL);				// wait until mount thread finishes
    pthread_join(downloadThreadInfo, NULL);             // wait until downloadThread finishes
	
    downloadCleanupBeforeQuit();

    Debug::out("CosmosEx terminated.");
    return 0;
 }

void sigint_handler(int sig)
{
    Debug::out("SIGINT signal received, terminating.");
	sigintReceived = 1;
} 

