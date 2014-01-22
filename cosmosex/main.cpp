#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "config/configstream.h"
#include "settings.h"
#include "global.h"
#include "ccorethread.h"
#include "gpio.h"
#include "debug.h"

volatile sig_atomic_t sigintReceived = 0;
void sigint_handler(int sig);

int main()
 {
	CCoreThread *core;

    Debug::out("\n\n---------------------------------------------------");
    Debug::out("CosmosEx starting...");

	if(signal(SIGINT, sigint_handler) == SIG_ERR) {
		Debug::out("Cannot register SIGINT handler!");
	}

	if(!gpio_open()) {							// try to open GPIO and SPI on RPi
		return 0;
	}

    core = new CCoreThread();
	core->run();

	delete core;
	gpio_close();

    Debug::out("CosmosEx terminated.");
    return 0;
 }

void sigint_handler(int sig)
{
    Debug::out("SIGINT signal received, terminating.");
	sigintReceived = 1;
    return;    
} 
