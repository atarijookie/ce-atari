#include <stdio.h>
#include <stdlib.h>
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

#include "debug.h"
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
        Debug::out("ikbd.serialSetup failed, won't be able to send IKDB data");
	}

    while(sigintReceived == 0) {
        // look for new input devices
        if(Utils::getCurrentMs() >= nextDevFindTime) {      // should we check for new devices?
            nextDevFindTime = Utils::getEndTime(3000);      // check the devices in 3 seconds

            ikbd.findDevices();
        }

        // process the incomming data from original keyboard and from ST
        ikbd.processReceivedCommands();

        // process events from attached input devices
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
    swapJoys    = false;

    // init uart RX cyclic buffers
    initCyclicBuffer(&cbStCommands);
    initCyclicBuffer(&cbKeyboardData);

    gotHalfPair     = false;
    halfPairData    = 0;

    fillSpecialCodeLengthTable();
    fillStCommandsLengthTable();

    resetInternalIkbdVars();
}

void Ikbd::processReceivedCommands(void)
{
    if(fdUart == -1) {                          // uart not open? quit
        return;
    }

    //-----------------
    // receive if there is something to receive
    BYTE bfr[128];
    BYTE *pBfr = gotHalfPair ? &bfr[1] : &bfr[0];               // if got half pair, receive to offset +1

    int res = read(fdUart, pBfr, 127);                          // try to read data from uart

    if(res > 0) {                                               // if some data arrived, add it
        if(gotHalfPair) {                                       // if got half pair from previous read, use it and unmark that we got it
            bfr[0]      = halfPairData;
            gotHalfPair = false;
            res++;                                              // now we got one byte more!
        }

        // divide the received data to ST commands and keyboard data
        for(int i=0; i<res; i += 2) {
            if((i+1) < res) {                                   // there's at least this pair in buffer
                if(bfr[i] == UARTMARK_STCMD) {                  // this is ST command byte, add it to ST command buffer
                    addToCyclicBuffer(&cbStCommands, bfr[i + 1]);
                } else if(bfr[i] == UARTMARK_KEYBDATA) {        // this is original keyboard data, add it to keyboard data buffer
                    addToCyclicBuffer(&cbKeyboardData, bfr[i + 1]);
                } else {                                        // this is not command MARK and not keyboard MARK, move half pair back (we're not in sync with pairs somehow)
                    i--;
                }
            } else {                                            // there's only half of this pair in buffer (this is the last iteration)
                gotHalfPair     = true;                         // mark that we got half pair and store the data
                halfPairData    = bfr[i];
            }
        }
    }

    if(res <= 0) {                                              // no data was received, nothing to resend
        return;
    }

    //-----------------
    // send if there are enough data to be sent
    
    if(cbStCommands.count > 0) {                                // got ST commands? process them
        processStCommands();
    }

    if(cbKeyboardData.count > 0) {                              // got keyboard data? process them
        processKeyboardData();
    }
}

