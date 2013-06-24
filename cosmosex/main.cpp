#include <stdio.h>

#include "settings.h"
#include "configstream.h"

#include "globaldefs.h"

#include <termios.h>

int main()
 {
    printf("CosmosEx starting...\n");

	//-----------------	 
	// read the settings and create devices as needed
	Settings s;
	 
	char key[32];
	int devTypes[8];
	bool somethingActive = false;						// flag to see if we got at least one device active
	 
	for(int id=0; id<8; id++) {						// read the list of device types from settings
		sprintf(key, "ACSI_DEVTYPE_%d", id);			// create settings KEY, e.g. ACSI_DEVTYPE_0
		devTypes[id] = s.getInt(key, DEVTYPE_OFF);		
		
		if(devTypes[id] < 0) {
			devTypes[id] = DEVTYPE_OFF;
		}
		
		if(devTypes[id] != DEVTYPE_OFF) {				// if we found something active, set the flag
			somethingActive = true;
		}
	}
	 
	if(!somethingActive) {								// if no device is activated, activate DEVTYPE_TRANSLATED on ACSI ID 0 to avoid bricking the device
		devTypes[0] = DEVTYPE_TRANSLATED;
	}
	//-----------------
	 
	char bfr[10240];
	ConfigStream::instance().getStream(true, bfr, 10240);
	printf("STREAM: %s\n", bfr);	 
/*
enter     - key = 13
esc       - key = 27
space     - key = 32
backspace - key = 8
delete    - key = 127

home      - vkey = 71, key = 0
left      - vkey = 75, key = 0
right     - vkey = 77, key = 0
up        - vkey = 72, key = 0
down      - vkey = 80, key = 0	 
*/	 
struct termios oldt;
struct termios newt;
tcgetattr(STDIN_FILENO, &oldt); /*store old settings */
newt = oldt; /* copy old settings to new settings */
newt.c_lflag &= ~(ICANON | ECHO); /* make one change to old settings in new settings */
tcsetattr(STDIN_FILENO, TCSANOW, &newt); /*apply the new settings immediatly */
	 

	 while(1) {
		int ch = getchar();
		
		switch(ch) {
			case 'a': ConfigStream::instance().onKeyDown(75,0); break;		// left
			case 'd': ConfigStream::instance().onKeyDown(77,0); break;		// right
			case 'w': ConfigStream::instance().onKeyDown(72,0); break;		// up
			case 's': ConfigStream::instance().onKeyDown(80,0); break;		// down
			
			case 10:  ConfigStream::instance().onKeyDown(0, 13); break;		// enter
			case 'q': ConfigStream::instance().onKeyDown(0,127); break;		// delete
			case 'e': ConfigStream::instance().onKeyDown(0,8); break;		// backspace
			case 'm': ConfigStream::instance().showMessageScreen((char *)"Warning", (char *)"This is a test warning!"); break;
			default: ConfigStream::instance().onKeyDown(0, ch); break;
		}

		ConfigStream::instance().getStream(false, bfr, 10240);
		printf("STREAM: %s\n", bfr);	 
	 }
	 
     return 0;
 }

