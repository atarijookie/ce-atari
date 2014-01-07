#include <stdio.h>

#include "config/configstream.h"
#include "settings.h"
#include "global.h"
#include "ccorethread.h"
#include "gpio.h"

void construct(void);
void destruct(void);

int main()
 {
	CCoreThread *core;
	
    printf("CosmosEx starting...\n");
	
	if(!gpio_open()) {							// try to open GPIO and SPI on RPi
		return 0;
	}
	
    core = new CCoreThread();
	core->run();

	delete core;
	gpio_close();
	
    printf("CosmosEx terminated.\n");
    return 0;
 }

