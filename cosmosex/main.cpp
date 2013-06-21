#include <stdio.h>

#include "settings.h"
#include "configstream.h"

#include <termios.h>

int main()
 {
     printf("CosmosEx starting...\n");
	 
struct termios oldt;
struct termios newt;
tcgetattr(STDIN_FILENO, &oldt); /*store old settings */
newt = oldt; /* copy old settings to new settings */
newt.c_lflag &= ~(ICANON | ECHO); /* make one change to old settings in new settings */
tcsetattr(STDIN_FILENO, TCSANOW, &newt); /*apply the new settings immediatly */
	 
	 Settings s;
	 bool val = s.getBool("test", false);
	 printf("The bool is: %d\n", val);
	 
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
	 while(1) {
		int ch = getchar();
		
		switch(ch) {
			case 'a': ConfigStream::instance().onKeyDown(75,0); break;		// left
			case 'd': ConfigStream::instance().onKeyDown(77,0); break;		// right
			case 'w': ConfigStream::instance().onKeyDown(72,0); break;		// up
			case 's': ConfigStream::instance().onKeyDown(80,0); break;		// down
			
			case 10: ConfigStream::instance().onKeyDown(0, 13); break;		// enter
			case 'q': ConfigStream::instance().onKeyDown(0,127); break;		// delete
			case 'e': ConfigStream::instance().onKeyDown(0,8); break;		// backspace
			default: ConfigStream::instance().onKeyDown(0, ch); break;
		}

		ConfigStream::instance().getStream(false, bfr, 10240);
		printf("STREAM: %s\n", bfr);	 
	 }
	 
     return 0;
 }

