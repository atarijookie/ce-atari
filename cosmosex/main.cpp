#include <stdio.h>

#include "settings.h"
#include "configstream.h"

int main()
 {
     printf("CosmosEx starting...\n");
	 
	 Settings s;
	 bool val = s.getBool("test", false);
	 printf("The bool is: %d\n", val);
	 
	 char bfr[10240];
	 ConfigStream::instance().getStream(true, bfr, 10240);
	 printf("STREAM: %s\n", bfr);	 
	 
     return 0;
 }

