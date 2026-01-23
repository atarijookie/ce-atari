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

extern TInputDevice ikbdDevs[INTYPE_MAX+1];

Ikbd::Ikbd(ChipInterface* ciIkbd)
{
    this->ciIkbd = ciIkbd;

    memset(mac, 0, 6);
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
    Settings s(mac);
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
    keybJoy0 = s.getBool("KEYBOARD_JOY0", false);
    keybJoy1 = s.getBool("KEYBOARD_JOY1", false);

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

void Ikbd::ikbdUartWriteToAll(uint8_t* bfr, int len)
{
    for(int i=0; i<MAX_CLIENTS; i++) {
        ClientInfo* ci = ciIkbd->clientGetByIndex(i);

        if(ci->fdClient == FD_EMPTY) {           // no client here? skip it
            continue;
        }

        write(ci->fdClient, bfr, len);    // send it
    }
}
