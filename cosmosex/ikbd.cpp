#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string>

#include <signal.h>
#include <termios.h> 
#include <linux/input.h>
#include <linux/joystick.h>
#include <errno.h>

#include "gpio.h"
#include "utils.h"
#include "ikbd.h"

extern volatile sig_atomic_t sigintReceived;

void *ikbdThreadCode(void *ptr)
{
    struct termios	termiosStruct;
    Ikbd ikbd;
    DWORD nextDevFindTime;

    bcm2835_gpio_write(PIN_TX_SEL1N2, HIGH);		// TX_SEL1N2, switch the RX line to receive from Franz, which does the 9600 to 7812 baud translation

    nextDevFindTime = Utils::getEndTime(3000);      // check the devices in 3 seconds
    ikbd.findDevices();

	// open and set up uart
	int res = ikbd.serialSetup(&termiosStruct);
	if(res == -1) {
        printf("ikbd.serialSetup failed, won't be able to send IKDB data\n");
	}

    while(sigintReceived == 0) {
        if(Utils::getCurrentMs() >= nextDevFindTime) {      // should we check for new devices?
            nextDevFindTime = Utils::getEndTime(3000);      // check the devices in 3 seconds

            ikbd.findDevices();
        }

        struct input_event  ev;
        struct js_event     js;

        ssize_t res;
        int i;
        bool gotSomething = false;

        for(i=0; i<4; i++) {                                        // go through the input devices
            if(ikbd.getFdByIndex(i) == -1) {                        // device not attached? skip the rest
                continue;
            }

            if(i < 2) {     // for keyboard and mouse
                res = read(ikbd.getFdByIndex(i), &ev, sizeof(input_event)); 
            } else {        // for joysticks
                res = read(ikbd.getFdByIndex(i), &js, sizeof(js_event)); 
            }

            if(res < 0) {                                           // on error, skip the rest
                if(errno == ENODEV) {                               // if device was removed, deinit it
                    ikbd.deinitDev(i);
                }

                continue;
            }

            gotSomething = true;                                    // mark that reading of at least one device succeeded

            switch(i) {
                case INTYPE_MOUSE:      ikbd.processMouse(&ev);          break;
                case INTYPE_KEYBOARD:   ikbd.processKeyboard(&ev);       break;
                case INTYPE_JOYSTICK1:  ikbd.processJoystick(&js, 0);    break;
                case INTYPE_JOYSTICK2:  ikbd.processJoystick(&js, 1);    break;
            }
        }

        if(!gotSomething) {                                         // didn't succeed with reading of any device? 
            usleep(10000);                                          // sleep for 10 us
            continue;
        }
    }
    
    ikbd.closeDevs();
    return 0;
}

Ikbd::Ikbd()
{
    initDevs();
    fillKeyTranslationTable();

    fdUart      = -1;
    mouseBtnNow = 0;
}

void Ikbd::processMouse(input_event *ev)
{
    if(ev->type == EV_KEY) {		// on button press
	    int btnNew = mouseBtnNow;
		
	    switch(ev->code) {
		    case BTN_LEFT:		
		    printf("mouse LEFT button %d", ev->value); 
			
			if(ev->value == 1) {				// on DOWN - add bit
			    btnNew |= 2; 
		    } else {						// on UP - remove bit
		    	btnNew &= ~2; 
			}
			
			break;
		
    		case BTN_RIGHT:		
			printf("mouse RIGHT button %d", ev->value); 
					
			if(ev->value == 1) {				// on DOWN - add bit
				btnNew |= 1; 
			} else {						// on UP - remove bit
				btnNew &= ~1; 
			}
					
			break;
		}
			
		if(btnNew == mouseBtnNow) {							// mouse buttons didn't change? 
			return;
		}
			
		mouseBtnNow = btnNew;								// store new button states
		serialSendMousePacket(fdUart, mouseBtnNow, 0, 0);	// send them to ST
		return;
	}
		
	if(ev->type == EV_REL) {
		if(ev->code == REL_X) {
			printf("mouse REL_X: %d\n", ev->value);
			serialSendMousePacket(fdUart, mouseBtnNow, ev->value, 0);	// send movement to ST
		}

		if(ev->code == REL_Y) {
			printf("mouse REL_Y: %d\n", ev->value);
			serialSendMousePacket(fdUart, mouseBtnNow, 0, ev->value);	// send movement to ST
		}
	}
}