void Ikbd::processStCommands(void)
{
    BYTE bfr[512];

    if(cbStCommands.count <= 0) {                               // no data? quit
        return;
    }

    while(cbStCommands.count > 0) {                             // while there are some data, process
        BYTE cmd = peekCyclicBuffer(&cbStCommands);             // get the data, but don't move the get pointer, because we might fail later
        int len = 0;

        if((cmd & 0x80) == 0) {                                 // highest bit is 0? SET command
            len = stCommandLen[cmd];                            // try to get the command length
        } else {                                                // highest bit is 1? GET command
            BYTE setEquivalent = cmd & 0x7f;                    // get the equivalent SET command (remove higherst bit)
            BYTE setLen = stCommandLen[setEquivalent];          // try to get the length of SET equivalent command

            if(setLen != 0) {                                   // if we got the SET equivalent command length, we support this command
                len = 1;                                        // the length of command is 1
            }
        }

        if(len == 0) {                                          // it's not GET command and we don't have this SET command defined?
            getFromCyclicBuffer(&cbStCommands);                 // just remove this byte and try the next byte
            continue;
        }

        if(cmd == STCMD_MEMORY_LOAD) {                          // special case: this command has length stored on index[3]
            if(cbStCommands.count < 4) {                        // not enough data to determine command length? quit
                return;
            }

            len = peekCyclicBufferWithOffset(&cbStCommands, 3); // get the data at offset [3] but don't really move
        }

        if(len > cbStCommands.count) {                          // if we don't have enough data in the buffer, quit
            return;
        }

        // if we got here, it's either GET or SET command that we support

        for(int i=0; i<len; i++) {                              // get the whole sequence
            bfr[i] = getFromCyclicBuffer(&cbStCommands);
        }

        // TODO: handle commands which need to be handled

        switch(bfr[0]) {
            // other commands
            //--------------------------------------------
            case STCMD_RESET:                           // reset keyboard - set everything to default values
            resetInternalIkbdVars();
            break;

            //--------------------------------------------
            case STCMD_PAUSE_OUTPUT:                    // disable any output to IKBD
            outputEnabled = false;
            break;

            //////////////////////////////////////////////
            // joystick related commands
            //--------------------------------------------
            case STCMD_SET_JOYSTICK_EVENT_REPORTING:    // mouse mode: event reporting
            joystickMode = JOYMODE_EVENT;
            joystickEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_SET_JOYSTICK_INTERROG_MODE:      // mouse mode: interrogation mode
            joystickMode = JOYMODE_INTERROGATION;
            joystickEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_DISABLE_JOYSTICKS:               // disable joystick; any valid joystick command enabled joystick
            joystickEnabled = false;
            break;
   
            //////////////////////////////////////////////
            // mouse related commands
            //--------------------------------------------
            case STCMD_SET_REL_MOUSE_POS_REPORTING:     // set relative mouse reporting
            mouseMode = MOUSEMODE_REL;
            mouseEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_SET_ABS_MOUSE_POS_REPORTING:     // set absolute mouse reporting
            mouseMode = MOUSEMODE_ABS;

            // read max X and Y max values
            absMouse.maxX = (((WORD) bfr[1]) << 8) | ((WORD) bfr[2]);
            absMouse.maxY = (((WORD) bfr[3]) << 8) | ((WORD) bfr[4]);

            fixAbsMousePos();                           // if absolute coordinates are out of bounds, fix them

            mouseEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_LOAD_MOUSE_POS:                  // load new absolute mouse position 
            // read max X and Y position values
            absMouse.x = (((WORD) bfr[2]) << 8) | ((WORD) bfr[3]);
            absMouse.y = (((WORD) bfr[4]) << 8) | ((WORD) bfr[5]);

            fixAbsMousePos();                           // if absolute coordinates are out of bounds, fix them

            mouseEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_SET_MOUSE_BTN_ACTION:            // set position reporting for absolute mouse mode on mouse button press
            mouseAbsBtnAct = MOUSEBTN_REPORT_NOTHING;

            if(bfr[1] & 0x01) {                         // if should report on PRESS
                mouseAbsBtnAct |= MOUSEBTN_REPORT_PRESS;
            }

            if(bfr[1] & 0x02) {                         // if should report on RELEASE
                mouseAbsBtnAct |= MOUSEBTN_REPORT_RELEASE;
            }
            mouseEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_SET_Y_AT_TOP:                    // Y=0 at top
            mouseY0atTop = true;
            mouseEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_SET_Y_AT_BOTTOM:                 // Y=0 at bottom
            mouseY0atTop = false;
            mouseEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_DISABLE_MOUSE:                   // disable mouse; any valid mouse command enables mouse
            mouseEnabled = false;
            break;
        }

        if(bfr[0] != STCMD_PAUSE_OUTPUT) {              // if this command is not PAUSE OUTPUT command, then enavle output
            outputEnabled = true;
        }

    }
}

