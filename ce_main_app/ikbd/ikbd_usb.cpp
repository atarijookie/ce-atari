#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/joystick.h>
#include <errno.h>
#include <unistd.h>

#include "global.h"
#include "debug.h"
#include "utils.h"
#include "settings.h"
#include "datatypes.h"
#include "statusreport.h"

#include "ikbd.h"

extern TInputDevice ikbdDevs[INTYPE_MAX+1];

void Ikbd::initDevs(void)
{
    int i;

    lastVDevMouseEventTime = 0;
    
    for(i=0; i<6; i++) {
        ikbdDevs[i].fd = -1;

        deinitDev(i);
    }
}

void Ikbd::deinitDev(int index)
{
    if(index < 0 || index > 5) {            // out of index?
        return;
    }

    if(ikbdDevs[index].fd != -1) {            // device was open? close it
        close(ikbdDevs[index].fd);
    }

    ikbdDevs[index].fd            = -1;       // set vars to default init values
    ikbdDevs[index].devPath[0]    = 0;
    
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

    for(i=0; i<6; i++) {
        if(ikbdDevs[i].fd != -1) {        // if device is open
            close(ikbdDevs[i].fd);        // close it
            ikbdDevs[i].fd = -1;          // mark it as closed
        }
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

void Ikbd::findVirtualDevices(void)
{
    char linkBuf[PATH_BUFF_SIZE];
    char devBuf[PATH_BUFF_SIZE];

    DIR *dir = opendir("/tmp/vdev/");							// try to open the dir
	
    if(dir == NULL) {                                 				// not found?
        return;
    }

	while(1) {                                                  	// while there are more files, store them
		struct dirent *de = readdir(dir);							// read the next directory entry
	
		if(de == NULL) {											// no more entries?
			break;
		}

    if(de->d_type != DT_FIFO) {									// if it's not a fifo, skip it
			continue;
		}

        char *pMouse  = strstr(de->d_name, "mouse");            // does the name contain 'mouse'?
        char *pKbd    = strstr(de->d_name, "kbd");              // does the name contain 'kbd'?

        if(pMouse == NULL && pKbd == NULL) {
            continue;
        }

        memset(linkBuf,	0, PATH_BUFF_SIZE);
        memset(devBuf,	0, PATH_BUFF_SIZE);

        strcpy(linkBuf, "/tmp/vdev");                          // create path to link files, e.g. /dev/input/by-path/usb-kbd-event
        strcat(linkBuf, HOSTPATH_SEPAR_STRING);
        strcat(linkBuf, de->d_name);

        /*
        int ires = readlink(linkBuf, devBuf, PATH_BUFF_SIZE);		// try to resolve the filename from the link
        if(ires == -1) {
                continue;
        }

        std::string pathAndFile = devBuf;
        std::string path, file;
        */

        std::string pathAndFile = linkBuf;
        std::string path, file;
        Utils::splitFilenameFromPath(pathAndFile, path, file);		// get only file name, skip the path (which now is something like '../../')
        //file = "/dev/input/" + file;    							// create full path - /dev/input/event0
        file = "/tmp/vdev/"+file;    							// create full path - /dev/input/event0

        processFoundDev((char *) file.c_str(), (char *) file.c_str());    // and do something with that file
    }

    closedir(dir);	
}

void Ikbd::processFoundDev(char *linkName, char *fullPath)
{
    TInputDevice *in = NULL;
    char *what;

    if(strstr(linkName, "/tmp/vdev/mouse") != NULL && in==NULL) {             // it's a mouse
        if(ikbdDevs[INTYPE_VDEVMOUSE].fd == -1) {             // don't have mouse?
            in = &ikbdDevs[INTYPE_VDEVMOUSE];
            what = (char *) "/tmp/vdev/mouse";
        } else {                                        // already have a mouse?
            return;
        }
    }

    if(strstr(linkName, "/tmp/vdev/kbd") != NULL && in==NULL) {               // it's a keyboard?
        if(ikbdDevs[INTYPE_VDEVKEYBOARD].fd == -1) {          // don't have keyboard?
            in = &ikbdDevs[INTYPE_VDEVKEYBOARD];
            what = (char *) "/tmp/vdev/keyboard";         
        } else {                                        // already have a keyboard?
            return;
        }
    }

    if(strstr(linkName, "mouse") != NULL && in==NULL) {             // it's a mouse
        if(ikbdDevs[INTYPE_MOUSE].fd == -1) {             // don't have mouse?
            in = &ikbdDevs[INTYPE_MOUSE];         
            what = (char *) "mouse";
        } else {                                        // already have a mouse?
            return;
        }
    }

    if(strstr(linkName, "kbd") != NULL && in==NULL) {               // it's a keyboard?
        if(ikbdDevs[INTYPE_KEYBOARD].fd == -1) {          // don't have keyboard?
            in = &ikbdDevs[INTYPE_KEYBOARD];
            what = (char *) "keyboard";         
        } else {                                        // already have a keyboard?
            return;
        }
    }

    if(strstr(linkName, "joystick") != NULL && in==NULL) {                  // it's a joystick?
        if(ikbdDevs[joy1st].fd == -1) {                                       // don't have joystick 1?
            if(strcmp(fullPath, ikbdDevs[joy2nd].devPath) == 0) {             // if this device is already connected as joystick 2, skip it
                return;
            }

            in = &ikbdDevs[joy1st];         
            what = (char *) "joystick1";
        } else if(ikbdDevs[joy2nd].fd == -1) {                                // don't have joystick 2?
            if(strcmp(fullPath, ikbdDevs[joy1st].devPath) == 0) {             // if this device is already connected as joystick 1, skip it
                return;
            }

            in = &ikbdDevs[joy2nd];         
            what = (char *) "joystick2";
        } else {                                                            // already have a joystick?
            return;
        }
    }

    if(in == NULL) {                                                        // this isn't mouse, keyboard of joystick, quit!
        return;
    }

    int fd = open(fullPath, O_RDONLY | O_NONBLOCK);

    if(fd < 0) {
        logDebugAndIkbd(LOG_ERROR, "Failed to open input device (%s): %s", what, fullPath);
        return;
    }

    in->fd = fd;
    strcpy(in->devPath, fullPath);
    logDebugAndIkbd(LOG_DEBUG, "Got device (%s): %s", what, fullPath);
}

void Ikbd::processMouse(input_event *ev)
{
    if(ev->type == EV_KEY) {		// on button press
	    int btnNew = mouseBtnNow;
		
        statuses.ikbdUsb.aliveTime = Utils::getCurrentMs();
        statuses.ikbdUsb.aliveSign = ALIVE_MOUSEVENT;

		BYTE absButtons = 0;
		
	    switch(ev->code) {
		    case BTN_LEFT:		
			if(ev->value == 1) {				// on DOWN - add bit
			    btnNew |= 2;
				absButtons			|= MOUSEABS_BTN_LEFT_DOWN;
				absMouse.buttons	|= MOUSEABS_BTN_LEFT_DOWN;
		    } else {						    // on UP - remove bit
		    	btnNew &= ~2; 
				absButtons			|= MOUSEABS_BTN_LEFT_UP;
				absMouse.buttons	|= MOUSEABS_BTN_LEFT_UP;
			}
			break;
		
    		case BTN_RIGHT:		
			if(ev->value == 1) {				// on DOWN - add bit
				btnNew |= 1; 
				absButtons			|= MOUSEABS_BTN_RIGHT_DOWN;
				absMouse.buttons	|= MOUSEABS_BTN_RIGHT_DOWN;
			} else {						    // on UP - remove bit
				btnNew &= ~1; 
				absButtons			|= MOUSEABS_BTN_RIGHT_UP;
				absMouse.buttons	|= MOUSEABS_BTN_RIGHT_UP;
			}
			break;
		}
			
		if(btnNew == mouseBtnNow) {							    // mouse buttons didn't change? 
			return;
		}

		mouseBtnNow = btnNew;								    // store new button states

        if(mouseAbsBtnAct & MOUSEBTN_REPORT_ACTLIKEKEYS) {      // if should report mouse clicks as keys
            BYTE bfr;
            
            switch(absButtons) {
                case MOUSEABS_BTN_LEFT_DOWN:    bfr = 0x74; fdWrite(fdUart, &bfr, 1); break;
                case MOUSEABS_BTN_LEFT_UP:      bfr = 0xf4; fdWrite(fdUart, &bfr, 1); break;
                case MOUSEABS_BTN_RIGHT_DOWN:   bfr = 0x75; fdWrite(fdUart, &bfr, 1); break;
                case MOUSEABS_BTN_RIGHT_UP:     bfr = 0xf5; fdWrite(fdUart, &bfr, 1); break;
            }
        }
        
        if(mouseMode == MOUSEMODE_ABS) {                        // for absolute mouse mode
			bool wasUp		= (absButtons		& MOUSEABS_BTN_UP)			!= 0;
			bool reportUp	= (mouseAbsBtnAct	& MOUSEBTN_REPORT_RELEASE)  != 0;
			bool wasDown	= (absButtons		& MOUSEABS_BTN_DOWN)		!= 0;
			bool reportDown	= (mouseAbsBtnAct	& MOUSEBTN_REPORT_PRESS)	!= 0;
		
            if(	(wasUp && reportUp) || (wasDown && reportDown) ) {	// if button pressed / released and we should report that
                sendMousePosAbsolute(fdUart, absButtons);
            }            
        } else {                                                // for relative mouse mode
            sendMousePosRelative(fdUart, mouseBtnNow, 0, 0);    // send them to ST
        }
		return;
	}
		
	if(ev->type == EV_REL) {
        statuses.ikbdUsb.aliveTime = Utils::getCurrentMs();
        statuses.ikbdUsb.aliveSign = ALIVE_MOUSEVENT;

		if(ev->code == REL_X) {
            // update absolute position
            absMouse.x += ev->value;
            fixAbsMousePos();

			sendMousePosRelative(fdUart, mouseBtnNow, ev->value, 0);	// send movement to ST
		}

		if(ev->code == REL_Y) {
            // update absolute position
            if(mouseY0atTop) {              // Y=0 at top - add value
                absMouse.y += ev->value;
            } else {                        // Y=0 at bottom - subtract value
                absMouse.y -= ev->value;
            }
            fixAbsMousePos();
                                 
			sendMousePosRelative(fdUart, mouseBtnNow, 0, ev->value);	// send movement to ST
		}
        
        //--------------------------
        // mouse wheel moved and mouse wheel should act as UP / DOWN keys?
        if(ev->code == REL_WHEEL && mouseWheelAsArrowsUpDown) {
            int stKey;
            
            if(ev->value > 0) {
                stKey = keyTranslator.pcKeyToSt(KEY_UP);
            } else {
                stKey = keyTranslator.pcKeyToSt(KEY_DOWN);
            }

            if(stKey == 0 || fdUart == -1) {    // key not found, or UART not open? quit
                return;
            }

            ikbdLog("\nEV_REL -> REL_WHEEL: ev->value: %d, stKey: %d", ev->value, stKey);

            BYTE bfr[2];
            bfr[0] = stKey;                     // key down
            bfr[1] = stKey | 0x80;              // key up
    
            int res = fdWrite(fdUart, bfr, 2); 

            if(res < 0) {
                logDebugAndIkbd(LOG_ERROR, "processMouse - sending to ST failed, errno: %d", errno);
            }
        }        
	}
}

void Ikbd::processKeyboard(input_event *ev)
{
//    ikbdLog("processKeyboard");
    
    if (ev->type == EV_KEY) {
        statuses.ikbdUsb.aliveTime = Utils::getCurrentMs();
        statuses.ikbdUsb.aliveSign = ALIVE_KEYDOWN;

        int stKey = keyTranslator.pcKeyToSt(ev->code);          // translate PC key to ST key

        if(stKey == 0 || fdUart == -1) {                        // key not found, no UART open? quit
            return;
        }
        
        if(keybJoy0 && keyJoyKeys.isKeybJoyKeyPc(0, ev->code)) {    // Keyb joy 0 is enabled, and it's a keyb joy 0 key? Handle it specially. 
            handlePcKeyAsKeybJoy(0, ev->code, ev->value);
            return;
        }

        if(keybJoy1 && keyJoyKeys.isKeybJoyKeyPc(1, ev->code)) {    // Keyb joy 1 is enabled, and it's a keyb joy 1 key? Handle it specially. 
            handlePcKeyAsKeybJoy(1, ev->code, ev->value);
            return;
        }

        // ev->value -- 1: down, 2: auto repeat, 0: up
        if(ev->value == 0) {        // when key is released, ST scan code has the highest bit set
            stKey = stKey | 0x80;
        }

        ikbdLog("\nEV_KEY: code %d, value %d, stKey: %02x", ev->code, ev->value, stKey);

        BYTE bfr;
        bfr = stKey;
    
    	int res = fdWrite(fdUart, &bfr, 1); 

    	if(res < 0) {
	    	logDebugAndIkbd(LOG_ERROR, "processKeyboard - sending to ST failed, errno: %d", errno);
	    }
    }
}

void Ikbd::processJoystick(js_event *jse, int joyNumber)
{
    TJoystickState *js;

    if(joyNumber == 0 || joyNumber == 1) {          // if the index is OK, use it
        js = &joystick[joyNumber];
    } else {                                        // index is not OK, quit
        return;
    }

    if(jse->type == JS_EVENT_AXIS) {
        statuses.ikbdUsb.aliveTime = Utils::getCurrentMs();
        statuses.ikbdUsb.aliveSign = ALIVE_JOYEVENT;

        if(jse->number >= JOYAXIS) {                // if the index of axis would be out of index, quit
            return;
        } 

	    js->axis[ jse->number ] = jse->value;       // store new axis value
    } else if(jse->type == JS_EVENT_BUTTON) {
        statuses.ikbdUsb.aliveTime = Utils::getCurrentMs();
        statuses.ikbdUsb.aliveSign = ALIVE_JOYEVENT;

        if(jse->number >= JOYBUTTONS) {             // if the index of button would be out of index, quit
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
            button = JOYDIR_BUTTON;
        }
    }

    if(fdUart == -1) {                  // nowhere to send data? quit
        return;
    }

    // port 0: mouse + joystick
    // port 1: joystick

    bool dirChanged = (dirTotal != js->lastDir);            // dir changed flag
    bool btnChanged = (button != js->lastBtn);              // btn changed flag
    
    if(dirChanged || btnChanged) {                          // direction or button changed?
        js->lastDir = dirTotal;
        js->lastBtn = button;

        if(joyNumber == 1 && (mouseMode == MOUSEMODE_REL) && btnChanged) {  // if button state changed, send it as mouse packet
            sendJoy0State();
        } else {
            sendJoyState(joyNumber, button | dirTotal);			            // report current direction and buttons
        }
    }
}

int Ikbd::getFdByIndex(int index)
{
    if(index < 0 || index > 5) {
        return -1;
    }

    return ikbdDevs[index].fd;
}

bool Ikbd::gotUsbMouse(void)
{
	if(ikbdDevs[INTYPE_MOUSE].fd != -1 ) {            // got real USB mouse? return true
		return true;
	}
	
    if(ikbdDevs[INTYPE_VDEVMOUSE].fd != -1) {                           // got virtual mouse? 
        DWORD diff = Utils::getCurrentMs() - lastVDevMouseEventTime;    // calculate how much time has passed since last VDevMouse event
        
        if(diff < 10000) {                                              // if virtual mouse moved in the last 10 seconds, we got it (otherwise we don't have it)
            return true;
        }
    }
    
	return false;                                   // no mouse present
}

bool Ikbd::gotUsbJoy1(void)
{
	if(ikbdDevs[INTYPE_JOYSTICK1].fd != -1) {
		return true;
	}
	
	return false;
}

bool Ikbd::gotUsbJoy2(void)
{
	if(ikbdDevs[INTYPE_JOYSTICK2].fd != -1) {
		return true;
	}
	
	return false;
}

void Ikbd::markVirtualMouseEvenTime(void)
{
    // store time when we received VDevMouse event -- for telling that 
    // the virtual mouse is not here when it doesn't move for a longer time
    lastVDevMouseEventTime = Utils::getCurrentMs();         
}

void Ikbd::handlePcKeyAsKeybJoy(int joyNumber, int pcKey, int eventValue)
{
    bool keyDown = (eventValue != 0);   // if eventValue is 0, then it's up; otherwise down
    handleKeyAsKeybJoy(true, joyNumber, pcKey, keyDown);  // handle this key press, and it comes from PC keys (therefore first param: true)
}
    
void Ikbd::handleKeyAsKeybJoy(bool pcNotSt, int joyNumber, int pcKey, bool keyDown)
{
    js_event jse;

    if(keyJoyKeys.isKeyUp(pcNotSt, joyNumber, pcKey)) {
        jse.type    = JS_EVENT_AXIS;        // axis movement
        jse.number  = 1;                    // it's Y axis
        jse.value   = keyDown ? -20000 : 0; // keyboard key down? Joy to direction, otherwise to center (on up)
    } else if(keyJoyKeys.isKeyDown(pcNotSt, joyNumber, pcKey)) {
        jse.type    = JS_EVENT_AXIS;        // axis movement
        jse.number  = 1;                    // it's Y axis
        jse.value   = keyDown ? 20000 : 0;  // keyboard key down? Joy to direction, otherwise to center (on up)
    } else if(keyJoyKeys.isKeyLeft(pcNotSt, joyNumber, pcKey)) {
        jse.type    = JS_EVENT_AXIS;        // axis movement
        jse.number  = 0;                    // it's X axis
        jse.value   = keyDown ? -20000 : 0; // keyboard key down? Joy to direction, otherwise to center (on up)
    } else if(keyJoyKeys.isKeyRight(pcNotSt, joyNumber, pcKey)) {
        jse.type    = JS_EVENT_AXIS;        // axis movement
        jse.number  = 0;                    // it's X axis
        jse.value   = keyDown ? 20000 : 0;  // keyboard key down? Joy to direction, otherwise to center (on up)
    } else if(keyJoyKeys.isKeyButton(pcNotSt, joyNumber, pcKey)) {
        jse.type    = JS_EVENT_BUTTON;      // button press
        jse.number  = 0;                    // it's Y axis
        jse.value   = keyDown ? 1 : 0;      // keyboard key down? 1 means down, 0 means up
    }
    
    processJoystick(&jse, joyNumber);
}