void Ikbd::processKeyboard(input_event *ev)
{
    if (ev->type == EV_KEY) {
        if(ev->code >= KEY_TABLE_SIZE) {        // key out of index? quit
            return;
        }

        int stKey = tableKeysPcToSt[ev->code];  // translate PC key to ST key

        if(stKey == 0) {                        // key not found? quit
            return;
        }

        // ev->value -- 1: down, 2: auto repeat, 0: up
        if(ev->value == 0) {        // when key is released, ST scan code has the highest bit set
            stKey = stKey | 0x80;
        }

        printf("\nEV_KEY: code %d, value %d, stKey: %02x\n", ev->code, ev->value, stKey);

        if(fdUart == -1) {                          // no UART open? quit
            return;
        }

        BYTE bfr;
        bfr = stKey;
    
    	int res = write(fdUart, &bfr, 1); 

    	if(res < 0) {
	    	printf("processKeyboard - sending to ST failed, errno: %d\n", errno);
	    }
    }
}

void Ikbd::processJoystick(js_event *jse, int joyNumber)
{
    TJoystickState *js;

    if(joyNumber == 0 || joyNumber == 1) {      // if the index is OK, use it
        js = &joystick[joyNumber];
    } else {                                    // index is not OK, quit
        return;
    }

    if(jse->type == JS_EVENT_AXIS) {
        if(jse->number >= JOYAXIS) {            // if the index of axis would be out of index, quit
            return;
        } 

	    js->axis[ jse->number ] = jse->value;   // store new axis value
    } else if(jse->type == JS_EVENT_BUTTON) {
        if(jse->number >= JOYBUTTONS) {         // if the index of button would be out of index, quit
            return;
        } 

	    js->button[ jse->number ] = jse->value;     // 1 means down, 0 means up
    } else {
	    return;
    }

    int dirTotal, dirX, dirY;

    if(js->axis[0] < -16000) {          // joy left?
        dirX    = JOYDIR_LEFT;
    } else if(js->axis[0] > 16000) {    // joy right?
        dirX    = JOYDIR_RIGHT;
    } else {                            // joy in center
        dirX    = 0;
    }

    if(js->axis[1] < -16000) {          // joy up?
        dirY    = JOYDIR_UP;
    } else if(js->axis[1] > 16000) {    // joy down?
        dirY    = JOYDIR_DOWN;
    } else {                            // joy in center
        dirY    = 0;
    }

    dirTotal = dirX | dirY;             // merge left-right with up-down

    int button = 0;
    for(int i=0; i<JOYBUTTONS; i++) {   // check if any button is pressed
        if(js->button[i] != 0) {        // this button pressed? mark that something is pressed
            button = 1;
        }
    }

    if(fdUart == -1) {                  // nowhere to send data? quit
        return;
    }

    // port 0: mouse + joystick
    // port 1: joystick

    BYTE bfr[3];
    int res;

    if(joyNumber == 0) {                // for joystick in port 0 (in mouse port)
        if((dirTotal != js->lastDir) || (button != js->lastBtn)) {  // direction or button changed?
            js->lastDir = dirTotal;
            js->lastBtn = button;

            sendJoy0State();            // report current direction and buttons for joy0
        }
    }

    if(joyNumber == 1) {                // for joystick in port 1 (non-mouse port)
        if(dirTotal != js->lastDir) {   // direction changed?
            js->lastDir = dirTotal;

            bfr[0] = 0xff;
            bfr[1] = dirTotal;

            res = write(fdUart, bfr, 2); 

        	if(res < 0) {
        		printf("write to uart (0) failed, errno: %d\n", errno);
            }
        }

        if(button != js->lastBtn) {     // button state changed?
            js->lastBtn = button;

            sendJoy0State();            // report current direction and buttons for joy0, because it contains joy1 button state
        }
    }
}

