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
#include "../settings.h"

#include "global.h"

#include "ssd1306.h"
#include "adafruit_gfx.h"
#include "lcdfont.h"
#include "displaythread.h"
#include "../chipinterface.h"
#include "../translated/translateddisk.h"

extern THwConfig hwConfig;
extern TFlags    flags;
extern DebugVars dbgVars;

SSD1306 *display;
Adafruit_GFX *gfx;

int displayPipeFd[2];
int beeperPipeFd[2];

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

//#define DEBUG_DISPLAY

// the following array of strings holds every line that can be shown as a raw string, they are filled by rest of the app, and accessed by specified line type number
#define DISP_LINE_MAXLEN    21
#define DISPLAY_LINES_SIZE  (DISP_LINE_COUNT * (DISP_LINE_MAXLEN + 1))
char display_lines[DISPLAY_LINES_SIZE];

// each screen here is a group of 4 line numbers, because display can show only 4 lines at the time, and this defines which screen shows which 4 lines
int display_screens[DISP_SCREEN_COUNT][5] = {DISP_SCREEN_HDD1_LINES, DISP_SCREEN_HDD2_LINES, DISP_SCREEN_TRANS_LINES, DISP_SCREEN_RECOVERY_LINES, DISP_SCREEN_POWEROFF_LINES};

// this defines for each existing screen which will be the next screen - this allows us to create a loop between them, plus have extra screens which are not normally shown but switch back to loop
int display_screens_next[DISP_SCREEN_COUNT];

// draw specified screen on front display
static void display_drawScreen(int screenIndex);

// get pointer to buffer where the line string should be stored
static char *get_displayLinePtr(int displayLineId);

extern ChipInterface* chipInterface;

const char* dispScreenToText(int index)
{
    switch(index) {
        case DISP_SCREEN_HDD1_IDX:  return "DISP_SCREEN_HDD1_IDX";
        case DISP_SCREEN_HDD2_IDX:  return "DISP_SCREEN_HDD2_IDX";
        case DISP_SCREEN_TRANS_IDX: return "DISP_SCREEN_TRANS_IDX";
        case DISP_SCREEN_RECOVERY:  return "DISP_SCREEN_RECOVERY";
        case DISP_SCREEN_POWEROFF:  return "DISP_SCREEN_POWEROFF";
        default: return "???";
    }
}

const char* dispLineToText(int index)
{
    switch(index) {
        case DISP_LINE_EMPTY:       return "DISP_LINE_EMPTY";
        case DISP_LINE_HDD_IDS:     return "DISP_LINE_HDD_IDS";
        case DISP_LINE_HDD_TYPES:   return "DISP_LINE_HDD_TYPES";
        case DISP_LINE_FLOPPY:      return "DISP_LINE_FLOPPY";
        case DISP_LINE_IKDB:        return "DISP_LINE_IKDB";
        case DISP_LINE_LAN :        return "DISP_LINE_LAN";
        case DISP_LINE_WLAN:        return "DISP_LINE_WLAN";
        case DISP_LINE_TRAN_SETT:   return "DISP_LINE_TRAN_SETT";
        case DISP_LINE_CONF_SHAR:   return "DISP_LINE_CONF_SHAR";
        case DISP_LINE_TRAN_DRIV:   return "DISP_LINE_TRAN_DRIV";
        case DISP_LINE_DATETIME:    return "DISP_LINE_DATETIME";
        case DISP_LINE_RECOVERY1:   return "DISP_LINE_RECOVERY1";
        case DISP_LINE_RECOVERY2:   return "DISP_LINE_RECOVERY2";
        case DISP_LINE_POWEROFF1:   return "DISP_LINE_POWEROFF1";
        default: return "???";
    }
}

static void fillNetworkDisplayLines(void)
{
    uint8_t bfr[10];
    Utils::getIpAdds(bfr);

    char tmp[64];

    if(bfr[0] == 1) {                               // eth0 enabled? add its IP
        sprintf(tmp, "LAN : %d.%d.%d.%d", (int) bfr[1], (int) bfr[2], (int) bfr[3], (int) bfr[4]);
        display_setLine(DISP_LINE_LAN, tmp);
    } else {
        display_setLine(DISP_LINE_LAN, "LAN : disabled");
    }

    if(bfr[5] == 1) {                               // wlan0 enabled? add its IP
        sprintf(tmp, "WLAN: %d.%d.%d.%d", (int) bfr[6], (int) bfr[7], (int) bfr[8], (int) bfr[9]);
        display_setLine(DISP_LINE_WLAN, tmp);
    } else {
        display_setLine(DISP_LINE_WLAN, "WLAN: disabled");
    }
}

