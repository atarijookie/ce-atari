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

static void handleConfigStreams(ConfigStream *cs, ConfigPipes &cp);
static void fillNetworkDisplayLines(void);

volatile uint32_t whenCanStartInstall;         // if this uint32_t has non-zero value, then it's a time when this app should be terminated for installing update

void *periodicThreadCode(void *ptr)
{
    fd_set readfds;
    fd_set writefds;
    int max_fd;
    struct timeval timeout;
    int inotifyFd;
    int wd = -1;
    ssize_t res;
    unsigned long wait;
    uint32_t now;

    Debug::out(LOG_DEBUG, "Periodic thread starting...");

    inotifyFd = inotify_init();
    if(inotifyFd < 0) {
        Debug::out(LOG_ERROR, "inotify_init() failed");
    } else {
        // TODO : update wd when configuration is changed
    }

    fillNetworkDisplayLines();

    while(sigintReceived == 0) {
        max_fd = -1;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        wait = 1*1000;      // max 1 sec, so we can often enough check for whenCanStartInstall and then terminate app for update

        now = Utils::getCurrentMs();

        // the next block is executed a couple of times after user starts update, but we need to let the app run for a second or two more to get info about update
        if(whenCanStartInstall) {               // if it's non-zero, we should check if it's time to do the update
            if(now >= whenCanStartInstall) {
                Debug::out(LOG_INFO, ">>> Terminating app, because user requests update and update was found. <<<\n");
                sigintReceived = 1;     // quit
            }
        }

        // file descriptors to "select"
        if(shared.configPipes.web.fd1 >= 0) {
            FD_SET(shared.configPipes.web.fd1, &readfds);
            if(shared.configPipes.web.fd1 > max_fd) max_fd = shared.configPipes.web.fd1;
        }
        if(shared.configPipes.term.fd1 >= 0) {
            FD_SET(shared.configPipes.term.fd1, &readfds);
            if(shared.configPipes.term.fd1 > max_fd) max_fd = shared.configPipes.term.fd1;
        }
        if(inotifyFd >= 0) {
            FD_SET(inotifyFd, &readfds);
            if(inotifyFd > max_fd) max_fd = inotifyFd;
        }

        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = wait / 1000;           // sec
        timeout.tv_usec = (wait % 1000)*1000;   // usec
        if(select(max_fd + 1, &readfds, &writefds, NULL, &timeout) < 0) {
            if(errno == EINTR) {
                continue;
            } else {
                Debug::out(LOG_ERROR, "periodic thread : select() %s", strerror(errno));
                continue;
            }
        }

        //------------------------------------
        // config streams handling
        if(shared.configPipes.web.fd1 >= 0 && FD_ISSET(shared.configPipes.web.fd1, &readfds)) {
            handleConfigStreams(shared.configStream.web,  shared.configPipes.web);
        }
        if(shared.configPipes.term.fd1 >= 0 && FD_ISSET(shared.configPipes.term.fd1, &readfds)) {
            handleConfigStreams(shared.configStream.term, shared.configPipes.term);
        }
    }

    Debug::out(LOG_DEBUG, "Periodic thread terminated.");
    return 0;
}

static void handleConfigStreams(ConfigStream *cs, ConfigPipes &cp)
{
    if(cp.fd1 < 0 || cp.fd2 < 0) {                              // missing handle? fail
        return;
    }

    int bytesAvailable;
    int ires = ioctl(cp.fd1, FIONREAD, &bytesAvailable);       // how many bytes we can read?

    if(ires != -1 && bytesAvailable >= 3) {                 // if there are at least 3 bytes waiting
        uint8_t cmd[6] = {0, 'C', 'E', 0, 0, 0};
        ires = read(cp.fd1, cmd + 3, 3);                       // read the byte triplet

        Debug::out(LOG_DEBUG, "confStream - through FIFO: %02x %02x %02x (ires = %d)", cmd[3], cmd[4], cmd[5], ires);

        pthread_mutex_lock(&shared.mtxConfigStreams);
        cs->processCommand(cmd, cp.fd2);
        pthread_mutex_unlock(&shared.mtxConfigStreams);
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
