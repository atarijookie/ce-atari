#ifndef _IKBD_H_
#define _IKBD_H_

#include <termios.h> 
#include <linux/joystick.h>

#include "ikbd_defs.h"
#include "cyclicbuff.h"

//#define SPYIKBD

#define INPUT_LINKS_PATH	    "/dev/input/by-path"
#define HOSTPATH_SEPAR_STRING   "/"
#define PATH_BUFF_SIZE		    1024

typedef struct {
    char    devPath[256];
    int     fd;
} TInputDevice;

#define JOYAXIS             8
#define JOYBUTTONS          12

#define KEY_TABLE_SIZE      256

#define UARTFILE	        "/dev/ttyAMA0"

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

    void findDevices(void);
    void findVirtualDevices();
    void closeDevs(void);
    int  serialSetup(termios *ts);

    int getFdByIndex(int index);
    void deinitDev(int index);

    void processMouse(input_event *ev);
    void processKeyboard(input_event *ev);
    void processJoystick(js_event *jse, int joyNumber);
	void markVirtualMouseEvenTime(void);

    void processReceivedCommands(void);

private:
	int				ceIkbdMode;

    int             tableKeysPcToSt[KEY_TABLE_SIZE];
    int             fdUart;

    bool            outputEnabled;

    int             mouseBtnNow;
    int             mouseMode;
    bool            mouseEnabled;
    bool            mouseY0atTop;
    int             mouseAbsBtnAct;

    int             joy1st;
    int             joy2nd;

	struct {
		int		threshX, threshY;
	} relMouse;
	
    struct {
        int		x,y;
        int		maxX, maxY;
		int		scaleX, scaleY;
		BYTE	buttons;
    } absMouse;

	struct {
		BYTE deltaX;
		BYTE deltaY;
	} keycodeMouse;
	
    DWORD           lastVDevMouseEventTime;
    
    int             joystickMode;
    TJoystickState  joystick[2];
    bool            joystickEnabled;

    CyclicBuff      cbStCommands;
    CyclicBuff      cbKeyboardData;

    bool            gotHalfPair;
    BYTE            halfPairData;

    BYTE            specialCodeLen[10];

    BYTE            stCommandLen[256];

    void initDevs(void);
    void initJoystickState(TJoystickState *joy);
    void fillKeyTranslationTable(void);
    void addToTable(int pcKey, int stKey);
    void fillSpecialCodeLengthTable(void);
    void fillStCommandsLengthTable(void);

    void processFoundDev(char *linkName, char *fullPath);

    void resetInternalIkbdVars(void);
    void sendJoy0State(void);
    void sendJoyState(int joyNumber, int dirTotal);
	void sendBothJoyReport(void);
    void sendMousePosRelative(int fd, BYTE buttons, BYTE xRel, BYTE yRel);
    void sendMousePosAbsolute(int fd, BYTE absButtons);
    void fixAbsMousePos(void);

    void processStCommands(void);
	void processGetCommand(BYTE getCmd);
    void processKeyboardData(void);

	bool gotUsbMouse(void);
	bool gotUsbJoy1(void);
	bool gotUsbJoy2(void);
    
    int fdWrite(int fd, BYTE *bfr, int cnt);
    
    void dumpBuffer(bool fromStNotKeyboard);
};

#endif

