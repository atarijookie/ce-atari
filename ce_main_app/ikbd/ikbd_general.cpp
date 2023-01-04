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
#include "utils.h"
#include "settings.h"
#include <stdint.h>
#include "periodicthread.h"
#include "config/configstream.h"
#include "chipinterface.h"

#include "ikbd.h"

TInputDevice ikbdDevs[INTYPE_MAX+1];

extern volatile sig_atomic_t    sigintReceived;
       volatile bool            do_loadIkbdConfig = false;

extern TFlags flags;                                // global flags from command line
extern SharedObjects shared;
extern ChipInterface* chipInterface;

void *ikbdThreadCode(void *ptr)
{
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

    chipInterface->ikbdUartEnable(true);        // enable UART for IKDB via some hardware magic

    inotifyFd = inotify_init();
    if(inotifyFd < 0) {
        logDebugAndIkbd(LOG_ERROR, "inotify_init() failed");
    } else {
        wd1 = inotify_add_watch(inotifyFd, "/dev/input", IN_CREATE);
        if(wd1 < 0) Debug::out(LOG_ERROR, "inotify_add_watch(/dev/input, IN_CREATE) failed");
        wd2 = inotify_add_watch(inotifyFd, "/dev/input/by-path", IN_CREATE | IN_DELETE_SELF);
        if(wd2 < 0) Debug::out(LOG_ERROR, "inotify_add_watch(/dev/input/by-path, IN_CREATE | IN_DELETE_SELF)");

        std::string vdevFolder = Utils::dotEnvValue("IKBD_VIRTUAL_DEVICES_PATH");
        wd3 = inotify_add_watch(inotifyFd, vdevFolder.c_str(), IN_CREATE);
        if(wd3 < 0) Debug::out(LOG_ERROR, "inotify_add_watch('%s', IN_CREATE)", vdevFolder.c_str());
    }

    ikbd.fillDisplayLine();      // fill it for showing it on display

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

        int fdUart = chipInterface->ikbdUartReadFd();       // get FD for reading from IKBD

        if(fdUart >= 0) {                                   // if fdUart is valid (open)
            FD_SET(fdUart, &readfds);
            if(fdUart > max_fd) max_fd = fdUart;
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

        // TODO: fix this later
        //bool clientConnected = ((Utils::getCurrentMs() - shared.configStream.acsi->getLastCmdTimestamp()) <= 2000);
        bool clientConnected = false;

        if(fdUart >= 0 && FD_ISSET(fdUart, &readfds)) {
            // process the incomming data from original keyboard and from ST
            ikbd.processReceivedCommands(clientConnected);
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
                    logDebugAndIkbd(LOG_ERROR, "ikbdThreadCode() read(%d) returned 0 (EOF) closing %d", fd, i);
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

    chipInterface->ikbdUartEnable(false);        // disable UART for IKDB via some hardware magic, so Atari keyboard and mouse will work even if ce_main_app doesn't run

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

    mouseBtnNow = 0;

    // init uart RX cyclic buffers
    cbStCommands.init();
    cbKeyboardData.init();
    cbReceivedData.init();

    fillSpecialCodeLengthTable();
    fillStCommandsLengthTable();

    resetInternalIkbdVars();
}

void Ikbd::loadSettings(void)
{
    Settings s;
    firstJoyIs0 = s.getBool("JOY_FIRST_IS_0", false);

    if(firstJoyIs0) {
        joy1st = INTYPE_JOYSTICK1;
        joy2nd = INTYPE_JOYSTICK2;
    } else {
        joy1st = INTYPE_JOYSTICK2;
        joy2nd = INTYPE_JOYSTICK1;
    }

    // get enabled flags for mouse wheel as keys
    mouseWheelAsArrowsUpDown = s.getBool("MOUSE_WHEEL_AS_KEYS", true);

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

int Ikbd::fdWrite(int fd, uint8_t *bfr, int cnt)
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

std::string ikbdLogFilePath;

void ikbdLog(const char *format, ...)
{
    if(!flags.ikbdLogs) {               // don't do IKBD logs? quit
        return;
    }

    static uint32_t prevLogOutIkbd = 0;

    va_list args;
    va_start(args, format);

    FILE *f;

    if(ikbdLogFilePath.empty()) {   // construct this path once, reuse later
        ikbdLogFilePath = Utils::dotEnvValue("LOG_DIR", "/var/log/ce");     // path to logs dir
        Utils::mergeHostPaths(ikbdLogFilePath, "ikbd.log");                 // full path = logs dir + filename
    }

    f = fopen(ikbdLogFilePath.c_str(), "a+t");

    if(!f) {
        printf("%08d: ", Utils::getCurrentMs());
        vprintf(format, args);
        printf("\n");

        va_end(args);
        return;
    }

    uint32_t now = Utils::getCurrentMs();
    uint32_t diff = now - prevLogOutIkbd;
    prevLogOutIkbd = now;

    fprintf(f, "%08d\t%08d\t ", now, diff);
    vfprintf(f, format, args);
    fprintf(f, "\n");
    fclose(f);

    va_end(args);
}

std::string chipLogFilePath;

// chipLog() will receive incomplete lines stored in cb, terminated by '\n'.
// It should write only complete lines to file, so it will try to gather chars until '\n' char and do write to file then.
void chipLog(uint16_t cnt, CyclicBuff *cb)
{
    static std::string oneLine;
    static uint32_t prevLogOutChips = 0;

    uint32_t now = Utils::getCurrentMs();
    uint32_t diff = now - prevLogOutChips;
    prevLogOutChips = now;

    char val = 0;

    for(int i=0; i<cnt; i++) {  // for cnt of characters
        val = cb->get();        // get from cyclic buffer
        oneLine += val;         // append to string

        if(val == '\n') {       // if last char was new line, dump it to file
            if(chipLogFilePath.empty()) {
                chipLogFilePath = Utils::dotEnvValue("LOG_DIR", "/var/log/ce");     // path to logs dir
                Utils::mergeHostPaths(chipLogFilePath, "chip.log");             // full path = dir + filename
            }

            FILE *f = fopen(chipLogFilePath.c_str(), "a+t");

            if(f != NULL) {     // if file open good, write data
                fprintf(f, "%08d\t%08d\t ", now, diff);
                fputs(oneLine.c_str(), f);
                fclose(f);
            }

            oneLine.clear();    // clear gathered line
        }
    }
}