void Ikbd::fixAbsMousePos(void)
{
    if(absMouse.x < 0) {                                            // if X is out of boundary, fix it
        absMouse.x = 0;
    }

    if(absMouse.x > absMouse.maxX) {                                // if X is out of boundary, fix it
        absMouse.x = absMouse.maxX;
    }

    if(absMouse.y < 0) {                                            // if Y is out of boundary, fix it
        absMouse.y = 0;
    }

    if(absMouse.y > absMouse.maxY) {                                // if Y is out of boundary, fix it
        absMouse.y = absMouse.maxY;
    }
}

void Ikbd::resetInternalIkbdVars(void)
{
    outputEnabled   = true;

    mouseMode       = MOUSEMODE_REL;
    mouseEnabled    = true;
    mouseY0atTop    = true;
    mouseAbsBtnAct  = MOUSEBTN_REPORT_NOTHING;

    absMouse.maxX   = 640;
    absMouse.maxY   = 400;
    absMouse.x      = 0;
    absMouse.y      = 0;

    joystickMode    = JOYMODE_EVENT;
    joystickEnabled = true;
}

void Ikbd::processKeyboardData(void)
{
    BYTE bfr[128];

    if(cbKeyboardData.count <= 0) {                             // no data? quit
        return;
    }

    while(cbKeyboardData.count > 0) {                           // while there are some data, process
        BYTE val = peekCyclicBuffer(&cbKeyboardData);           // get the data, but don't move the get pointer, because we might fail later

        if(val >= 0xf6 && val <= 0xff) {                        // if this a special code?
            BYTE index = val - 0xf6;                            // convert it to table index

            BYTE len = specialCodeLen[index];                   // get length of the special sequence from table

            if(len > cbKeyboardData.count) {                    // not enough data in buffer? Maybe next time... Quit.
                return;
            }

            for(int i=0; i<len; i++) {                          // get the whole sequence
                bfr[i] = getFromCyclicBuffer(&cbKeyboardData);
            }

            fdWrite(fdUart, bfr, len);                         // send the whole sequence to ST
            continue;
        }

        // if we got here, it's not a special code, just make / break keyboard code
        val = getFromCyclicBuffer(&cbKeyboardData);             // get data from buffer
        fdWrite(fdUart, &val, 1);                               // send byte to ST
    }
}

void Ikbd::initCyclicBuffer(TCyclicBuff *cb)
{
    cb->count    = 0;
    cb->addPos   = 0;
    cb->getPos   = 0;
}

void Ikbd::addToCyclicBuffer(TCyclicBuff *cb, BYTE val)
{
    if(cb->count >= CYCLIC_BUF_SIZE) {  // buffer full? quit
        return;
    }
    cb->count++;                        // update count

    cb->buf[cb->addPos] = val;          // store data

    cb->addPos++;                       // update 'add' position
    cb->addPos = cb->addPos & CYCLIC_BUF_MASK;
}

BYTE Ikbd::getFromCyclicBuffer(TCyclicBuff *cb)
{
    if(cb->count == 0) {                // buffer empty? quit
        return 0;
    }
    cb->count--;                        // update count

    BYTE val = cb->buf[cb->getPos];     // get data

    cb->getPos++;                       // update 'get' position
    cb->getPos = cb->getPos & CYCLIC_BUF_MASK;

    return val;
}

BYTE Ikbd::peekCyclicBuffer(TCyclicBuff *cb)
{
    if(cb->count == 0) {                // buffer empty? 
        return 0;
    }

    BYTE val = cb->buf[cb->getPos];     // just get the data
    return val;
}

BYTE Ikbd::peekCyclicBufferWithOffset(TCyclicBuff *cb, int offset)
{
    if(offset >= cb->count) {                                           // not enough data in buffer? quit
        return 0;
    }

    int getPosWithOffet = (cb->getPos + offset) & CYCLIC_BUF_MASK;      // calculate get position

    BYTE val = cb->buf[getPosWithOffet];                                // get data
    return val;
}

