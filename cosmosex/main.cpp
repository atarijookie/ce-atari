#include <stdio.h>
#include <string.h>

#include "config/configstream.h"
#include "settings.h"
#include "global.h"
#include "ccorethread.h"
#include "gpio.h"
#include "debug.h"

int main()
 {
	CCoreThread *core;
	
    Debug::out("CosmosEx starting...");
	
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
