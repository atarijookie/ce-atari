#include <stdio.h>

#include "config/configstream.h"
#include "settings.h"
#include "global.h"
#include "ccorethread.h"

void construct(void);
void destruct(void);

int main()
 {
	CCoreThread *core;
	
    printf("CosmosEx starting...\n");
	
    core = new CCoreThread();
	core->run();

	delete core;
    printf("CosmosEx terminated.\n");
    return 0;
 }