void Ikbd::processMouse(input_event *ev)
{
    if(ev->type == EV_KEY) {		// on button press
	    int btnNew = mouseBtnNow;
		
        bool down   = false;
        bool up     = false;

	    switch(ev->code) {
		    case BTN_LEFT:		
			if(ev->value == 1) {				// on DOWN - add bit
			    btnNew |= 2;
                down = true;
		    } else {						    // on UP - remove bit
		    	btnNew &= ~2; 
                up = true;
			}
			break;
		
    		case BTN_RIGHT:		
			if(ev->value == 1) {				// on DOWN - add bit
				btnNew |= 1; 
                down = true;
			} else {						    // on UP - remove bit
				btnNew &= ~1; 
                up = true;
			}
			break;
		}
			
		if(btnNew == mouseBtnNow) {							    // mouse buttons didn't change? 
			return;
		}

		mouseBtnNow = btnNew;								    // store new button states

        if(mouseMode == MOUSEMODE_ABS) {                        // for absolute mouse mode
            if( (down   && ((mouseAbsBtnAct & MOUSEBTN_REPORT_PRESS)    != 0)) ||       // if btn was pressed and we should report abs position on press
                (up     && ((mouseAbsBtnAct & MOUSEBTN_REPORT_RELEASE)  != 0)) ) {      // if btn was released and we should report abs position on release
                sendMousePosAbsolute(fdUart);
            }            
        } else {                                                // for relative mouse mode
            sendMousePosRelative(fdUart, mouseBtnNow, 0, 0);    // send them to ST
        }
		return;
	}
		
	if(ev->type == EV_REL) {
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

//        Debug::out("\nEV_KEY: code %d, value %d, stKey: %02x", ev->code, ev->value, stKey);

        if(fdUart == -1) {                          // no UART open? quit
            return;
        }

        BYTE bfr;
        bfr = stKey;
    
    	int res = fdWrite(fdUart, &bfr, 1); 

    	if(res < 0) {
	    	Debug::out("processKeyboard - sending to ST failed, errno: %d", errno);
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
        if(jse->number >= JOYAXIS) {                // if the index of axis would be out of index, quit
            return;
        } 

	    js->axis[ jse->number ] = jse->value;       // store new axis value
    } else if(jse->type == JS_EVENT_BUTTON) {
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
            button = 1;
        }
    }

    if(fdUart == -1) {                  // nowhere to send data? quit
        return;
    }

    // port 0: mouse + joystick
    // port 1: joystick

    if((dirTotal != js->lastDir) || (button != js->lastBtn)) {  // direction or button changed?
        js->lastDir = dirTotal;
        js->lastBtn = button;
    
        if(button != 0) {                                       // if the button is pressed, extend the dirTotal by button flag
            dirTotal |= JOYDIR_BUTTON;
        }

        sendJoyState(joyNumber, dirTotal);                      // report current direction and buttons
    }
}

// the following works when joystick event reporting is enabled
void Ikbd::sendJoyState(int joyNumber, int dirTotal)
{
    BYTE bfr[2];
    int res;

    // first set the joystick 0 / 1 tag 
    if(!swapJoys) {                 // joysticks NOT swapped?
        if(joyNumber == 0) {        // joy 0
            bfr[0] = 0xfe;
        } else {                    // joy 1
            bfr[0] = 0xff;
        }
    } else {                        // joysticks ARE swapped?
        if(joyNumber == 0) {        // joy 0
            bfr[0] = 0xff;
        } else {                    // joy 1
            bfr[0] = 0xfe;
        }
    }

    bfr[1] = dirTotal;

    res = fdWrite(fdUart, bfr, 2); 

    if(res < 0) {
        Debug::out("write to uart (0) failed, errno: %d", errno);
    }
}

