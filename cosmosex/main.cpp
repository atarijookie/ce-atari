#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "config/configstream.h"
#include "settings.h"
#include "global.h"
#include "ccorethread.h"
#include "gpio.h"

extern "C" void outDebugString(const char *format, ...);

int main()
 {
	CCoreThread *core;
	
    outDebugString("CosmosEx starting...");
	
	if(!gpio_open()) {							// try to open GPIO and SPI on RPi
		return 0;
	}
	
    core = new CCoreThread();
	core->run();

	delete core;
	gpio_close();
	
    outDebugString("CosmosEx terminated.");
    return 0;
 }

void outDebugString(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    vprintf(format, args);
	printf("\n");

    va_end(args);
}