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
#include "config/netsettings.h"
#include "ce_conf_on_rpi.h"

#include "devfinder.h"
#include "ifacewatcher.h"
#include "timesync.h"

#include "periodicthread.h"

extern THwConfig    hwConfig;
extern TFlags       flags;
extern DebugVars    dbgVars;

extern SharedObjects shared;

extern volatile BYTE updateListDownloadStatus;

#define DEV_CHECK_TIME_MS	        3000
#define UPDATE_CHECK_TIME           1000
#define INET_IFACE_CHECK_TIME       1000
#define UPDATE_SCRIPTS_TIME         10000

#define UPDATELIST_DOWNLOAD_TIME_ONFAIL     ( 1 * 60 * 1000)
#define UPDATELIST_DOWNLOAD_TIME_ONSUCCESS  (30 * 60 * 1000)

bool state_eth0     = false;
bool state_wlan0    = false;

static void handleConfigStreams(ConfigStream *cs, ConfigPipes &cp);
static void updateUpdateState(void);

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

    DWORD nextUpdateCheckTime           = Utils::getEndTime(5000);          // create a time when update download status should be checked

    DWORD nextUpdateListDownloadTime    = Utils::getEndTime(3000);          // try to download update list at this time

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

    DevFinder devFinder;
	devFinder.lookForDevChanges();				            // look for devices attached / detached

    IfaceWatcher ifaceWatcher;

    TimeSync timeSync;

	while(sigintReceived == 0) {
        max_fd = -1;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        wait = 10*1000; // max 10 sec ?

        now = Utils::getCurrentMs();

        //------------------------------------
        // check client command timestamp (we don't really care how often its done)
        shared.clientConnected = (now - shared.configStream.acsi->getLastCmdTimestamp() <= 2000);

        //------------------------------------
        // should we do something related to devFinder?
        if(shared.devFinder_detachAndLook || shared.devFinder_look) {   // detach devices & look for them again, or just look for them?
            if(shared.devFinder_detachAndLook) {                    // if should also detach, do it
                pthread_mutex_lock(&shared.mtxScsi);
                pthread_mutex_lock(&shared.mtxTranslated);

                shared.translated->detachAllUsbMedia();             // detach all translated USB media
                shared.scsi->detachAllUsbMedia();                   // detach all RAW USB media

                pthread_mutex_unlock(&shared.mtxScsi);
                pthread_mutex_unlock(&shared.mtxTranslated);
            }

            // and now try to attach everything back
            devFinder.clearMap();						            // make all the devices appear as new
            devFinder.lookForDevChanges();					        // and now find all the devices

            shared.devFinder_detachAndLook  = false;
            shared.devFinder_look           = false;
            continue;
        }

        if(now >= timeSync.nextProcessTime) {
            timeSync.process(false);
        } else {
            if(now + wait > timeSync.nextProcessTime) wait = timeSync.nextProcessTime - now;
        }

        if(now >= nextUpdateListDownloadTime) {
            Debug::out(LOG_DEBUG, "periodicThreadCode -- will download update list now");
            Update::downloadUpdateList(NULL);                                                           // download the list of components with the newest available versions

            nextUpdateListDownloadTime = Utils::getEndTime(UPDATELIST_DOWNLOAD_TIME_ONSUCCESS);         // set the next download time in the future, so we won't do this immediately again
            continue;
        } else {
            if(now + wait > nextUpdateListDownloadTime) wait = nextUpdateListDownloadTime - now;
        }

        //------------------------------------
        // should check the update status?
        if(now >= nextUpdateCheckTime) {
            nextUpdateCheckTime   = Utils::getEndTime(UPDATE_CHECK_TIME);                               // update the time when we should check update status again

            if(updateListDownloadStatus == DWNSTATUS_DOWNLOAD_FAIL) {                                   // download of update list fail?
                updateListDownloadStatus    = DWNSTATUS_WAITING;
                nextUpdateListDownloadTime  = Utils::getEndTime(UPDATELIST_DOWNLOAD_TIME_ONFAIL);       // set the next download time in the future, so we won't do this immediately again
                Debug::out(LOG_DEBUG, "periodicThreadCode -- update list download failed, will retry in %d seconds", UPDATELIST_DOWNLOAD_TIME_ONFAIL / 1000);
            } else if(updateListDownloadStatus == DWNSTATUS_DOWNLOAD_OK) {                                     // download of update list good?
                updateListDownloadStatus    = DWNSTATUS_WAITING;
                nextUpdateListDownloadTime  = Utils::getEndTime(UPDATELIST_DOWNLOAD_TIME_ONSUCCESS);    // try to download the update list in longer time again
                Debug::out(LOG_DEBUG, "periodicThreadCode -- update list download good, will retry in %d minutes", UPDATELIST_DOWNLOAD_TIME_ONSUCCESS / (60 * 1000));

                if(!Update::versions.updateListWasProcessed) {                                          // Didn't process update list yet? process it
                    Update::processUpdateList();
                }

                if(Update::versions.updateListWasProcessed) {                                           // if we processed the list, update config stream
                    // if the config screen is shown, then update info on it
                    pthread_mutex_lock(&shared.mtxConfigStreams);

                    shared.configStream.acsi->fillUpdateWithCurrentVersions();
                    shared.configStream.web->fillUpdateWithCurrentVersions();
                    shared.configStream.term->fillUpdateWithCurrentVersions();

                    pthread_mutex_unlock(&shared.mtxConfigStreams);
                }
            }

            updateUpdateState();
            continue;
        } else {
            if(now + wait > nextUpdateCheckTime) wait = nextUpdateCheckTime - now;
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
                pthread_mutex_lock(&shared.mtxTranslated);
                shared.translated->reloadSettings(SETTINGSUSER_TRANSLATED);
                pthread_mutex_unlock(&shared.mtxTranslated);

                Debug::out(LOG_DEBUG, "periodicThreadCode -- eth0 or wlan0 changed to up, will now download update list");
                nextUpdateListDownloadTime = Utils::getEndTime(1000);
            }
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
			    devFinder.lookForDevChanges();				            // look for devices attached / detached
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

static void updateUpdateState(void)
{
    int updateState = Update::state();                              // get the update state

    switch(updateState) {
        case UPDATE_STATE_DOWNLOADING:
        pthread_mutex_lock(&shared.mtxConfigStreams);

        // refresh config screen with download status
        shared.configStream.acsi->fillUpdateDownloadWithProgress();
        shared.configStream.web->fillUpdateDownloadWithProgress();
        shared.configStream.term->fillUpdateDownloadWithProgress();

        pthread_mutex_unlock(&shared.mtxConfigStreams);
        break;

        //-----------
        case UPDATE_STATE_DOWNLOAD_FAIL:
        pthread_mutex_lock(&shared.mtxConfigStreams);

        // show fail message on config screen
        shared.configStream.acsi->showUpdateDownloadFail();
        shared.configStream.web->showUpdateDownloadFail();
        shared.configStream.term->showUpdateDownloadFail();

        pthread_mutex_unlock(&shared.mtxConfigStreams);

        Update::stateGoIdle();
        Debug::out(LOG_ERROR, "Update state - download failed");
        break;

        //-----------
        case UPDATE_STATE_DOWNLOAD_OK:
        {
            pthread_mutex_lock(&shared.mtxConfigStreams);

            // check if any of the config streams shows download page
            bool shownA = shared.configStream.acsi->isUpdateDownloadPageShown();
            bool shownW = shared.configStream.web->isUpdateDownloadPageShown();
            bool shownT = shared.configStream.term->isUpdateDownloadPageShown();

            pthread_mutex_unlock(&shared.mtxConfigStreams);

            if(!shownA && !shownW && !shownT) {         // if user is NOT waiting on download page (cancel pressed), don't update
                Update::stateGoIdle();
                Debug::out(LOG_DEBUG, "Update state - download OK, but user is not on download page - NOT doing update");
            } else {                                    // if user is waiting on download page, apply update
                bool res = Update::createUpdateScript();

                if(!res) {                              // if failed to create update script, fail and go to update idle state
                    pthread_mutex_lock(&shared.mtxConfigStreams);

                    shared.configStream.acsi->showUpdateError();
                    shared.configStream.web->showUpdateError();
                    shared.configStream.term->showUpdateError();

                    pthread_mutex_unlock(&shared.mtxConfigStreams);

                    Debug::out(LOG_ERROR, "Update state - download OK, failed to create update script - NOT doing update");
                    Update::stateGoIdle();
                } else {                                // update downloaded, user is on download page, update script created - now wait a while before doing the install
                    Debug::out(LOG_INFO, "Update state - download OK, update script created, will wait before install");
                    Update::stateGoWaitBeforeInstall();
                }
            }
        break;
        }

        //---------
        // are we waiting a while before the install?
        case UPDATE_STATE_WAITBEFOREINSTALL:
            shared.configStream.acsi->fillUpdateDownloadWithFinish();
            shared.configStream.web->fillUpdateDownloadWithFinish();
            shared.configStream.term->fillUpdateDownloadWithFinish();

            if(Update::canStartInstall()) {
                Debug::out(LOG_INFO, "Update state - waiting before install - can start install, will quit and install!");
                sigintReceived = 1;
            } else {
                Debug::out(LOG_INFO, "Update state - waiting before install, not starting install yet");
            }
        break;
    }
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
