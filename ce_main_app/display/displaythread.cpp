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
#include "displaythread.h"

extern THwConfig hwConfig;
extern TFlags    flags;
extern DebugVars dbgVars;

SSD1306 *display;
Adafruit_GFX *gfx;

int displayPipeFd[2];
int beeperPipeFd[2];

static void doBeep(int beeperCommand);

/*
 This we want to show on display:
 123456789012345678901

 ACSI 01......
 FDD1 image_name_here
 IKBD Kbd Mouse J1 J2
 LAN  192.168.xxx.yyy

 USB tran   ZIP dir
 config O   shared N
 drives: CDE
 2017-03-20 10:25
 */

// the following array of strings holds every line that can be shown as a raw string, they are filled by rest of the app, and accessed by specified line type number
#define DISP_LINE_MAXLEN    21
#define DISPLAY_LINES_SIZE  (DISP_LINE_COUNT * (DISP_LINE_MAXLEN + 1))
char display_lines[DISPLAY_LINES_SIZE];

// each screen here is a group of 4 line numbers, because display can show only 4 lines at the time, and this defines which screen shows which 4 lines
int display_screens[DISP_SCREEN_COUNT][4] = {DISP_SCREEN_HDD1_LINES, DISP_SCREEN_HDD2_LINES, DISP_SCREEN_TRANS_LINES};

// this defines for each existing screen which will be the next screen - this allows us to create a loop between them, plus have extra screens which are not normally shown but switch back to loop
int display_screens_next[DISP_SCREEN_COUNT];

// draw specified screen on front display
static void display_drawScreen(int screenIndex);

// get pointer to buffer where the line string should be stored
static char *get_displayLinePtr(int displayLineId);

void *displayThreadCode(void *ptr)
{
    // create pipes as needed
    pipe2(displayPipeFd, O_NONBLOCK);
    pipe2(beeperPipeFd, O_NONBLOCK);

    int currentScreen = 0;

    // this defines how the screens switch from one to another
    display_screens_next[DISP_SCREEN_HDD1_IDX]  = DISP_SCREEN_HDD2_IDX;
    display_screens_next[DISP_SCREEN_HDD2_IDX]  = DISP_SCREEN_TRANS_IDX;
    display_screens_next[DISP_SCREEN_TRANS_IDX] = DISP_SCREEN_HDD1_IDX;

    Debug::out(LOG_DEBUG, "Display thread starting...");

    display_init();

    // fd vars for select()
    fd_set readfds;
    int max_fd = (displayPipeFd[0] > beeperPipeFd[0]) ? displayPipeFd[0] : beeperPipeFd[0];
    struct timeval timeout;

    while(sigintReceived == 0) {
        // set timeout - might be changed by select(), so set every time before select()
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        // init fd set
        FD_ZERO(&readfds);
        FD_SET(displayPipeFd[0], &readfds);
        FD_SET(beeperPipeFd[0], &readfds);

        // wait for pipe or timeout
        int res = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if(res < 0) {       // on select() error and signal received
            continue;
        }

        bool redrawDisplay = false;

        if(res > 0) {           // if some fd is ready
            // display trigger pipe is ready
            if(FD_ISSET(displayPipeFd[0], &readfds)) {
                char newDisplayIndex = 0;
                res = read(displayPipeFd[0], &newDisplayIndex, 1);  // try to read new display index

                if(res != -1) { // read good? store new display index
                    currentScreen = newDisplayIndex;
                    redrawDisplay = true;
                }
            }

            // beeper pipe is ready?
            if(FD_ISSET(beeperPipeFd[0], &readfds)) {
                char beeperCommand = 0;
                res = read(beeperPipeFd[0], &beeperCommand, 1);     // try to read beeper command

                if(res != -1) { // read good? do beep
                    doBeep(beeperCommand);
                }
            }
        } else if(res == 0) {   // on timeout
            redrawDisplay = true;
        }

        // if should redraw display - on timeout or on request, do it
        if(redrawDisplay) {
            // draw screen on display
            display_drawScreen(currentScreen);

            // move to next screen
            currentScreen = display_screens_next[currentScreen];
        }
    }

    display_deinit();

    // close the pipes
    close(displayPipeFd[0]);
    displayPipeFd[0] = 0;
    close(displayPipeFd[1]);
    displayPipeFd[1] = 0;

    close(beeperPipeFd[0]);
    beeperPipeFd[0] = 0;
    close(beeperPipeFd[1]);
    beeperPipeFd[1] = 0;

    Debug::out(LOG_DEBUG, "Display thread terminated.");
    return 0;
}

