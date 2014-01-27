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

volatile sig_atomic_t sigintReceived = 0;
void sigint_handler(int sig);

int main()
 {
	CCoreThread *core;
	pthread_t	mountThreadInfo;

    Debug::out("\n\n---------------------------------------------------");
    Debug::out("CosmosEx starting...");

	if(signal(SIGINT, sigint_handler) == SIG_ERR) {		// register SIGINT handler
		Debug::out("Cannot register SIGINT handler!");
	}

	if(!gpio_open()) {									// try to open GPIO and SPI on RPi
		return 0;
	}

	int res = pthread_create( &mountThreadInfo, NULL, mountThreadCode, NULL);	// create mount thread and run it
	
	if(res != 0) {
		Debug::out("Failed to create mount thread, mounting won't work");
	} else {
		Debug::out("Mount thread created");
	}
		
    core = new CCoreThread();
	core->run();										// run the main thread

	delete core;
	gpio_close();										// close gpio and spi

	pthread_join(mountThreadInfo, NULL);				// wait until mount thread finishes
	
    Debug::out("CosmosEx terminated.");
    return 0;
 }

void sigint_handler(int sig)
{
    Debug::out("SIGINT signal received, terminating.");
	sigintReceived = 1;
} 

