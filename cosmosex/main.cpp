#include <stdio.h>

#include "settings.h"
#include "configstream.h"

int main()
 {
     printf("CosmosEx starting...\n");
	 
	 Settings s;
	 bool val = s.getBool("test", false);
	 printf("The bool is: %d\n", val);
	 
	 ConfigStream cs;
	 cs.goToHomeScreen();
	 
	 char bfr[1024];
	 cs.getStream(bfr, 1024);
	 printf("STREAM: %s\n", bfr);	 
	 
     return 0;
 }

