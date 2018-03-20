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
#define DISPLAY_NORMAL_SCREEN_COUNT     2
int display_screens[DISPLAY_NORMAL_SCREEN_COUNT][4] = {DISP_SCREEN_HDD1, DISP_SCREEN_HDD2};

void display_showScreen(int screenNumber);

void *displayThreadCode(void *ptr)
{
    //fd_set readfds;
    //int max_fd;
    int currentScreen = 0;

    Debug::out(LOG_DEBUG, "Display thread starting...");

    display_init();

    gfx = new Adafruit_GFX(SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT);    // font displaying library

    while(sigintReceived == 0) {
        //max_fd = -1;
        //FD_ZERO(&readfds);

        //int activity = select(max_fd + 1, &read_fds, &write_fds, &except_fds, NULL);
        Utils::sleepMs(5000);

        // show next normal screen
        display_showScreen(currentScreen);

        // move to next regular screen
        currentScreen++;
        if(currentScreen >= DISPLAY_NORMAL_SCREEN_COUNT) {
            currentScreen = 0;
        }
    }

    display_print_center("CosmosEx stopped");
    display_deinit();

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

void display_showScreen(int screenNumber)
{
    // get lines numbers from the specified screen number - this will be 4 indexes of lines in screenLines[]
    int *screenLines = (int *) &display_screens[screenNumber];

    int i, y;
    for(i=0; i<4; i++) {
        const char *lineStr = display_line[screenLines[i]]; // get pointer to string from screen definition
        y = i * CHAR_H;
        gfx->drawString(0, y, lineStr);     // show it on display
    }
}
