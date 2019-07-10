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
#include "gpio.h"
#include "mounter.h"
#include "downloader.h"
#include "update.h"
#include "ce_conf_on_rpi.h"

#include "devfinder.h"
#include "ifacewatcher.h"
#include "timesync.h"

#include "periodicthread.h"
#include "display/displaythread.h"

extern THwConfig    hwConfig;
extern TFlags       flags;
extern DebugVars    dbgVars;

extern SharedObjects shared;

extern volatile BYTE updateListDownloadStatus;

#define DEV_CHECK_TIME_MS           3000
#define UPDATE_CHECK_TIME           1000
#define INET_IFACE_CHECK_TIME       1000
#define UPDATE_SCRIPTS_TIME         10000

#define UPDATELIST_DOWNLOAD_TIME_ONFAIL     ( 1 * 60 * 1000)
#define UPDATELIST_DOWNLOAD_TIME_ONSUCCESS  (30 * 60 * 1000)

static void handleConfigStreams(ConfigStream *cs, ConfigPipes &cp);
static void fillNetworkDisplayLines(void);

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
    DWORD now;

    Debug::out(LOG_DEBUG, "Periodic thread starting...");

    ce_conf_createFifos();                                                  // if should run normally, create the ce_conf FIFOs

    inotifyFd = inotify_init();
    if(inotifyFd < 0) {
        Debug::out(LOG_ERROR, "inotify_init() failed");
    } else {
        // TODO : update wd when configuration is changed
        const char * path = shared.mountRawNotTrans ? DISK_LINKS_PATH_ID : DISK_LINKS_PATH_UUID;
        wd = inotify_add_watch(inotifyFd, path, IN_CREATE|IN_DELETE);
        if(wd < 0) Debug::out(LOG_ERROR, "inotify_add_watch(%s, IN_CREATE|IN_DELETE) failed", path);
    }

    fillNetworkDisplayLines();

    DevFinder devFinder;
    devFinder.lookForDevChanges();                          // look for devices attached / detached

    IfaceWatcher ifaceWatcher;

    TimeSync timeSync;

    while(sigintReceived == 0) {
        max_fd = -1;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        wait = 10*1000; // max 10 sec ?

        now = Utils::getCurrentMs();

        //------------------------------------
        // should we do something related to devFinder?
        if(shared.devFinder_detachAndLook || shared.devFinder_look) {   // detach devices & look for them again, or just look for them?
            if(shared.devFinder_detachAndLook) {                    // if should also detach, do it
                TranslatedDisk * translated = TranslatedDisk::getInstance();
                pthread_mutex_lock(&shared.mtxScsi);
                if(translated) translated->mutexLock();

                if(translated) translated->detachAllUsbMedia();             // detach all translated USB media
                shared.scsi->detachAllUsbMedia();                   // detach all RAW USB media

                pthread_mutex_unlock(&shared.mtxScsi);
                if(translated) translated->mutexUnlock();
            }

            // and now try to attach everything back
            devFinder.clearMap();                                   // make all the devices appear as new
            devFinder.lookForDevChanges();                          // and now find all the devices

            shared.devFinder_detachAndLook  = false;
            shared.devFinder_look           = false;
            continue;
        }

        if(now >= timeSync.nextProcessTime) {
            timeSync.process(false);
        } else {
            if(now + wait > timeSync.nextProcessTime) wait = timeSync.nextProcessTime - now;
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
        if(ifaceWatcher.getFd() >= 0) {
            FD_SET(ifaceWatcher.getFd(), &readfds);
            if(ifaceWatcher.getFd() > max_fd) max_fd = ifaceWatcher.getFd();
        }
        if(timeSync.waitingForDataOnFd()) {
            FD_SET(timeSync.getFd(), &readfds);
            if(timeSync.getFd() > max_fd) max_fd = timeSync.getFd();
        }
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = wait / 1000;           // sec
        timeout.tv_usec = (wait % 1000)*1000;   // usec
        if(select(max_fd + 1, &readfds, &writefds, NULL, &timeout) < 0) {
            if(errno == EINTR) {
                continue;
            } else {
                Debug::out(LOG_ERROR, "peridic thread : select() %s", strerror(errno));
                continue;
            }
        }

        //------------------------------------
        // Network interface watcher
        if(ifaceWatcher.getFd() >= 0 && FD_ISSET(ifaceWatcher.getFd(), &readfds)) {
            bool newIfaceUpAndRunning = false;
            ifaceWatcher.processMsg(&newIfaceUpAndRunning);
            if(newIfaceUpAndRunning) {
                Debug::out(LOG_DEBUG, "Internet interface comes up: reload network mount settings");
                TranslatedDisk * translated = TranslatedDisk::getInstance();
                if(translated) {
                    translated->mutexLock();
                    translated->reloadSettings(SETTINGSUSER_TRANSLATED);
                    translated->mutexUnlock();
                }

                Debug::out(LOG_DEBUG, "periodicThreadCode -- eth0 or wlan0 changed to up, will now download update list");
            }

            // fill network into for display as we might have new address or something
            fillNetworkDisplayLines();
        }

        //------------------------------------
        // should we check for the new devices?
        if(inotifyFd >= 0 && FD_ISSET(inotifyFd, &readfds)) {
            char buf[sizeof(struct inotify_event) + NAME_MAX + 1];
            res = read(inotifyFd, buf, sizeof(buf));
            if(res < 0) {
                Debug::out(LOG_ERROR, "read(inotifyFd) : %s", strerror(errno));
            } else {
                struct inotify_event *iev = (struct inotify_event *)buf;
                Debug::out(LOG_DEBUG, "inotify msg %dbytes wd=%d mask=%04x name=%s", (int)res, iev->wd, iev->mask, (iev->len > 0) ? iev->name : "");
                // if(iev->mask & IN_DELETE) / & IN_CREATE
                devFinder.lookForDevChanges();                          // look for devices attached / detached
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
        //------------------------------------

        if(timeSync.waitingForDataOnFd() && FD_ISSET(timeSync.getFd(), &readfds)) {
            timeSync.process(true);
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
        BYTE cmd[6] = {0, 'C', 'E', 0, 0, 0};
        ires = read(cp.fd1, cmd + 3, 3);                       // read the byte triplet

        Debug::out(LOG_DEBUG, "confStream - through FIFO: %02x %02x %02x (ires = %d)", cmd[3], cmd[4], cmd[5], ires);

        pthread_mutex_lock(&shared.mtxConfigStreams);
        cs->processCommand(cmd, cp.fd2);
        pthread_mutex_unlock(&shared.mtxConfigStreams);
    }
}

static void fillNetworkDisplayLines(void)
{
    BYTE bfr[10];
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
