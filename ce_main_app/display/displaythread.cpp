#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>

#include <signal.h>
#include <pthread.h>
#include <queue>

#include "utils.h"
#include "debug.h"
#include "update.h"

#include "global.h"
#include "gpio.h"

#include "ssd1306.h"
#include "adafruit_gfx.h"
#include "lcdfont.h"
#include "swi2c.h"
#include "displaythread.h"

extern THwConfig hwConfig;
extern TFlags    flags;
extern DebugVars dbgVars;

Adafruit_GFX *gfx;
SoftI2CMaster *i2c;

int displayPipeFd[2];

/*
 This we want to show on display:
 123456789012345678901

 ACSI 01......
 FDD1 image_name_here
 IKBD Kbd Mouse J1 J2
 LAN  192.168.xxx.yyy
 */

// the following array of strings holds every line that can be shown as a raw string, they are filled by rest of the app, and accessed by specified line type number
#define DISP_LINE_MAXLEN 21
char display_line[DISP_LINE_COUNT][DISP_LINE_MAXLEN + 1];

// each screen here is a group of 4 line numbers, because display can show only 4 lines at the time, and this defines which screen shows which 4 lines
int display_screens[DISP_SCREEN_COUNT][4] = {DISP_SCREEN_HDD1_LINES, DISP_SCREEN_HDD2_LINES};

// this defines for each existing screen which will be the next screen - this allows us to create a loop between them, plus have extra screens which are not normally shown but switch back to loop
int display_screens_next[DISP_SCREEN_COUNT];

// draw specified screen on front display
static void display_drawScreen(int screenIndex);

// get read / write end of pipe
static int getPipe(bool readNotWrite);

void *displayThreadCode(void *ptr)
{
	// get the read end of pipe
	int displayTriggerPipe = getPipe(true);

    int currentScreen = 0;

	// this defines how the screens switch from one to another
	display_screens_next[DISP_SCREEN_HDD1_IDX] = DISP_SCREEN_HDD2_IDX;
	display_screens_next[DISP_SCREEN_HDD2_IDX] = DISP_SCREEN_HDD1_IDX;

    Debug::out(LOG_DEBUG, "Display thread starting...");

    display_init();

    gfx = new Adafruit_GFX(SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT);    // font displaying library

	// fd vars for select()
	fd_set readfds;
    int max_fd;
	struct timeval timeout;

	// init fd set
	FD_ZERO(&readfds);
	FD_SET(displayTriggerPipe, &readfds);
	max_fd = displayTriggerPipe;

    while(sigintReceived == 0) {
		// set timeout - might be changed by select(), so set every time before select()
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

        int res = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

		if(res > 0) {			// if some fd is ready
            if(FD_ISSET(displayTriggerPipe, &readfds)) {				// display trigger pipe is ready
				char newDisplayIndex = 0;
				res = read(displayTriggerPipe, &newDisplayIndex, 1);	// try to read new display index

				if(res == -1) {	// failed to read? skip it
					continue;
				}

				// read good, store new display index
				currentScreen = newDisplayIndex;
			}
		}

        // draw screen on display
        display_drawScreen(currentScreen);

        // move to next screen
		currentScreen = display_screens_next[currentScreen];
    }

    display_print_center("CosmosEx stopped");
    display_deinit();

	// close the display pipe
	close(displayPipeFd[0]);
	close(displayPipeFd[1]);

    Debug::out(LOG_DEBUG, "Display thread terminated.");
    return 0;
}

void display_init(void)
{
    i2c = new SoftI2CMaster();              // software I2C master on GPIO pins

    ssd1306_begin(SSD1306_SWITCHCAPVCC);    // low level OLED library
    ssd1306_clearDisplay();
    ssd1306_display();
}

void display_deinit(void)
{
    delete gfx;
    delete i2c;
}

void display_print_center(const char *str)
{
    int len = strlen(str);
    int x = (SSD1306_LCDWIDTH - (CHAR_W * len)) / 2;
    int y = (SSD1306_LCDHEIGHT - CHAR_H)/2;

    ssd1306_clearDisplay();
    gfx->drawString(x, y, str);
    ssd1306_display();
}

void display_setLine(int displayLineId, const char *newLineString)
{
    if(displayLineId < 0 || displayLineId >= DISP_LINE_COUNT) {  // verify array index validity
        return;
    }

    char *line = display_line[displayLineId];           // get pointer
    memset(line, ' ', DISP_LINE_MAXLEN);                // clear whole line
    strncpy(line, newLineString, DISP_LINE_MAXLEN);     // copy data
    line[DISP_LINE_MAXLEN] = 0;                         // zero terminate
}

static void display_drawScreen(int screenIndex)
{
    // get lines numbers from the specified screen number - this will be 4 indexes of lines in screenLines[]
    int *screenLines = (int *) &display_screens[screenIndex];

    int i, y;
    for(i=0; i<4; i++) {
        const char *lineStr = display_line[screenLines[i]]; // get pointer to string from screen definition
        y = i * CHAR_H;
        gfx->drawString(0, y, lineStr);     // show it on display
    }
}

static int getPipe(bool readNotWrite)
{
	if(displayPipeFd[0] == 0) {		// if the display pipe is not created yet, create it
		int res = pipe2(displayPipeFd, O_NONBLOCK);

		if(res == -1) {				// failed to create pipes? fail
			return -1;
		}
	}

	// return right end of pipe
	return (readNotWrite ? displayPipeFd[0] : displayPipeFd[1]);
}

void display_showNow(int screenIndex)
{
	// bad screen index? do nothing
	if(screenIndex < 0 || screenIndex >= DISP_SCREEN_COUNT) {
		return;
	}

	// get write end of pipe
	int fd = getPipe(false);

	// got pipe?
	if(fd != -1) {
		char outBfr = (char) screenIndex;
		write(fd, &outBfr, 1);	// send screen index through pipe
	}
}

