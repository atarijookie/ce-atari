#ifndef _IKBD_H_
#define _IKBD_H_

#include <termios.h> 
#include <linux/joystick.h>

#define INPUT_LINKS_PATH	    "/dev/input/by-path"
#define HOSTPATH_SEPAR_STRING   "/"
#define PATH_BUFF_SIZE		    1024

#define INTYPE_MOUSE        0
#define INTYPE_KEYBOARD     1
#define INTYPE_JOYSTICK1    2
#define INTYPE_JOYSTICK2    3
#define INTYPE_VDEVMOUSE    4 //virtual device for Web API
#define INTYPE_VDEVKEYBOARD 5 //virtual device for Web API
#define INTYPE_MAX          5 

#define JOYDIR_UP       0x01
#define JOYDIR_DOWN     0x02
#define JOYDIR_LEFT     0x04
#define JOYDIR_RIGHT    0x08
#define JOYDIR_BUTTON   0x80

typedef struct {
    char    devPath[256];
    int     fd;
} TInputDevice;

#define JOYAXIS             8
#define JOYBUTTONS          12

#define KEY_TABLE_SIZE      256

#define UARTFILE	        "/dev/ttyAMA0"

#define CYCLIC_BUF_SIZE     256
#define CYCLIC_BUF_MASK     0xff;

#define UARTMARK_STCMD      0xAA
#define UARTMARK_KEYBDATA   0xBB

typedef struct {
    int axis[JOYAXIS];
    int button[JOYBUTTONS];

    int lastDir;
    int lastBtn;
	
	int lastDirAndBtn;
} TJoystickState;

typedef struct {
    BYTE buf[CYCLIC_BUF_SIZE];
    int  count;
    int  addPos;
    int  getPos;
} TCyclicBuff;

#define CE_IKBDMODE_SOLO		0
#define CE_IKBDMODE_INJECT		1

#define MOUSEMODE_REL           0x08
#define MOUSEMODE_ABS           0x09
#define MOUSEMODE_KEYCODE		0x0a

#define MOUSEBTN_REPORT_NOTHING 		0
#define MOUSEBTN_REPORT_PRESS   		1
#define MOUSEBTN_REPORT_RELEASE			2
#define MOUSEBTN_REPORT_ACTLIKEKEYS		4

#define JOYMODE_EVENT           0x14
#define JOYMODE_INTERROGATION   0x15
#define JOYMODE_KEYCODE			0x19

#define MOUSEABS_BTN_LEFT_UP		8
#define MOUSEABS_BTN_LEFT_DOWN		4
#define MOUSEABS_BTN_RIGHT_UP		2
#define MOUSEABS_BTN_RIGHT_DOWN		1

#define MOUSEABS_BTN_UP				(MOUSEABS_BTN_LEFT_UP	| MOUSEABS_BTN_RIGHT_UP)
#define MOUSEABS_BTN_DOWN			(MOUSEABS_BTN_LEFT_DOWN	| MOUSEABS_BTN_RIGHT_DOWN)

// data sent from keyboard
#define KEYBDATA_SPECIAL_LOWEST		0xf6
#define KEYBDATA_STATUS             0xf6
#define KEYBDATA_MOUSE_ABS          0xf7
#define KEYBDATA_MOUSE_REL8         0xf8
#define KEYBDATA_MOUSE_REL9         0xf9
#define KEYBDATA_MOUSE_RELA         0xfa
#define KEYBDATA_MOUSE_RELB         0xfb
#define KEYBDATA_TIMEOFDAY          0xfc
#define KEYBDATA_JOY_BOTH           0xfd
#define KEYBDATA_JOY0               0xfe
#define KEYBDATA_JOY1               0xff

#define KEYBDATA_STATUS_LEN         8
#define KEYBDATA_MOUSE_ABS_LEN      6
#define KEYBDATA_MOUSE_REL8_LEN     3
#define KEYBDATA_MOUSE_REL9_LEN     3
#define KEYBDATA_MOUSE_RELA_LEN     3
#define KEYBDATA_MOUSE_RELB_LEN     3
#define KEYBDATA_TIMEOFDAY_LEN      7
#define KEYBDATA_JOY_BOTH_LEN       3
#define KEYBDATA_JOY0_LEN           2
#define KEYBDATA_JOY1_LEN           2

