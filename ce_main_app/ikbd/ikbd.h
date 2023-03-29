// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#ifndef _IKBD_H_
#define _IKBD_H_

#include <termios.h>
#include <linux/joystick.h>

#include <bitset>

#include "ikbd_defs.h"
#include "cyclicbuff.h"
#include "keybjoys.h"
#include "keytranslator.h"

//#define SPYIKBD

#define INPUT_LINKS_PATH        "/dev/input/by-path"
#define HOSTPATH_SEPAR_STRING   "/"
#define PATH_BUFF_SIZE            1024

typedef struct {
    char    devPath[256];
    int     fd;
} TInputDevice;

#define JOYAXIS             8
#define JOYBUTTONS          12

// count of key to check for key combination
#define KBD_KEY_COUNT   256

#define UARTMARK_STCMD      0xAA
#define UARTMARK_KEYBDATA   0xBB

typedef struct {
    int axis[JOYAXIS];
    int button[JOYBUTTONS];

    int lastDir;
    int lastBtn;

    int lastDirAndBtn;
} TJoystickState;

void *ikbdThreadCode(void *ptr);

void ikbdLog(const char *format, ...);

#define logDebugAndIkbd(LOGLEVEL, FORMAT...)  {                                 \
                                                Debug::out(LOGLEVEL, FORMAT);   \
                                                ikbdLog(FORMAT);                \
                                              }

extern TInputDevice ikbdDevs[INTYPE_MAX+1];

class Ikbd
{
public:
    Ikbd();

    void loadSettings(void);
    void fillDisplayLine(void);

    void findDevices(void);
    void findVirtualDevices();
    void closeDevs(void);
    int  serialSetup(termios *ts);

    int getFdByIndex(int index);
    void deinitDev(int index);

    void processMouse(input_event *ev);
    void processKeyboard(input_event *ev, bool skipKeyboardTranslation);
    void processJoystick(js_event *jse, int joyNumber);
    void markVirtualMouseEvenTime(void);

    void processReceivedCommands(bool skipKeyboardTranslation);

private:
    enum JoystickState {
        Disabled,
        EnabledInMouseMode, // joy1 direction works but its button is mapped as mouse; joy0 is treated as mouse
        Enabled // joy1 and joy0 are treated as joysticks
    };

    int     ceIkbdMode;

    bool    outputEnabled;

    int     mouseBtnNow;
    int     mouseMode;
    bool    mouseEnabled;
    bool    mouseY0atTop;
    int     mouseAbsBtnAct;

    int     joy1st;
    int     joy2nd;

    bool    firstJoyIs0;                    // if true, joystick keyboard mapping and usb/atari joysticks are swapped
    bool    mouseWheelAsArrowsUpDown;       // if true, mouse wheel up / down will be translated to arrow up / down
    bool    keybJoy0;                       // if true, specific keys will act as joy 0
    bool    keybJoy1;                       // if true, specific keys will act as joy 1

    // this is basically all keys which are used for the hotswap, perhaps
    // it should be made a bit more flexible, for different key combinations
    int     leftShiftsPressed;  // how many LEFT SHIFTs are pressed (1x for PC, 1x for Atari)
    int     rightShiftsPressed; // how many RIGHT SHIFTs are pressed (1x for PC, 1x for Atari)
    int     ctrlsPressed;       // how many C[ON]TR[O]Ls are pressed (2x for PC, 1x for Atari)
    int     f11sPressed;        // HELP on Atari, F11 on PC
    int     f12sPressed;        // UNDO on Atari, F12 on PC
    bool    waitingForHotkeyRelease;

    KeybJoyKeys     keyJoyKeys;
    KeyTranslator   keyTranslator;

    struct {
        int        threshX, threshY;
    } relMouse;

    struct {
        int        x,y;
        int        maxX, maxY;
        int        scaleX, scaleY;
        uint8_t    buttons;
    } absMouse;

    struct {
        uint8_t deltaX;
        uint8_t deltaY;
    } keycodeMouse;

    uint32_t           lastVDevMouseEventTime;

    int             joystickMode;
    TJoystickState  joystick[2];
    JoystickState   joystickState;

    CyclicBuff cbReceivedData;
    CyclicBuff cbStCommands;
    CyclicBuff cbKeyboardData;

    uint8_t            specialCodeLen[10];
    uint8_t            stCommandLen[256];

    std::bitset<KBD_KEY_COUNT> pressedKeys;
    bool            keyboardExclusiveAccess;

    void initDevs(void);
    void initJoystickState(TJoystickState *joy);
    void fillKeyTranslationTable(void);
    void addToTable(int pcKey, int stKey, int humanKey=0);
    void fillSpecialCodeLengthTable(void);
    void fillStCommandsLengthTable(void);

    void processFoundDev(const char *linkName, const char *fullPath);

    void resetInternalIkbdVars(void);
    void sendJoyButtonsInMouseMode(void);
    void sendJoyState(int joyNumber, int dirTotal);
    void sendBothJoyReport(void);
    void sendMousePosRelative(uint8_t buttons, uint8_t xRel, uint8_t yRel);
    void sendMousePosAbsolute(uint8_t absButtons);
    void fixAbsMousePos(void);

    void processStCommands(void);
    void processGetCommand(uint8_t getCmd);
    void processKeyboardData(bool skipKeyboardTranslation);

    bool gotUsbMouse(void);
    bool gotUsbKeyboard(void);
    bool gotUsbJoy1(void);
    bool gotUsbJoy2(void);

    void handlePcKeyAsKeybJoy(int joyNumber, int pcKey, int eventValue);
    bool handleStKeyAsKeybJoy(uint8_t val);
    void handleKeyAsKeybJoy  (bool pcNotSt, int joyNumber, int pcKey, bool keyDown);
    bool handleHotkeys(int pcKey, bool pressed, bool skipKeyboardTranslation);

    int fdWrite(int fd, uint8_t *bfr, int cnt);

    void toggleKeyboardExclusiveAccess(void);
    void grabExclusiveAccess(int fd);
    void releaseExclusiveAccess(int fd);

    void dumpBuffer(bool fromStNotKeyboard);
};

#endif