static void fillTranslatedDisplayLines(void)
{
    TranslatedDisk* translated = TranslatedDisk::getInstance();

    if(translated) {
        translated->mutexLock();
        translated->fillTranslatedDisplayLines();
        translated->mutexUnlock();
    }
}

void *displayThreadCode(void *ptr)
{
    int btnDownTime = 0;
    uint32_t nextScreenTime = 0;

    display_setLine(DISP_LINE_POWEROFF1, "     Power off?      ");

    FloppyConfig floppyConfig;                                  // this contains floppy sound settings
    Settings s;
    s.loadFloppyConfig(&floppyConfig);

    // create pipes as needed
    pipe2(displayPipeFd, O_NONBLOCK);
    pipe2(beeperPipeFd, O_NONBLOCK);

    int currentScreen = 0;

    // this defines how the screens switch from one to another
    display_screens_next[DISP_SCREEN_HDD1_IDX]  = DISP_SCREEN_HDD2_IDX;
    display_screens_next[DISP_SCREEN_HDD2_IDX]  = DISP_SCREEN_TRANS_IDX;
    display_screens_next[DISP_SCREEN_TRANS_IDX] = DISP_SCREEN_HDD1_IDX;
    display_screens_next[DISP_SCREEN_RECOVERY]  = DISP_SCREEN_HDD1_IDX;     // if you show RECOVERY screen, next screen will be the 1st screen in the loop
    display_screens_next[DISP_SCREEN_POWEROFF]  = DISP_SCREEN_HDD1_IDX;     // if you show POWER-OFF screen, next screen will be the 1st screen in the loop

    Debug::out(LOG_DEBUG, "Display thread starting...");

    display_init();

    // fd vars for select()
    fd_set readfds;
    int max_fd = (displayPipeFd[0] > beeperPipeFd[0]) ? displayPipeFd[0] : beeperPipeFd[0];
    struct timeval timeout;

    nextScreenTime = Utils::getEndTime(NEXT_SCREEN_INTERVAL);

    while(sigintReceived == 0) {
        // set timeout - might be changed by select(), so set every time before select()
        timeout.tv_sec  = 0;
        timeout.tv_usec = 100000;   // 100 ms

        // init fd set
        FD_ZERO(&readfds);
        FD_SET(displayPipeFd[0], &readfds);
        FD_SET(beeperPipeFd[0], &readfds);

        // wait for pipe or timeout
        int res = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if(res < 0) {       // on select() error and signal received
            continue;
        }

        uint32_t now = Utils::getCurrentMs();
        bool redrawDisplay = false;

        if(res == 0) {                  // on select timeout?
            if(now >= nextScreenTime) { // did enough time pass to go to next screen?
                nextScreenTime = Utils::getEndTime(NEXT_SCREEN_INTERVAL);

                if(btnDownTime < 1) {   // if not showing recovery progress thing, redraw display
                    redrawDisplay  = true;
                }
            } else {                    // not enough time for next screen? check button state, possibly show recovery progress
                if(chipInterface) {
                    chipInterface->handleButton(btnDownTime, nextScreenTime);       // now handle the button state
                }
                continue;
            }
        }

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
                    if(beeperCommand == BEEP_RELOAD_SETTINGS) {     // if should just reload settings?
                        s.loadFloppyConfig(&floppyConfig);
                    } else {                        // some other beeper command?
                        if(chipInterface) {         // and got the chip interface?
                            chipInterface->handleBeeperCommand(beeperCommand, floppyConfig.soundEnabled);
                        }
                    }
                }
            }
        }

        // if should redraw display - on timeout or on request, do it
        if(redrawDisplay) {
            // draw screen on display
            display_drawScreen(currentScreen);

            if(currentScreen == DISP_SCREEN_POWEROFF) { // in case we've just shown power off screen, do also the power off beep
                beeper_beep(BEEP_POWER_OFF_INTERVAL);
            }

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

static void display_ce_logo(void)
{
    const static uint8_t logo[128] =
    { 0x00,0x00,0x00,0x00, 0x0f,0x80,0x01,0xf0, 0x0f,0x80,0x01,0xf0, 0x1c,0x07,0xe0,0x38, 0x18,0x38,0x1c,0x18, 0x38,0xc1,0x83,0x1c,
      0x31,0x03,0xc0,0x8c, 0x32,0x07,0xe0,0x4c, 0x02,0x01,0x80,0x40, 0x04,0x01,0x80,0x20, 0x0b,0xc1,0x83,0xd0, 0x0b,0x81,0x81,0xd0,
      0x0b,0xc1,0x83,0xd0, 0x12,0xf1,0x8f,0x48, 0x10,0x3d,0xbc,0x08, 0x10,0x0f,0xf0,0x08, 0x10,0x03,0xc0,0x08, 0x10,0x07,0xe0,0x08,
      0x10,0x0e,0x70,0x08, 0x08,0x9c,0x39,0x10, 0x08,0xb8,0x1d,0x10, 0x08,0xf0,0x0f,0x10, 0x04,0xe0,0x07,0x20, 0x02,0xf8,0x1f,0x40,
      0x32,0x00,0x00,0x4c, 0x31,0x80,0x01,0x8c, 0x38,0x40,0x02,0x1c, 0x18,0x38,0x1c,0x18, 0x1c,0x07,0xe0,0x38, 0x0f,0x80,0x01,0xf0,
      0x0f,0x80,0x01,0xf0, 0x00,0x00,0x00,0x00};

    int x,y, idx = 0, whichBit = 7;
    uint8_t byte, bit;
    for(y=0; y<32; y++) {
        for(x=0; x<32; x++) {
            if(whichBit == 7) {                             // need to get another byte?
                byte = logo[idx];
                idx++;
            }

            bit = byte & (1 << whichBit);                   // get the right bit
            whichBit = (whichBit > 0) ? (whichBit - 1) : 7; // move to next bit

            uint16_t color = bit ? SSD1306_WHITE : SSD1306_BLACK;
            display->drawPixel(x, y, color);                // draw it
        }
    }
}

#define LOGO_WIDTH          32
#define MSG_MAX_WIDTH       (SSD1306_LCDWIDTH - LOGO_WIDTH)
#define MSG_MAX_LINES       (SSD1306_LCDHEIGHT / CHAR_H)
#define MSG_MAX_LINE_LENGTH (MSG_MAX_WIDTH / CHAR_W)

static int getLineSplitLen(char *input)
{
    int len = strlen(input);            // get length of the string
    if(len <= MSG_MAX_LINE_LENGTH) {    // if it would fit completely, return whole string length
        return len;                     // could be zero on no more string
    }

    // the string will not fit in one piece, find where we can split it
    char *sepPrev = NULL;
    char *sepNext = NULL;
    sepNext = strstr(input, " ");       // find where the next separator is

    while(1) {
        if(sepNext == NULL) {           // string doesn't containt separator? return whole / part of the string
            return MIN(len, MSG_MAX_LINE_LENGTH);
        }

        len = sepNext - input;          // length of string from begining to this separator

        if(len > MSG_MAX_LINE_LENGTH) { // if the string up to this separator will NOT fit
            if(sepPrev != NULL) {       // got previous separator? return that length
                return (sepPrev - input);
            } else {                    // no previous separator? just cut it in the middle of the word
                return MSG_MAX_LINE_LENGTH; // return maximum what can be cut from this line
            }
        }

        // string still would fit
        sepPrev = sepNext;
        sepNext = strstr(sepPrev + 1, " "); // find next, starting from this one
    }
}

static int splitIntoLines(char *input, int linesCount)
{
    char *tmp = new char[MSG_MAX_LINE_LENGTH + 1];
    int lc = 0;

    int y=0;
    if(linesCount != -1) {                              // if should draw, not just count
        linesCount = MIN(linesCount, MSG_MAX_LINES);    // draw maximum 4 lines
        y = (SSD1306_LCDHEIGHT - (linesCount * CHAR_H)) / 2;    // calculate at which y should we start to be centered
    }

    while(1) {
        int len = getLineSplitLen(input);

        if(len == 0) {                                  // end of string?
            break;
        }

        if(linesCount != -1) {                          // if linesCount isn't -1, should draw on display
            int skip = (input[0] == ' ') ? 1 : 0;       // skip first if it's blank

            memset(tmp, 0, MSG_MAX_LINE_LENGTH + 1);    // clear buffer
            memcpy(tmp, input + skip, len - skip);      // copy string part

            int stringWidth = (len - skip) * CHAR_W;    // string width = char count * char width
            int x = LOGO_WIDTH + (MSG_MAX_WIDTH - stringWidth) / 2;
            gfx->drawString(x, y, tmp);                 // draw on screen
            y += CHAR_H;                                // move to next line
        }

        lc++;

        if(lc >= MSG_MAX_LINES) {                       // if it's more lines than we can draw, just quit
            break;
        }

        input += len;                                   // move forward in string
    }

    delete []tmp;
    return lc;
}

void display_print_center(char *msg)
{
    display->clearDisplay();    // clear display buffer
    display_ce_logo();          // copy CE logo into buffer

    int linesCount = splitIntoLines(msg, -1);           // call 1st time just to count lines (for y-centering)
                     splitIntoLines(msg, linesCount);   // call 2nd time to draw those lines centered

    display->display();         // show the buffer on display
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

static void fillLine_datetime(void)
{
    char humanTime[128];
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    sprintf(humanTime, "%04d-%02d-%02d    %02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
    display_setLine(DISP_LINE_DATETIME, humanTime);
}

void fillLine_recovery(int buttonDownTime)
{
    int downTime      = MIN(buttonDownTime, 15);    // limit button down time to be max 15, which is used for maximum recovery level (3)
    int recoveryLevel = downTime / 5;               // convert seconds to recovery level 0 - 3
    static int recoveryLevelPrev = 0;

    if(recoveryLevelPrev != recoveryLevel) {        // if recovery level changed, do a short beep as feedback
        beeper_beep(BEEP_SHORT);
        recoveryLevelPrev = recoveryLevel;
    }

    char tmp[32];

    // first line - current recovery level
    sprintf(tmp, "  Recovery Level %d  ", recoveryLevel);
    display_setLine(DISP_LINE_RECOVERY1, tmp);

    // second line - showing progress of button down holding vs recovery level
    memset(tmp, ' ', DISP_LINE_MAXLEN);
    tmp[DISP_LINE_MAXLEN] = 0;

    int i;
    for(i=0; i<15; i++) {       // show up to all 15 seconds / progress dashes + recovery levels
        if(i >= downTime) {
            break;
        }

        char a;
        if(((i+1) % 5) == 0) {  // for i 4,9,14 the related seconds are 5,10,15, so show 1,2,3 instead of dashes
            a = '0' + (i + 1) / 5;
        } else {                // other parts of progress - just show dash
            a = '-';
        }

        tmp[3 + i] = a;         // fill progress / number
    }

    display_setLine(DISP_LINE_RECOVERY2, tmp);
}

static void display_drawScreen(int screenIndex)
{
    static uint32_t nextFillTime = 0;
    uint32_t now = Utils::getCurrentMs();

    if(now > nextFillTime) {
        nextFillTime = Utils::getEndTime(15000);        // refresh these every 15 seconds
        fillLine_datetime();
        fillNetworkDisplayLines();
        fillTranslatedDisplayLines();
    }

    display->clearDisplay();

#ifdef DEBUG_DISPLAY
    Debug::out(LOG_DEBUG, "display_drawScreen - screenIndex: %02d -> %s", screenIndex, dispScreenToText(screenIndex));
#endif

    // get lines numbers from the specified screen number - this will be 4 indexes of lines in screenLines[]
    int *screenLines = (int *) &display_screens[screenIndex];

    int i, y;
    for(i=0; i<4; i++) {
        int screenLine = screenLines[i];        // which line we should show?
        char *line = get_displayLinePtr(screenLine);

        if(line == NULL) {
#ifdef DEBUG_DISPLAY
        Debug::out(LOG_DEBUG, "display_drawScreen - screenLine: %02d -> %-19s <= this line is EMPTY", screenLine, dispLineToText(screenLine));
#endif
            continue;
        }

#ifdef DEBUG_DISPLAY
        Debug::out(LOG_DEBUG, "display_drawScreen - screenLine: %02d -> %-19s => %s", screenLine, dispLineToText(screenLine), line);
#endif

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
    // if beep is ranging from short to long, or it's power-off interval beep
    if((beepLen >= BEEP_SHORT && beepLen <= BEEP_LONG) || (beepLen == BEEP_POWER_OFF_INTERVAL)){
        // got pipe?
        if(beeperPipeFd[1] > 0) {
            char outBfr = (char) beepLen;
            write(beeperPipeFd[1], &outBfr, 1);
        }
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

void beeper_reloadSettings(void)
{
    // got pipe?
    if(beeperPipeFd[1] > 0) {
        char outBfr = (char) BEEP_RELOAD_SETTINGS;
        write(beeperPipeFd[1], &outBfr, 1);
    }
}
