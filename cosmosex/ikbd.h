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

#define JOYDIR_UP       1
#define JOYDIR_DOWN     2
#define JOYDIR_LEFT     4
#define JOYDIR_RIGHT    8

typedef struct {
    char    devPath[256];
    int     fd;
} TInputDevice;

#define JOYAXIS     8
#define JOYBUTTONS  12

typedef struct {
    int axis[JOYAXIS];
    int button[JOYBUTTONS];

    int lastDir;
    int lastBtn;
} TJoystickState;

#define KEY_TABLE_SIZE      256

#define UARTFILE	"/dev/ttyAMA0"

#define UART_RXBUF_SIZE  128

void *ikbdThreadCode(void *ptr);

class Ikbd
{
public:
    Ikbd();

    void findDevices(void);
    void closeDevs(void);
    int  serialSetup(termios *ts);

    int getFdByIndex(int index);
    void deinitDev(int index);

    void processMouse(input_event *ev);
    void processKeyboard(input_event *ev);
    void processJoystick(js_event *jse, int joyNumber);

    void processReceivedCommands(void);

private:
    TInputDevice    inDevs[4];
    int             tableKeysPcToSt[KEY_TABLE_SIZE];
    int             fdUart;

    int             mouseBtnNow;

    TJoystickState  joystick[2];

    struct {
        char buf[UART_RXBUF_SIZE];
        int  count;
        int  addPos;
        int  getPos;
    } uartRx;        
    
    void addToRxBuffer(BYTE val);
    BYTE getFromRxBuffer(void);
    BYTE peekRxBuffer(void);

    void initDevs(void);
    void initJoystickState(TJoystickState *joy);
    void fillKeyTranslationTable(void);
    void addToTable(int pcKey, int stKey);

    void processFoundDev(char *linkName, char *fullPath);

    void sendJoy0State(void);
    void serialSendMousePacket(int fd, BYTE buttons, BYTE xRel, BYTE yRel);
};

#endif

