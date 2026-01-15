// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <sys/select.h>
#include <sys/inotify.h>
#include <limits.h>
#include <signal.h>
#include <termios.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>

#include "../misc/global.h"
#include "../misc/debug.h"
#include "../misc/utils.h"
#include "../misc/settings.h"
#include "../chipinterface/chipinterface.h"

#include "ikbd.h"

TInputDevice ikbdDevs[INTYPE_MAX+1];

void *ikbdThreadCode(void *ptr)
{
    int max_fd;
    int fd;
    fd_set readfds;
    /*struct timeval timeout;*/
    int i;
    int inotifyFd;
    int wd1, wd2, wd3;
    ssize_t res;

    logIkbd(LOG_DEBUG, "----------------------------------------------------------");
    logIkbd(LOG_DEBUG, "ikbdThreadCode will enter loop...");

    // create network chip interface
    ChipInterface* ciIkbd = new ChipInterface(LOGFILE_FDD, NET_ATN_FRANZ_ID, SYNC_TAG_FDD);
    ciIkbd->ciOpen();

    Ikbd ikbd(ciIkbd);

    inotifyFd = inotify_init();
    if(inotifyFd < 0) {
        logIkbd(LOG_ERROR, "inotify_init() failed");
    } else {
        wd1 = inotify_add_watch(inotifyFd, "/dev/input", IN_CREATE);
        if(wd1 < 0) logIkbd(LOG_ERROR, "inotify_add_watch(/dev/input, IN_CREATE) failed");
        wd2 = inotify_add_watch(inotifyFd, "/dev/input/by-path", IN_CREATE | IN_DELETE_SELF);
        if(wd2 < 0) logIkbd(LOG_ERROR, "inotify_add_watch(/dev/input/by-path, IN_CREATE | IN_DELETE_SELF)");

        std::string vdevFolder = Utils::dotEnvValue("IKBD_VIRTUAL_DEVICES_PATH");
        wd3 = inotify_add_watch(inotifyFd, vdevFolder.c_str(), IN_CREATE);
        if(wd3 < 0) logIkbd(LOG_ERROR, "inotify_add_watch('%s', IN_CREATE)", vdevFolder.c_str());
    }

    ikbd.findDevices();
    ikbd.findVirtualDevices();

    while(sigintReceived == 0) {
        Utils::sleepMs(1);      // intentional sleep to not utilize cpu to max when looping too much before client disconnect

        // reload config if needed
        if(events.loadIkbdConfig) {
            events.loadIkbdConfig = false;
            ikbd.loadSettings();
        }

        ciIkbd->clientsDisconnectInactive();

        max_fd = -1;
        FD_ZERO(&readfds);
        for(i = 0; i < 6; i++) {                                       // go through the input devices
            fd = ikbd.getFdByIndex(i);
            if(fd >= 0) {
                FD_SET(fd, &readfds);
                if(fd > max_fd) max_fd = fd;
            }
        }

        // get listening socket from chip interface, add it to readfds
        int fdListen = ciIkbd->getFdListen();
        FD_SET(fdListen, &readfds);
        max_fd = MAX(max_fd, fdListen);

        // get all connected client fds, add them to readfds
        max_fd = MAX(max_fd, ciIkbd->setAllClientFds(&readfds));     // all valid client fds will be set to readfds, and highest fd into max_fd

        if(inotifyFd >= 0) {
            FD_SET(inotifyFd, &readfds);
            if(inotifyFd > max_fd) max_fd = inotifyFd;
        }

        // add timeout to select(), so we can check for connection status, settings reload, etc.
        timeval timeout;
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 2;

        if(select(max_fd + 1, &readfds, NULL, NULL, &timeout) < 0) {
            if(errno == EINTR) {
                continue;   // a signal was delivered
            } else {
                logIkbd(LOG_ERROR, "ikbdThreadCode() select: %s", strerror(errno));
                continue;
            }
        }

        if(inotifyFd >= 0 && FD_ISSET(inotifyFd, &readfds)) {
            char buf[sizeof(struct inotify_event) + NAME_MAX + 1];
            res = read(inotifyFd, buf, sizeof(buf));
            if(res < 0) {
                logIkbd(LOG_ERROR, "read(inotifyFd) : %s", strerror(errno));
            } else {
                struct inotify_event *iev = (struct inotify_event *)buf;
                logIkbd(LOG_DEBUG, "inotify msg %dbytes wd=%d mask=%04x name=%s", (int)res, iev->wd, iev->mask, (iev->len > 0) ? iev->name : "");
                if(iev->wd == wd1) {
                    if(iev->len > 0 && (0 == strcmp(iev->name, "by-path"))) {
                        wd2 = inotify_add_watch(inotifyFd, "/dev/input/by-path", IN_CREATE | IN_DELETE_SELF);
                        if(wd2 < 0) logIkbd(LOG_ERROR, "inotify_add_watch(/dev/input/by-path, IN_CREATE | IN_DELETE_SELF)");
                    }
                } else if(iev->wd == wd2) {
                    if(iev->mask & IN_DELETE_SELF) {
                        inotify_rm_watch(inotifyFd, wd2);
                        wd2 = -1;
                    } else {
                        // look for new input devices
                        ikbd.findDevices();
                    }
                } else if(iev->wd == wd3) {
                    // look for new input devices
                    ikbd.findVirtualDevices();
                }
            }
        }

        // if listening socket is set, handle it
        if(FD_ISSET(fdListen, &readfds)) {
            ciIkbd->acceptSocketIfNeededAndPossible();
        }

        // process the incoming data from original keyboard and from ST
        bool clientConnected = false;

        for(int i=0; i<MAX_CLIENTS; i++) {
            ClientInfo* ci = ciIkbd->clientGetByIndex(i);

            if(ci->fdClient == FD_EMPTY) {           // no client here? skip it
                continue;
            }

            if(FD_ISSET(ci->fdClient, &readfds)) {           // this fd read for read?
                // process the incoming data from original keyboard and from ST
                int bytesRead = ikbd.processReceivedCommands(clientConnected, ci->fdClient);

                if(bytesRead > 0) {     // something was read, mark client as active
                    ci->lastMs = Utils::getCurrentMs();
                }
            }
        }

        // process events from attached input devices
        struct input_event  ev;
        struct js_event     js;

        for(i = 0; i < 6; i++) {                                        // go through the input devices
            fd = ikbd.getFdByIndex(i);

            if(fd >= 0 && FD_ISSET(fd, &readfds)) {
                switch(i) {
                case INTYPE_MOUSE:
                case INTYPE_KEYBOARD: // for keyboard and mouse
                case INTYPE_VDEVMOUSE:
                case INTYPE_VDEVKEYBOARD: // for virtual mouse and keyboard
                    res = read(ikbd.getFdByIndex(i), &ev, sizeof(input_event));
                    break;
                case INTYPE_JOYSTICK1:
                case INTYPE_JOYSTICK2: // for joysticks
                    res = read(ikbd.getFdByIndex(i), &js, sizeof(js_event));
                    break;
                }
                if(res < 0) {                                           // on error, skip the rest
                    if(errno == ENODEV) {                               // if device was removed, deinit it
                        ikbd.deinitDev(i);
                    } else {
                        logIkbd(LOG_ERROR, "ikbdThreadCode() read(%d) : %s", fd, strerror(errno));
                    }
                } else if( res==0 ) {                                           // on error, skip the rest
                    logIkbd(LOG_ERROR, "ikbdThreadCode() read(%d) returned 0 (EOF) closing %d", fd, i);
                    ikbd.deinitDev(i);
                } else {
                    switch(i) {
                    case INTYPE_VDEVMOUSE:
                        ikbd.markVirtualMouseEvenTime();                // first mark the event time
                    case INTYPE_MOUSE:
                        ikbd.processMouse(&ev);                         // then process the event
                        break;
                    case INTYPE_KEYBOARD:
                    case INTYPE_VDEVKEYBOARD:
                        ikbd.processKeyboard(&ev, clientConnected);
                        break;
                    case INTYPE_JOYSTICK1:
                    case INTYPE_JOYSTICK2:
                        ikbd.processJoystick(&js, i - INTYPE_JOYSTICK1);
                        break;
                    }
                }
            }
        }
    }

    if(inotifyFd >= 0) {
        close(inotifyFd);
    }
    ikbd.closeDevs();

    ciIkbd->ciClose();
    delete ciIkbd;
    ciIkbd = NULL;

    logIkbd(LOG_DEBUG, "ikbdThreadCode has quit");
    return 0;
}