// this can be used to send joystick button states when joystick event reporting is not enabled (it reports joy buttons as mouse buttons)
void Ikbd::sendJoy0State(void)
{
    int bothButtons = 0xf8;     // neutral position
    if(joystick[0].lastBtn) {
        bothButtons |= 0x02;    // left mouse  button, joy0 button
    }
    if(joystick[1].lastBtn) {
        bothButtons |= 0x01;    // right mouse button, joy1 button
    }

    BYTE bfr[3];
    bfr[0] = bothButtons;
    bfr[1] = 0;
    bfr[2] = 0;

    int res = fdWrite(fdUart, bfr, 3); 

    if(res < 0) {
        Debug::out("write to uart (1) failed, errno: %d", errno);
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
        Debug::out("Failed to open input device (%s): %s", what, fullPath);
        return;
    }

    in->fd = fd;
    strcpy(in->devPath, fullPath);
    Debug::out("Got device (%s): %s", what, fullPath);
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

void Ikbd::fillStCommandsLengthTable(void)
{
    memset(stCommandLen, 0, 256);       // init table with zeros

    stCommandLen[STCMD_RESET]                           = STCMD_RESET_LEN;
    stCommandLen[STCMD_SET_MOUSE_BTN_ACTION]            = STCMD_SET_MOUSE_BTN_ACTION_LEN;
    stCommandLen[STCMD_SET_REL_MOUSE_POS_REPORTING]     = STCMD_SET_REL_MOUSE_POS_REPORTING_LEN;
    stCommandLen[STCMD_SET_ABS_MOUSE_POS_REPORTING]     = STCMD_SET_ABS_MOUSE_POS_REPORTING_LEN;
    stCommandLen[STCMD_SET_MOUSE_KEYCODE_MODE]          = STCMD_SET_MOUSE_KEYCODE_MODE_LEN;
    stCommandLen[STCMD_SET_MOUSE_THRESHOLD]             = STCMD_SET_MOUSE_THRESHOLD_LEN;
    stCommandLen[STCMD_SET_MOUSE_SCALE]                 = STCMD_SET_MOUSE_SCALE_LEN;
    stCommandLen[STCMD_INTERROGATE_MOUSE_POS]           = STCMD_INTERROGATE_MOUSE_POS_LEN;
    stCommandLen[STCMD_LOAD_MOUSE_POS]                  = STCMD_LOAD_MOUSE_POS_LEN;
    stCommandLen[STCMD_SET_Y_AT_BOTTOM]                 = STCMD_SET_Y_AT_BOTTOM_LEN;
    stCommandLen[STCMD_SET_Y_AT_TOP]                    = STCMD_SET_Y_AT_TOP_LEN;
    stCommandLen[STCMD_RESUME]                          = STCMD_RESUME_LEN;
    stCommandLen[STCMD_DISABLE_MOUSE]                   = STCMD_DISABLE_MOUSE_LEN;
    stCommandLen[STCMD_PAUSE_OUTPUT]                    = STCMD_PAUSE_OUTPUT_LEN;
    stCommandLen[STCMD_SET_JOYSTICK_EVENT_REPORTING]    = STCMD_SET_JOYSTICK_EVENT_REPORTING_LEN;
    stCommandLen[STCMD_SET_JOYSTICK_INTERROG_MODE]      = STCMD_SET_JOYSTICK_INTERROG_MODE_LEN;
    stCommandLen[STCMD_JOYSTICK_INTERROGATION]          = STCMD_JOYSTICK_INTERROGATION_LEN;
    stCommandLen[STCMD_SET_JOYSTICK_MONITORING]         = STCMD_SET_JOYSTICK_MONITORING_LEN;
    stCommandLen[STCMD_SET_FIRE_BUTTON_MONITORING]      = STCMD_SET_FIRE_BUTTON_MONITORING_LEN;
    stCommandLen[STCMD_SET_JOYSTICK_KEYCODE_MODE]       = STCMD_SET_JOYSTICK_KEYCODE_MODE_LEN;
    stCommandLen[STCMD_DISABLE_JOYSTICKS]               = STCMD_DISABLE_JOYSTICKS_LEN;
    stCommandLen[STCMD_TIMEOFDAY_CLOCK_SET]             = STCMD_TIMEOFDAY_CLOCK_SET_LEN;
    stCommandLen[STCMD_INTERROGATE_TIMEOFDAT_CLOCK]     = STCMD_INTERROGATE_TIMEOFDAT_CLOCK_LEN;
    stCommandLen[STCMD_MEMORY_LOAD]                     = STCMD_MEMORY_LOAD_LEN;
    stCommandLen[STCMD_MEMORY_READ]                     = STCMD_MEMORY_READ_LEN;
    stCommandLen[STCMD_CONTROLLER_EXECUTE]              = STCMD_CONTROLLER_EXECUTE_LEN;
}

void Ikbd::fillSpecialCodeLengthTable(void)
{
    specialCodeLen[KEYBDATA_STATUS		- KEYBDATA_SPECIAL_LOWEST] = KEYBDATA_STATUS_LEN;
    specialCodeLen[KEYBDATA_MOUSE_ABS	- KEYBDATA_SPECIAL_LOWEST] = KEYBDATA_MOUSE_ABS_LEN;
    specialCodeLen[KEYBDATA_MOUSE_REL8	- KEYBDATA_SPECIAL_LOWEST] = KEYBDATA_MOUSE_REL8_LEN;
    specialCodeLen[KEYBDATA_MOUSE_REL9	- KEYBDATA_SPECIAL_LOWEST] = KEYBDATA_MOUSE_REL9_LEN;
    specialCodeLen[KEYBDATA_MOUSE_RELA	- KEYBDATA_SPECIAL_LOWEST] = KEYBDATA_MOUSE_RELA_LEN;
    specialCodeLen[KEYBDATA_MOUSE_RELB	- KEYBDATA_SPECIAL_LOWEST] = KEYBDATA_MOUSE_RELB_LEN;
    specialCodeLen[KEYBDATA_TIMEOFDAY	- KEYBDATA_SPECIAL_LOWEST] = KEYBDATA_TIMEOFDAY_LEN;
    specialCodeLen[KEYBDATA_JOY_BOTH	- KEYBDATA_SPECIAL_LOWEST] = KEYBDATA_JOY_BOTH_LEN;
    specialCodeLen[KEYBDATA_JOY0		- KEYBDATA_SPECIAL_LOWEST] = KEYBDATA_JOY0_LEN;
    specialCodeLen[KEYBDATA_JOY1		- KEYBDATA_SPECIAL_LOWEST] = KEYBDATA_JOY1_LEN;
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
        Debug::out("addToTable -- Can't add pair %d - %d - out of range.", pcKey, stKey);
        return;
    }

    tableKeysPcToSt[pcKey] = stKey;
}