void Ikbd::sendJoy0State(void)
{
    int bothButtons = 0xf8;     // neutral position
    if(joystick[0].lastBtn) {
        bothButtons |= 0x02;    // left mouse  button, joy0 button
    }
    if(joystick[1].lastBtn) {
        bothButtons |= 0x01;    // right mouse button, joy1 button
    }

    int dirX0, dirY0;

    // convert X direction to ST format for joy0
    if(joystick[0].lastDir & JOYDIR_LEFT) {
        dirX0   = 0xff;
    } else if(joystick[0].lastDir & JOYDIR_RIGHT) {
        dirX0   = 0x01;
    } else {
        dirX0   = 0x00;
    }

    // convert Y direction to ST format for joy0
    if(joystick[0].lastDir & JOYDIR_UP) {
        dirY0   = 0xff;
    } else if(joystick[0].lastDir & JOYDIR_DOWN) {
        dirY0   = 0x01;
    } else {
        dirY0   = 0x00;
    }

    char bfr[3];
    bfr[0] = bothButtons;
    bfr[1] = dirY0;
    bfr[2] = dirX0;

    int res = write(fdUart, bfr, 3); 

    if(res < 0) {
        printf("write to uart (1) failed, errno: %d\n", errno);
    }
}

void Ikbd::findDevices(void)
{
	char linkBuf[PATH_BUFF_SIZE];
	char devBuf[PATH_BUFF_SIZE];
	
	DIR *dir = opendir(INPUT_LINKS_PATH);							// try to open the dir
	
    if(dir == NULL) {                                 				// not found?
        return;
    }

	while(1) {                                                  	// while there are more files, store them
		struct dirent *de = readdir(dir);							// read the next directory entry
	
		if(de == NULL) {											// no more entries?
			break;
		}

		if(de->d_type != DT_LNK) {									// if it's not a link, skip it
			continue;
		}

        char *pJoystick = strstr(de->d_name, "joystick");           // does the name contain 'joystick'?
        char *pEvent    = strstr(de->d_name, "event");              // does the name contain 'event'?

        if(pJoystick == NULL && pEvent == NULL) {                   // if it's not joystick and doesn't contain 'event', skip it (non-joystick must have event)
            continue;
        }

        if(pJoystick != NULL && pEvent != NULL) {                   // if it's joystick and contains 'event', skip it (joystick must NOT have event)
            continue;
        }

		memset(linkBuf,	0, PATH_BUFF_SIZE);
		memset(devBuf,	0, PATH_BUFF_SIZE);
		
		strcpy(linkBuf, INPUT_LINKS_PATH);                          // create path to link files, e.g. /dev/input/by-path/usb-kbd-event
		strcat(linkBuf, HOSTPATH_SEPAR_STRING);
		strcat(linkBuf, de->d_name);

		int ires = readlink(linkBuf, devBuf, PATH_BUFF_SIZE);		// try to resolve the filename from the link
		if(ires == -1) {
			continue;
		}
		
		std::string pathAndFile = devBuf;
		std::string path, file;
		
		Utils::splitFilenameFromPath(pathAndFile, path, file);		// get only file name, skip the path (which now is something like '../../')
		file = "/dev/input/" + file;    							// create full path - /dev/input/event0
		
		processFoundDev((char *) de->d_name, (char *) file.c_str());    // and do something with that file
    }

	closedir(dir);	
}

void Ikbd::processFoundDev(char *linkName, char *fullPath)
{
    TInputDevice *in = NULL;
    char *what;

    if(strstr(linkName, "mouse") != NULL) {             // it's a mouse
        if(inDevs[INTYPE_MOUSE].fd == -1) {             // don't have mouse?
            in = &inDevs[INTYPE_MOUSE];         
            what = (char *) "mouse";
        } else {                                        // already have a mouse?
            return;
        }
    }

    if(strstr(linkName, "kbd") != NULL) {               // it's a keyboard?
        if(inDevs[INTYPE_KEYBOARD].fd == -1) {          // don't have keyboard?
            in = &inDevs[INTYPE_KEYBOARD];
            what = (char *) "keyboard";         
        } else {                                        // already have a keyboard?
            return;
        }
    }

    if(strstr(linkName, "joystick") != NULL) {                              // it's a joystick?
        if(inDevs[INTYPE_JOYSTICK1].fd == -1) {                             // don't have joystick 1?
            if(strcmp(fullPath, inDevs[INTYPE_JOYSTICK2].devPath) == 0) {   // if this device is already connected as joystick 2, skip it
                return;
            }

            in = &inDevs[INTYPE_JOYSTICK1];         
            what = (char *) "joystick1";
        } else if(inDevs[INTYPE_JOYSTICK2].fd == -1) {                      // don't have joystick 2?
            if(strcmp(fullPath, inDevs[INTYPE_JOYSTICK1].devPath) == 0) {   // if this device is already connected as joystick 1, skip it
                return;
            }

            in = &inDevs[INTYPE_JOYSTICK2];         
            what = (char *) "joystick2";
        } else {                                        // already have a joystick?
            return;
        }
    }

    if(in == NULL) {                                    // this isn't mouse, keyboard of joystick, quit!
        return;
    }

    int fd = open(fullPath, O_RDONLY | O_NONBLOCK);

    if(fd < 0) {
        printf("Failed to open input device (%s): %s\n", what, fullPath);
        return;
    }

    in->fd = fd;
    strcpy(in->devPath, fullPath);
    printf("Got device (%s): %s\n", what, fullPath);
}

