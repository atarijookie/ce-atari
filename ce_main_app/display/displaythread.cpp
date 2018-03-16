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

void display_init(void);
void display_deinit(void);
void display_print_center(char *str);

void *displayThreadCode(void *ptr)
{
    fd_set readfds;
    int max_fd;

    Debug::out(LOG_DEBUG, "Display thread starting...");

    display_init();

    gfx = new Adafruit_GFX(SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT);    // font displaying library

    while(sigintReceived == 0) {
        max_fd = -1;
        FD_ZERO(&readfds);

        Utils::sleepMs(100);
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

void display_print_center(char *str)
{
    int len = strlen(str);
    int x = (SSD1306_LCDWIDTH - (CHAR_W * len)) / 2;
    int y = (SSD1306_LCDHEIGHT - CHAR_H)/2;

    ssd1306_clearDisplay();
    gfx->drawString(x, y, str);
    ssd1306_display();
}
