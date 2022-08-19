// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
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
#include "ccorethread.h"
#include "native/scsi.h"
#include "native/scsi_defs.h"
#include "update.h"

#include "periodicthread.h"
#include "display/displaythread.h"

extern THwConfig    hwConfig;
extern TFlags       flags;
extern DebugVars    dbgVars;

extern SharedObjects shared;

extern volatile uint8_t updateListDownloadStatus;

#define DEV_CHECK_TIME_MS           3000
#define UPDATE_CHECK_TIME           1000
#define INET_IFACE_CHECK_TIME       1000
#define UPDATE_SCRIPTS_TIME         10000

#define UPDATELIST_DOWNLOAD_TIME_ONFAIL     ( 1 * 60 * 1000)
#define UPDATELIST_DOWNLOAD_TIME_ONSUCCESS  (30 * 60 * 1000)

static void fillNetworkDisplayLines(void);

volatile uint32_t whenCanStartInstall;         // if this uint32_t has non-zero value, then it's a time when this app should be terminated for installing update

void *periodicThreadCode(void *ptr)
{
    uint32_t wait = 1000;          // max 1 sec, so we can often enough check for whenCanStartInstall and then terminate app for update
    uint32_t now;

    Debug::out(LOG_DEBUG, "Periodic thread starting...");

    fillNetworkDisplayLines();

    while(sigintReceived == 0) {
        now = Utils::getCurrentMs();

        // the next block is executed a couple of times after user starts update, but we need to let the app run for a second or two more to get info about update
        if(whenCanStartInstall) {               // if it's non-zero, we should check if it's time to do the update
            if(now >= whenCanStartInstall) {
                Debug::out(LOG_INFO, ">>> Terminating app, because user requests update and update was found. <<<\n");
                sigintReceived = 1;     // quit
            }
        }

        sleep(wait);
    }

    Debug::out(LOG_DEBUG, "Periodic thread terminated.");
    return 0;
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