int Ikbd::getFdByIndex(int index)
{
    if(index < 0 || index > 3) {
        return -1;
    }

    return inDevs[index].fd;
}

void Ikbd::initDevs(void)
{
    int i;

    for(i=0; i<4; i++) {
        inDevs[i].fd = -1;

        deinitDev(i);
    }
}

void Ikbd::deinitDev(int index)
{
    if(index < 0 || index > 3) {            // out of index?
        return;
    }

    if(inDevs[index].fd != -1) {            // device was open? close it
        close(inDevs[index].fd);
    }

    inDevs[index].fd            = -1;       // set vars to default init values
    inDevs[index].devPath[0]    = 0;
    
    if(index == INTYPE_JOYSTICK1) {         // for joy1 - init it
        initJoystickState(&joystick[0]);
    }

    if(index == INTYPE_JOYSTICK2) {         // for joy2 - init it
        initJoystickState(&joystick[1]);
    }
}

void Ikbd::initJoystickState(TJoystickState *joy)
{
    for(int i=0; i<JOYAXIS; i++) {
        joy->axis[i] = 0;
    }

    for(int i=0; i<JOYBUTTONS; i++) {
        joy->button[i] = 0;
    }

    joy->lastDir = 0;
    joy->lastBtn = 0;
}

void Ikbd::closeDevs(void)
{
    int i;

    for(i=0; i<4; i++) {
        if(inDevs[i].fd != -1) {        // if device is open
            close(inDevs[i].fd);        // close it
            inDevs[i].fd = -1;          // mark it as closed
        }
    }
}