void display_init(void)
{
    display = new SSD1306();

    bool res = display->begin(SSD1306_SWITCHCAPVCC);    // low level OLED library
    display->clearDisplay();
    display->display();

    Debug::out(LOG_INFO, "Front display init: %s", res ? "OK" : "FAILED");

    gfx = new Adafruit_GFX(SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT, display);    // font displaying library
}

void display_deinit(void)
{
    delete gfx;
    gfx = NULL;

    delete display;
    display = NULL;
}

void display_print_center(const char *str)
{
    int len = strnlen(str, 32);
    int x = (SSD1306_LCDWIDTH - (CHAR_W * len)) / 2;
    int y = (SSD1306_LCDHEIGHT - CHAR_H)/2;

    display->clearDisplay();
    gfx->drawString(x, y, str);
    display->display();
}

char *get_displayLinePtr(int displayLineId)
{
    if(displayLineId < 0 || displayLineId >= DISP_LINE_COUNT) {  // verify array index validity
         return NULL;
    }

    if(display_lines == NULL) {
        return NULL;
    }

    char *line = display_lines + (displayLineId * (DISP_LINE_MAXLEN + 1)); // get pointer
    return line;
}

void display_setLine(int displayLineId, const char *newLineString)
{
    char *line = get_displayLinePtr(displayLineId);

    if(line == NULL) {
        return;
    }

    memset(line, ' ', DISP_LINE_MAXLEN);                // clear whole line
    strncpy(line, newLineString, DISP_LINE_MAXLEN);     // copy data
    line[DISP_LINE_MAXLEN] = 0;                         // zero terminate
}

static void doBeep(int beeperCommand)
{
    // should be short-mid-long beep?
    if(beeperCommand >= BEEP_SHORT && beeperCommand <= BEEP_LONG) {
        int beepLengthMs[3] = {50, 150, 500};       // beep length: short, mid, long
        int lengthMs = beepLengthMs[beeperCommand]; // get beep length in ms

        bcm2835_gpio_write(PIN_BEEPER, HIGH);
        Utils::sleepMs(lengthMs);
        bcm2835_gpio_write(PIN_BEEPER, LOW);
    }

    // should be floppy seek noise?
    if((beeperCommand & BEEP_FLOPPY_SEEK) == BEEP_FLOPPY_SEEK) {
        int trackCount = beeperCommand - BEEP_FLOPPY_SEEK;
        if(trackCount < 0) {        // too little?
            trackCount = 0;
        }

        if(trackCount > 100) {      // too much?
            trackCount = 80;
        }

        // for each track seek do a short bzzzz, so in the end it's not a long beep, but a buzzing sound
        int i;
        for(i=0; i<trackCount; i++) {
            bcm2835_gpio_write(PIN_BEEPER, HIGH);
            Utils::sleepMs(1);
            bcm2835_gpio_write(PIN_BEEPER, LOW);
            Utils::sleepMs(2);
        }
    }
}

static void display_drawScreen(int screenIndex)
{
    display->clearDisplay();

    // get lines numbers from the specified screen number - this will be 4 indexes of lines in screenLines[]
    int *screenLines = (int *) &display_screens[screenIndex];

    int i, y;
    for(i=0; i<4; i++) {
        int screenLine = screenLines[i];        // which line we should show?

        if(screenLine == DISP_LINE_DATETIME) {  // is it datetime? update it now
            char humanTime[128];
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            sprintf(humanTime, "%04d-%02d-%02d    %02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
            display_setLine(DISP_LINE_DATETIME, humanTime);
        }

        char *line = get_displayLinePtr(screenLine);

        if(line == NULL) {
            continue;
        }

        y = i * CHAR_H;
        gfx->drawString(0, y, line);            // show it on display
    }

    display->display();
}

void display_showNow(int screenIndex)
{
    // bad screen index? do nothing
    if(screenIndex < 0 || screenIndex >= DISP_SCREEN_COUNT) {
        return;
    }

    // got pipe?
    if(displayPipeFd[1] > 0) {
        char outBfr = (char) screenIndex;
        write(displayPipeFd[1], &outBfr, 1);    // send screen index through pipe
    }
}

void beeper_beep(int beepLen)
{
    // invalid beep? quit
    if(beepLen < BEEP_SHORT || beepLen > BEEP_LONG) {
        return;
    }

    // got pipe?
    if(beeperPipeFd[1] > 0) {
        char outBfr = (char) beepLen;
        write(beeperPipeFd[1], &outBfr, 1);
    }
}

void beeper_floppySeek(int trackCount)
{
    // invalid beep? quit
    if(trackCount < 0 || trackCount > 100) {
        return;
    }

    // got pipe?
    if(beeperPipeFd[1] > 0) {
        char outBfr = (char) (trackCount + BEEP_FLOPPY_SEEK);
        write(beeperPipeFd[1], &outBfr, 1);
    }
}