void Ikbd::sendMousePosAbsolute(int fd)
{
    if(fd == -1) {                      // no UART open?
        return;
    }

    if(!mouseEnabled) {                 // if mouse not enabled, don't send anything
        return;
    }

    if(mouseMode != MOUSEMODE_ABS) {    // if we're not in absolute mouse mode, quit
        return;
    }


}

void Ikbd::sendMousePosRelative(int fd, BYTE buttons, BYTE xRel, BYTE yRel)
{
    if(fd == -1) {                      // no UART open? quit
        return;
    }

    if(!mouseEnabled) {                 // if mouse not enabled, don't send anything
        return;
    }

    if(mouseMode != MOUSEMODE_REL) {    // send relative mouse position changes only in relative mouse mode
        return;
    }

	BYTE bfr[3];
	
    char yRelVal = yRel;

    if(!mouseY0atTop) {                 // if mouse Y=0 is at bottom, invert the Y axis
        yRelVal = yRelVal * (-1);   
    }

	bfr[0] = 0xf8 | buttons;
	bfr[1] = xRel;
	bfr[2] = yRel;
	
	int res = fdWrite(fd, bfr, 3); 

	if(res < 0) {
		Debug::out("sendMousePosRelative failed, errno: %d", errno);
	}
}

int Ikbd::serialSetup(termios *ts) 
{
	int fd;
	
    fdUart = -1;

	fd = open(UARTFILE, O_RDWR | O_NOCTTY | O_NDELAY | O_NONBLOCK);
	if(fd == -1) {
        Debug::out("Failed to open %s", UARTFILE);
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
    if(fd == -1) {                                  // no fd? quit
        return 0;
    }

    if(!outputEnabled) {                            // output not enabled? Pretend that it was sent...
        return cnt;
    }

    int res = write(fd, bfr, cnt);                  // send content
    return res;
}