void Ikbd::fillKeyTranslationTable(void)
{
    for(int i=0; i<KEY_TABLE_SIZE; i++) {
        tableKeysPcToSt[i] = 0;
    }

    addToTable(KEY_ESC,         0x01);
    addToTable(KEY_1,           0x02);
    addToTable(KEY_2,           0x03);
    addToTable(KEY_3,           0x04);
    addToTable(KEY_4,           0x05);
    addToTable(KEY_5,           0x06);
    addToTable(KEY_6,           0x07);
    addToTable(KEY_7,           0x08);
    addToTable(KEY_8,           0x09);
    addToTable(KEY_9,           0x0a);
    addToTable(KEY_0,           0x0b);
    addToTable(KEY_MINUS,       0x0c);
    addToTable(KEY_EQUAL,       0x0d);
    addToTable(KEY_BACKSPACE,   0x0e);
    addToTable(KEY_TAB,         0x0f);
    addToTable(KEY_Q,           0x10);
    addToTable(KEY_W,           0x11);
    addToTable(KEY_E,           0x12);
    addToTable(KEY_R,           0x13);
    addToTable(KEY_T,           0x14);
    addToTable(KEY_Y,           0x15);
    addToTable(KEY_U,           0x16);
    addToTable(KEY_I,           0x17);
    addToTable(KEY_O,           0x18);
    addToTable(KEY_P,           0x19);
    addToTable(KEY_LEFTBRACE,   0x1a);
    addToTable(KEY_RIGHTBRACE,  0x1b);
    addToTable(KEY_ENTER,       0x1c);
    addToTable(KEY_LEFTCTRL,    0x1d);
    addToTable(KEY_A,           0x1e);
    addToTable(KEY_S,           0x1f);
    addToTable(KEY_D,           0x20);
    addToTable(KEY_F,           0x21);
    addToTable(KEY_G,           0x22);
    addToTable(KEY_H,           0x23);
    addToTable(KEY_J,           0x24);
    addToTable(KEY_K,           0x25);
    addToTable(KEY_L,           0x26);
    addToTable(KEY_SEMICOLON,   0x27);
    addToTable(KEY_APOSTROPHE,  0x28);
    addToTable(KEY_GRAVE,       0x2b);
    addToTable(KEY_LEFTSHIFT,   0x2a);
    addToTable(KEY_BACKSLASH,   0x60);
    addToTable(KEY_Z,           0x2c);
    addToTable(KEY_X,           0x2d);
    addToTable(KEY_C,           0x2e);
    addToTable(KEY_V,           0x2f);
    addToTable(KEY_B,           0x30);
    addToTable(KEY_N,           0x31);
    addToTable(KEY_M,           0x32);
    addToTable(KEY_COMMA,       0x33);
    addToTable(KEY_DOT,         0x34);
    addToTable(KEY_SLASH,       0x35);
    addToTable(KEY_RIGHTSHIFT,  0x36);
    addToTable(KEY_KPASTERISK,  0x66);
    addToTable(KEY_LEFTALT,     0x38);
    addToTable(KEY_SPACE,       0x39);
    addToTable(KEY_CAPSLOCK,    0x3a);
    addToTable(KEY_F1,          0x3b);
    addToTable(KEY_F2,          0x3c);
    addToTable(KEY_F3,          0x3d);
    addToTable(KEY_F4,          0x3e);
    addToTable(KEY_F5,          0x3f);
    addToTable(KEY_F6,          0x40);
    addToTable(KEY_F7,          0x41);
    addToTable(KEY_F8,          0x42);
    addToTable(KEY_F9,          0x43);
    addToTable(KEY_F10,         0x44);
    addToTable(KEY_KP7,         0x67);
    addToTable(KEY_KP8,         0x68);
    addToTable(KEY_KP9,         0x69);
    addToTable(KEY_KPMINUS,     0x4a);
    addToTable(KEY_KP4,         0x6a);
    addToTable(KEY_KP5,         0x6b);
    addToTable(KEY_KP6,         0x6c);
    addToTable(KEY_KPPLUS,      0x4e);
    addToTable(KEY_KP1,         0x6d);
    addToTable(KEY_KP2,         0x6e);
    addToTable(KEY_KP3,         0x6f);
    addToTable(KEY_KP0,         0x70);
    addToTable(KEY_KPDOT,       0x71);
    addToTable(KEY_KPENTER,     0x72);
    addToTable(KEY_RIGHTCTRL,   0x1d);
    addToTable(KEY_KPSLASH,     0x65);
    addToTable(KEY_RIGHTALT,    0x38);
    addToTable(KEY_UP,          0x48);
    addToTable(KEY_LEFT,        0x4b);
    addToTable(KEY_RIGHT,       0x4d);
    addToTable(KEY_DOWN,        0x50);
    addToTable(KEY_HOME,        0x62);
    addToTable(KEY_PAGEUP,      0x61);
    addToTable(KEY_PAGEDOWN,    0x47);
    addToTable(KEY_INSERT,      0x52);
    addToTable(KEY_DELETE,      0x53);
}

void Ikbd::addToTable(int pcKey, int stKey)
{
    if(pcKey >= KEY_TABLE_SIZE) {
        printf("addToTable -- Can't add pair %d - %d - out of range.\n", pcKey, stKey);
        return;
    }

    tableKeysPcToSt[pcKey] = stKey;
}

void Ikbd::serialSendMousePacket(int fd, BYTE buttons, BYTE xRel, BYTE yRel)
{
    if(fd == -1) {              // no UART open? quit
        return;
    }

	BYTE bfr[3];
	
	bfr[0] = 0xf8 | buttons;
	bfr[1] = xRel;
	bfr[2] = yRel;
	
	int res = write(fd, bfr, 3); 

	if(res < 0) {
		printf("serialSendMousePacket failed, errno: %d\n", errno);
	}
}

int Ikbd::serialSetup(termios *ts) 
{
	int fd;
	
    fdUart = -1;

	fd = open(UARTFILE, O_RDWR | O_NOCTTY | O_NDELAY);
	if(fd == -1) {
        printf("Failed to open %s\n", UARTFILE);
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
	cfsetispeed(ts, B9600);
	cfsetospeed(ts, B9600);
	ts->c_cflag |=  CS8 | CLOCAL | CREAD;			// uart: 8N1

	ts->c_cc[VMIN ] = 0;
	ts->c_cc[VTIME] = 30;

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

    fdUart = fd;
	return fd;
}

