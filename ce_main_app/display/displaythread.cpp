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

extern THwConfig hwConfig;
extern TFlags    flags;
extern DebugVars dbgVars;

Adafruit_GFX *gfx;

void *displayThreadCode(void *ptr)
{
    fd_set readfds;
    int max_fd;

    Debug::out(LOG_DEBUG, "Display thread starting...");

    ssd1306_begin(SSD1306_SWITCHCAPVCC);
    ssd1306_clearDisplay();
    ssd1306_display();

    gfx = new Adafruit_GFX(SSD1306_LCDWIDTH, SSD1306_LCDHEIGHT);

    while(sigintReceived == 0) {
        max_fd = -1;
        FD_ZERO(&readfds);

        Utils::sleepMs(100);
    }
    
    ssd1306_clearDisplay();
    gfx->drawString(0, (SSD1306_LCDHEIGHT-CHAR_H)/2, "CosmosEx stopped");
    ssd1306_display();

    delete gfx;
    Debug::out(LOG_DEBUG, "Display thread terminated.");
    return 0;
}