// commands sent from ST
#define STCMD_RESET                         0x80
#define STCMD_SET_MOUSE_BTN_ACTION          0x07
#define STCMD_SET_REL_MOUSE_POS_REPORTING   0x08
#define STCMD_SET_ABS_MOUSE_POS_REPORTING   0x09
#define STCMD_SET_MOUSE_KEYCODE_MODE        0x0a
#define STCMD_SET_MOUSE_THRESHOLD           0x0b
#define STCMD_SET_MOUSE_SCALE               0x0c
#define STCMD_INTERROGATE_MOUSE_POS         0x0d
#define STCMD_LOAD_MOUSE_POS                0x0e
#define STCMD_SET_Y_AT_BOTTOM               0x0f
#define STCMD_SET_Y_AT_TOP                  0x10
#define STCMD_RESUME                        0x11
#define STCMD_DISABLE_MOUSE                 0x12
#define STCMD_PAUSE_OUTPUT                  0x13
#define STCMD_SET_JOYSTICK_EVENT_REPORTING  0x14
#define STCMD_SET_JOYSTICK_INTERROG_MODE    0x15
#define STCMD_JOYSTICK_INTERROGATION        0x16
#define STCMD_SET_JOYSTICK_MONITORING       0x17
#define STCMD_SET_FIRE_BUTTON_MONITORING    0x18
#define STCMD_SET_JOYSTICK_KEYCODE_MODE     0x19
#define STCMD_DISABLE_JOYSTICKS             0x1a
#define STCMD_TIMEOFDAY_CLOCK_SET           0x1b
#define STCMD_INTERROGATE_TIMEOFDAT_CLOCK   0x1c
#define STCMD_MEMORY_LOAD                   0x20
#define STCMD_MEMORY_READ                   0x21
#define STCMD_CONTROLLER_EXECUTE            0x22

// inquiry commands - any SET command ORed with 0x80
// valid inquiries: 0x87 - 0x8c, 0x8f, 0x90, 0x92, 0x94, 0x95, 0x99, 0x9a

// length of commands sent from ST
#define STCMD_RESET_LEN                         2
#define STCMD_SET_MOUSE_BTN_ACTION_LEN          2
#define STCMD_SET_REL_MOUSE_POS_REPORTING_LEN   1
#define STCMD_SET_ABS_MOUSE_POS_REPORTING_LEN   5
#define STCMD_SET_MOUSE_KEYCODE_MODE_LEN        3
#define STCMD_SET_MOUSE_THRESHOLD_LEN           3
#define STCMD_SET_MOUSE_SCALE_LEN               3
#define STCMD_INTERROGATE_MOUSE_POS_LEN         1
#define STCMD_LOAD_MOUSE_POS_LEN                6
#define STCMD_SET_Y_AT_BOTTOM_LEN               1
#define STCMD_SET_Y_AT_TOP_LEN                  1
#define STCMD_RESUME_LEN                        1
#define STCMD_DISABLE_MOUSE_LEN                 1
#define STCMD_PAUSE_OUTPUT_LEN                  1
#define STCMD_SET_JOYSTICK_EVENT_REPORTING_LEN  1
#define STCMD_SET_JOYSTICK_INTERROG_MODE_LEN    1
#define STCMD_JOYSTICK_INTERROGATION_LEN        1
#define STCMD_SET_JOYSTICK_MONITORING_LEN       2
#define STCMD_SET_FIRE_BUTTON_MONITORING_LEN    1
#define STCMD_SET_JOYSTICK_KEYCODE_MODE_LEN     7
#define STCMD_DISABLE_JOYSTICKS_LEN             1
#define STCMD_TIMEOFDAY_CLOCK_SET_LEN           7
#define STCMD_INTERROGATE_TIMEOFDAT_CLOCK_LEN   1
#define STCMD_MEMORY_LOAD_LEN                   4           // or more!!!
#define STCMD_MEMORY_READ_LEN                   3
#define STCMD_CONTROLLER_EXECUTE_LEN            3

void *ikbdThreadCode(void *ptr);

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

    void processReceivedCommands(void);

private:
	int				ceIkbdMode;

    TInputDevice    inDevs[INTYPE_MAX+1];
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
	
    int             joystickMode;
    TJoystickState  joystick[2];
    bool            joystickEnabled;

    TCyclicBuff     cbStCommands;
    TCyclicBuff     cbKeyboardData;

    bool            gotHalfPair;
    BYTE            halfPairData;

    BYTE            specialCodeLen[10];

    BYTE            stCommandLen[256];

    void initCyclicBuffer(TCyclicBuff *cb);
    void addToCyclicBuffer(TCyclicBuff *cb, BYTE val);
    BYTE getFromCyclicBuffer(TCyclicBuff *cb);
    BYTE peekCyclicBuffer(TCyclicBuff *cb);
    BYTE peekCyclicBufferWithOffset(TCyclicBuff *cb, int offset);

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
};

#endif

