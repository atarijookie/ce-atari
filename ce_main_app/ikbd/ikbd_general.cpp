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

#include "global.h"
#include "debug.h"
#include "gpio.h"
#include "utils.h"
#include "settings.h"
#include "datatypes.h"
#include "periodicthread.h"

#include "ikbd.h"

TInputDevice ikbdDevs[INTYPE_MAX+1];

extern volatile sig_atomic_t    sigintReceived;
       volatile bool            do_loadIkbdConfig = false;

extern TFlags flags;                                // global flags from command line
extern SharedObjects shared;

void *ikbdThreadCode(void *ptr)
{
    struct termios    termiosStruct;
    Ikbd ikbd;
    int max_fd;
    int fd;
    fd_set readfds;
    /*struct timeval timeout;*/
    int i;
    int inotifyFd;
    int wd1, wd2, wd3;
    ssize_t res;

    ikbdLog("----------------------------------------------------------");
    ikbdLog("ikbdThreadCode will enter loop...");

    bcm2835_gpio_write(PIN_TX_SEL1N2, HIGH);        // TX_SEL1N2, switch the RX line to receive from Franz, which does the 9600 to 7812 baud translation

    // open and set up uart
    if(ikbd.serialSetup(&termiosStruct) == -1) {
        logDebugAndIkbd(LOG_ERROR, "ikbd.serialSetup failed, won't be able to send IKDB data");
    }

    inotifyFd = inotify_init();
    if(inotifyFd < 0) {
        logDebugAndIkbd(LOG_ERROR, "inotify_init() failed");
    } else {
        wd1 = inotify_add_watch(inotifyFd, "/dev/input", IN_CREATE);
        if(wd1 < 0) Debug::out(LOG_ERROR, "inotify_add_watch(/dev/input, IN_CREATE) failed");
        wd2 = inotify_add_watch(inotifyFd, "/dev/input/by-path", IN_CREATE | IN_DELETE_SELF);
        if(wd2 < 0) Debug::out(LOG_ERROR, "inotify_add_watch(/dev/input/by-path, IN_CREATE | IN_DELETE_SELF)");
        wd3 = inotify_add_watch(inotifyFd, "/tmp/vdev", IN_CREATE);
        if(wd3 < 0) Debug::out(LOG_ERROR, "inotify_add_watch(/tmp/vdev, IN_CREATE)");
    }

    ikbd.findDevices();
    ikbd.findVirtualDevices();

    while(sigintReceived == 0) {
        // reload config if needed
        if(do_loadIkbdConfig) {
            do_loadIkbdConfig = false;
            ikbd.loadSettings();
        }

        max_fd = -1;
        FD_ZERO(&readfds);
        for(i = 0; i < 6; i++) {                                       // go through the input devices
            fd = ikbd.getFdByIndex(i);
            if(fd >= 0) {
                FD_SET(fd, &readfds);
                if(fd > max_fd) max_fd = fd;
            }
        }
        if(ikbd.fdUart >= 0) {
            FD_SET(ikbd.fdUart, &readfds);
            if(ikbd.fdUart > max_fd) max_fd = ikbd.fdUart;
        }
        if(inotifyFd >= 0) {
            FD_SET(inotifyFd, &readfds);
            if(inotifyFd > max_fd) max_fd = inotifyFd;
        }
        //memset(&timeout, 0, sizeof(timeout));
        //timeout.tv_sec = 3;
        if(select(max_fd + 1, &readfds, NULL, NULL, NULL/*&timeout*/) < 0) {
            if(errno == EINTR) {
                continue;   // a signal was delivered
            } else {
                Debug::out(LOG_ERROR, "ikbdThreadCode() select: %s", strerror(errno));
                continue;
            }
        }

        if(inotifyFd >= 0 && FD_ISSET(inotifyFd, &readfds)) {
            char buf[sizeof(struct inotify_event) + NAME_MAX + 1];
            res = read(inotifyFd, buf, sizeof(buf));
            if(res < 0) {
                Debug::out(LOG_ERROR, "read(inotifyFd) : %s", strerror(errno));
            } else {
                struct inotify_event *iev = (struct inotify_event *)buf;
                Debug::out(LOG_DEBUG, "inotify msg %dbytes wd=%d mask=%04x name=%s", (int)res, iev->wd, iev->mask, (iev->len > 0) ? iev->name : "");
                if(iev->wd == wd1) {
                    if(iev->len > 0 && (0 == strcmp(iev->name, "by-path"))) {
                        wd2 = inotify_add_watch(inotifyFd, "/dev/input/by-path", IN_CREATE | IN_DELETE_SELF);
                        if(wd2 < 0) Debug::out(LOG_ERROR, "inotify_add_watch(/dev/input/by-path, IN_CREATE | IN_DELETE_SELF)");
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

        if(ikbd.fdUart >= 0 && FD_ISSET(ikbd.fdUart, &readfds)) {
            // process the incomming data from original keyboard and from ST
            ikbd.processReceivedCommands(shared.clientConnected);
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
                        logDebugAndIkbd(LOG_ERROR, "ikbdThreadCode() read(%d) : %s", fd, strerror(errno));
                    }
                } else if( res==0 ) {                                           // on error, skip the rest
                    logDebugAndIkbd(LOG_ERROR, "ikbdThreadCode() read(%d) returned 0", fd);
                } else {
                    switch(i) {
                    case INTYPE_VDEVMOUSE:
                        ikbd.markVirtualMouseEvenTime();                // first mark the event time
                    case INTYPE_MOUSE:
                        ikbd.processMouse(&ev);                         // then process the event
                        break;
                    case INTYPE_KEYBOARD:
                    case INTYPE_VDEVKEYBOARD:
                        ikbd.processKeyboard(&ev, shared.clientConnected);
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

    logDebugAndIkbd(LOG_DEBUG, "ikbdThreadCode has quit");
    return 0;
}

Ikbd::Ikbd()
{
    loadSettings();

    initDevs();

    keyJoyKeys.setKeyTranslator(&keyTranslator);        // first set the translator
    keyJoyKeys.loadKeys();                              // load the keys used for keyb joys

    ceIkbdMode = CE_IKBDMODE_SOLO;

    fdUart      = -1;
    mouseBtnNow = 0;

    // init uart RX cyclic buffers
    cbStCommands.init();
    cbKeyboardData.init();

    gotHalfPair     = false;
    halfPairData    = 0;

    fillSpecialCodeLengthTable();
    fillStCommandsLengthTable();

    resetInternalIkbdVars();
}

void Ikbd::loadSettings(void)
{
    Settings s;
    firstJoyIs0 = s.getBool((char *) "JOY_FIRST_IS_0", false);

    if(firstJoyIs0) {
        joy1st = INTYPE_JOYSTICK1;
        joy2nd = INTYPE_JOYSTICK2;
    } else {
        joy1st = INTYPE_JOYSTICK2;
        joy2nd = INTYPE_JOYSTICK1;
    }

    // get enabled flags for mouse wheel as keys
    mouseWheelAsArrowsUpDown = s.getBool((char *) "MOUSE_WHEEL_AS_KEYS", true);

    // get enabled flags for keyb joys
    keybJoy0 = s.getBool("KEYBORD_JOY0", false);
    keybJoy1 = s.getBool("KEYBORD_JOY1", false);

    keyJoyKeys.setKeyTranslator(&keyTranslator);        // first set the translator
    keyJoyKeys.loadKeys();                              // load the keys used for keyb joys
}

void Ikbd::resetInternalIkbdVars(void)
{
    outputEnabled   = true;

    mouseMode       = MOUSEMODE_REL;
    mouseEnabled    = true;
    mouseY0atTop    = true;
    mouseAbsBtnAct  = MOUSEBTN_REPORT_NOTHING;

    absMouse.maxX       = 640;
    absMouse.maxY       = 400;
    absMouse.x          = 0;
    absMouse.y          = 0;
    absMouse.buttons    = 0;
    absMouse.scaleX        = 1;
    absMouse.scaleY        = 1;

    relMouse.threshX    = 1;
    relMouse.threshY    = 1;

    keycodeMouse.deltaX    = 0;
    keycodeMouse.deltaY    = 0;

    joystickMode    = JOYMODE_EVENT;
    joystickState   = EnabledInMouseMode;

    leftShiftsPressed  = 0;
    rightShiftsPressed = 0;
    ctrlsPressed       = 0;
    f11sPressed        = 0;
    f12sPressed        = 0;
    waitingForHotkeyRelease = false;
}

int Ikbd::serialSetup(termios *ts)
{
    int fd;

    fdUart = -1;

    fd = open(UARTFILE, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
    if(fd == -1) {
        logDebugAndIkbd(LOG_ERROR, "Failed to open %s", UARTFILE);
        return -1;
    }

    fcntl(fd, F_SETFL, 0);
    tcgetattr(fd, ts);

    /* reset the settings */
    cfmakeraw(ts);
    ts->c_cflag &= ~(CSIZE | CRTSCTS);
    ts->c_iflag &= ~(IXON | IXOFF | IXANY | IGNPAR);
    ts->c_lflag &= ~(ECHOK | ECHOCTL | ECHOKE);
    ts->c_oflag &= ~(OPOST | ONLCR);

    /* setup the new settings */
    cfsetispeed(ts, B19200);
    cfsetospeed(ts, B19200);
    ts->c_cflag |=  CS8 | CLOCAL | CREAD;            // uart: 8N1

    ts->c_cc[VMIN ] = 0;
    ts->c_cc[VTIME] = 0;

    /* set the settings */
    tcflush(fd, TCIFLUSH);

    if (tcsetattr(fd, TCSANOW, ts) != 0) {
        close(fd);
        return -1;
    }

    /* confirm they were set */
    struct termios settings;
    tcgetattr(fd, &settings);
    if (settings.c_iflag != ts->c_iflag ||
        settings.c_oflag != ts->c_oflag ||
        settings.c_cflag != ts->c_cflag ||
        settings.c_lflag != ts->c_lflag) {
        close(fd);
        return -1;
    }

    fcntl(fd, F_SETFL, FNDELAY);                    // make reading non-blocking

    fdUart = fd;
    return fd;
}

int Ikbd::fdWrite(int fd, BYTE *bfr, int cnt)
{
#if !defined(IKBDSPY)
    if(flags.ikbdLogs) {
        std::string text = "sending to ST: ";
        char tmp[16];

        for(int i=0; i<cnt; i++) {
            sprintf(tmp, "%02x ", bfr[i]);
            text += tmp;
        }
        ikbdLog(text.c_str());
    }
#endif

    if(fd == -1) {                                  // no fd? quit
        return 0;
    }

    if(!outputEnabled) {                            // output not enabled? Pretend that it was sent...
        return cnt;
    }

    int res = write(fd, bfr, cnt);                  // send content
    return res;
}

void ikbdLog(const char *format, ...)
{
    if(!flags.ikbdLogs) {               // don't do IKBD logs? quit
        return;
    }

    static DWORD prevLogOutIkbd = 0;

    va_list args;
    va_start(args, format);

    FILE *f;

    f = fopen("/var/log/ikbdlog.txt", "a+t");

    if(!f) {
        printf("%08d: ", Utils::getCurrentMs());
        vprintf(format, args);
        printf("\n");

        va_end(args);
        return;
    }

    DWORD now = Utils::getCurrentMs();
    DWORD diff = now - prevLogOutIkbd;
    prevLogOutIkbd = now;

    fprintf(f, "%08d\t%08d\t ", now, diff);
    vfprintf(f, format, args);
    fprintf(f, "\n");
    fclose(f);

    va_end(args);
}
