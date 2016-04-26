#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "global.h"
#include "debug.h"
#include "utils.h"
#include "statusreport.h"

#include "ikbd.h"

void Ikbd::processReceivedCommands(void)
{
    if(fdUart == -1) {                                          // uart not open? quit
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
                    cbStCommands.add(bfr[i + 1]);
                } else if(bfr[i] == UARTMARK_KEYBDATA) {        // this is original keyboard data, add it to keyboard data buffer
                    cbKeyboardData.add(bfr[i + 1]);
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
    #ifndef SPYIKBD                                             // if not spying on IKBD, process this normaly
        if(cbStCommands.count > 0) {                            // got ST commands? process them
            processStCommands();
        }

        if(cbKeyboardData.count > 0) {                          // got keyboard data? process them
            processKeyboardData();
        }
    #else                                                       // if spying on IKBD, just resend data
        if(cbStCommands.count > 0) {                            // got ST commands? process them
            dumpBuffer(true);
        }

        if(cbKeyboardData.count > 0) {                          // got keyboard data? process them
            dumpBuffer(false);
        }
    #endif
}

void Ikbd::dumpBuffer(bool fromStNotKeyboard)
{
    std::string text;
    CyclicBuff *cb;
    
    if(fromStNotKeyboard) {
        text = "ST says      : ";
        cb = &cbStCommands;
    } else {
        text = "Keyboard says: ";
        cb = &cbKeyboardData;
    }
    
    char bfr[16];

    while(cb->count > 0) {                      // if there's some data
        BYTE val = cb->get();                   // get data
        
        sprintf(bfr, "%02x ", val);             // add hex dump of this data
        text += bfr;
        
        if(!fromStNotKeyboard) {                // if it's from keyboard, send it to ST
            fdWrite(fdUart, &val, 1);
        }
    }
    
    ikbdLog(text.c_str());                      // show the hex dump of data
}

void Ikbd::processStCommands(void)
{
    BYTE bfr[512];

    if(cbStCommands.count <= 0) {                               // no data? quit
        return;
    }

    statuses.ikbdSt.aliveTime = Utils::getCurrentMs();
    statuses.ikbdSt.aliveSign = ALIVE_IKBD_CMD;

    while(cbStCommands.count > 0) {                             // while there are some data, process
        BYTE cmd = cbStCommands.peek();                         // get the data, but don't move the get pointer, because we might fail later
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

        ikbdLog( "Ikbd::processStCommands -- got command %02x, len: %d, cb contains %d bytes", cmd, len, cbStCommands.count);
        
        if(len == 0) {                                          // it's not GET command and we don't have this SET command defined?
            ikbdLog( "Ikbd::processStCommands -- not GET cmd, and we don't know what to do");
            cbStCommands.get();                                 // just remove this byte and try the next byte
            continue;
        }

        if(cmd == STCMD_MEMORY_LOAD) {                          // special case: this command has length stored on index[3]
            ikbdLog("Ikbd::processStCommands -- ST says: STCMD_MEMORY_LOAD");
            
            if(cbStCommands.count < 4) {                        // not enough data to determine command length? quit
                return;
            }

            len = cbStCommands.peekWithOffset(3);               // get the data at offset [3] but don't really move
        }

        if(len > cbStCommands.count) {                          // if we don't have enough data in the buffer, quit
            ikbdLog( "Ikbd::processStCommands -- not enough data in buffer, quitting (%d > %d)", len, cbStCommands.count);
            return;
        }

        // if we got here, it's either GET or SET command that we support

        for(int i=0; i<len; i++) {                              // get the whole sequence
            bfr[i] = cbStCommands.get();
        }

        if(bfr[0] != STCMD_PAUSE_OUTPUT) {              		// if this command is not PAUSE OUTPUT command, then enable output
            outputEnabled = true;
        }

		if((cmd & 0x80) != 0) {									// is it a GET command?
			processGetCommand(cmd);
			continue;
		}
		
        switch(bfr[0]) {
            // other commands
            //--------------------------------------------
            case STCMD_RESET:                           // reset keyboard - set everything to default values
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_RESET");

            resetInternalIkbdVars();
			
			if(ceIkbdMode != CE_IKBDMODE_SOLO) {		// if we're not in solo mode, don't answer
				break;
			}

			BYTE derp[2];
			derp[0] = 0xf0;
			
			fdWrite(fdUart, derp, 1);					// send report that everything is OK 
            break;

            //--------------------------------------------
            case STCMD_PAUSE_OUTPUT:                    // disable any output to IKBD
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_PAUSE_OUTPUT");

            outputEnabled = false;
            break;

            //////////////////////////////////////////////
            // joystick related commands
            //--------------------------------------------
            case STCMD_SET_JOYSTICK_EVENT_REPORTING:    // mouse mode: event reporting
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_SET_JOYSTICK_EVENT_REPORTING");

            joystickMode = JOYMODE_EVENT;
            joystickEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_SET_JOYSTICK_INTERROG_MODE:      // mouse mode: interrogation mode
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_SET_JOYSTICK_INTERROG_MODE");

            joystickMode = JOYMODE_INTERROGATION;
            joystickEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_JOYSTICK_INTERROGATION:
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_JOYSTICK_INTERROGATION");
            
			joystickEnabled = true;
			
			if(ceIkbdMode != CE_IKBDMODE_SOLO) {		// if we're not in solo mode, don't answer (will be handled when the data from keyboard arrive)
				break;
			}
			
			if(joystickMode != JOYMODE_INTERROGATION) {	// if we're not in joy interrogation mode, ignore
				break;
			}

			sendBothJoyReport();
			break;

            //--------------------------------------------
            case STCMD_DISABLE_JOYSTICKS:               // disable joystick; any valid joystick command enabled joystick
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_DISABLE_JOYSTICKS");
            
            joystickEnabled = false;
            break;
   
            //////////////////////////////////////////////
            // mouse related commands
            //--------------------------------------------
            case STCMD_SET_REL_MOUSE_POS_REPORTING:     // set relative mouse reporting
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_SET_REL_MOUSE_POS_REPORTING");
            
            mouseMode = MOUSEMODE_REL;
            mouseEnabled = true;
            break;

            //--------------------------------------------
			case STCMD_SET_MOUSE_KEYCODE_MODE:			// set keycode mouse reporting
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_SET_MOUSE_KEYCODE_MODE");

            mouseMode = MOUSEMODE_KEYCODE;
            mouseEnabled = true;
            break;

            //--------------------------------------------
			case STCMD_SET_MOUSE_THRESHOLD:
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_SET_MOUSE_THRESHOLD");
                        
			relMouse.threshX	= bfr[1];
			relMouse.threshY	= bfr[2];
            mouseEnabled = true;
            break;
			
            //--------------------------------------------
			case STCMD_SET_MOUSE_SCALE:
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_SET_MOUSE_SCALE");
            
			absMouse.scaleX		= bfr[1];
			absMouse.scaleY		= bfr[2];
            mouseEnabled = true;
            break;			
            
			//--------------------------------------------
            case STCMD_SET_ABS_MOUSE_POS_REPORTING:     // set absolute mouse reporting
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_SET_ABS_MOUSE_POS_REPORTING");
            
            mouseMode = MOUSEMODE_ABS;

            // read max X and Y max values
            absMouse.maxX = (((WORD) bfr[1]) << 8) | ((WORD) bfr[2]);
            absMouse.maxY = (((WORD) bfr[3]) << 8) | ((WORD) bfr[4]);

            ikbdLog( "new mouse max: [%04x, %04x]", absMouse.maxX, absMouse.maxY);

            fixAbsMousePos();                           // if absolute coordinates are out of bounds, fix them

            mouseEnabled = true;
            break;

            //--------------------------------------------
			case STCMD_INTERROGATE_MOUSE_POS:			// get the current absolute mouse position
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_INTERROGATE_MOUSE_POS");
        
            mouseEnabled = true;

			if(mouseMode != MOUSEMODE_ABS) {			// this is only valid in absolute mouse mode
                ikbdLog( "Ikbd::processStCommands -- STCMD_INTERROGATE_MOUSE_POS -- not sending anything because not in ABS mouse mode");
				break;
			}
			
			if(ceIkbdMode == CE_IKBDMODE_INJECT && !gotUsbMouse()) {	// when working in INJECT mode and don't have mouse, don't answer
                ikbdLog( "Ikbd::processStCommands -- STCMD_INTERROGATE_MOUSE_POS -- not sending anything because we're in INJECT mode and we don't have USB mouse");
				break;
			} else {
                ikbdLog( "Ikbd::processStCommands -- STCMD_INTERROGATE_MOUSE_POS -- will send USB mouse position (either we're in SOLO mode and / or we gotUsbMouse() )");
            }
			
			sendMousePosAbsolute(fdUart, absMouse.buttons);				// send position and accumulated buttons		
			absMouse.buttons = 0;										// no buttons for now
			break;
			
            //--------------------------------------------
            case STCMD_LOAD_MOUSE_POS:                  // load new absolute mouse position 
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_LOAD_MOUSE_POS");
            
            // read max X and Y position values
            absMouse.x = (((WORD) bfr[2]) << 8) | ((WORD) bfr[3]);
            absMouse.y = (((WORD) bfr[4]) << 8) | ((WORD) bfr[5]);

            ikbdLog( "new mouse pos  : [%04x, %04x]", absMouse.x, absMouse.y);

            fixAbsMousePos();                           // if absolute coordinates are out of bounds, fix them
            mouseEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_SET_MOUSE_BTN_ACTION:            // set position reporting for absolute mouse mode on mouse button press
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_SET_MOUSE_BTN_ACTION - param: %02X", (int) bfr[1]);
            
            mouseAbsBtnAct = bfr[1];					// store flags what we should report
            mouseEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_SET_Y_AT_TOP:                    // Y=0 at top
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_SET_Y_AT_TOP");
            
            mouseY0atTop = true;
            mouseEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_SET_Y_AT_BOTTOM:                 // Y=0 at bottom
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_SET_Y_AT_BOTTOM");

            mouseY0atTop = false;
            mouseEnabled = true;
            break;

            //--------------------------------------------
            case STCMD_DISABLE_MOUSE:                   // disable mouse; any valid mouse command enables mouse
            ikbdLog( "Ikbd::processStCommands -- ST says: STCMD_DISABLE_MOUSE");

            mouseEnabled = false;
            break;
        }
    }
}

void Ikbd::processGetCommand(BYTE getCmd)
{
	if(ceIkbdMode != CE_IKBDMODE_SOLO) {							// not SOLO mode? don't reply!
		return;
	}

	BYTE setEquivalent = getCmd & 0x7f;								// remove highest bit

	bool send = false;
	
	BYTE bfr[8];
	memset(bfr, 0, 8);
	
	bfr[0] = KEYBDATA_STATUS;
	
	switch(setEquivalent) {
		case STCMD_SET_REL_MOUSE_POS_REPORTING:		// relative mouse: report mouse mode
        ikbdLog( "Ikbd::processGetCommand -- STCMD_SET_REL_MOUSE_POS_REPORTING");

		bfr[1] = mouseMode;
		send = true;
		break;
		
		case STCMD_SET_ABS_MOUSE_POS_REPORTING:		// absolute mouse: report mode and maximums
        ikbdLog( "Ikbd::processGetCommand -- STCMD_SET_ABS_MOUSE_POS_REPORTING");

		bfr[1] = mouseMode;
		bfr[2] = (BYTE) (absMouse.maxX >> 8);
		bfr[3] = (BYTE)  absMouse.maxX;
		bfr[4] = (BYTE) (absMouse.maxY >> 8);
		bfr[5] = (BYTE)  absMouse.maxY;
		send = true;
		break;

		case STCMD_SET_MOUSE_KEYCODE_MODE:			// keycode mouse: report mode and deltas
        ikbdLog( "Ikbd::processGetCommand -- STCMD_SET_MOUSE_KEYCODE_MODE");

		bfr[1] = mouseMode;
		bfr[2] = keycodeMouse.deltaX;
		bfr[3] = keycodeMouse.deltaY;
		send = true;
		break;
		
		case STCMD_SET_Y_AT_TOP:					// report mouse Y=0 location (top or bottom)
		case STCMD_SET_Y_AT_BOTTOM:
        ikbdLog( "Ikbd::processGetCommand -- STCMD_SET_Y_AT_TOP or STCMD_SET_Y_AT_BOTTOM");

		bfr[1] = mouseY0atTop ? STCMD_SET_Y_AT_TOP : STCMD_SET_Y_AT_BOTTOM;
		send = true;
		break;
	
		case STCMD_DISABLE_MOUSE:					// report if mouse is disabled
        ikbdLog( "Ikbd::processGetCommand -- STCMD_DISABLE_MOUSE");
        
		bfr[1] = mouseEnabled ? 0 : STCMD_DISABLE_MOUSE;
		send = true;
		break;
		
		case STCMD_SET_JOYSTICK_EVENT_REPORTING:	// report joystick mode
		case STCMD_SET_JOYSTICK_INTERROG_MODE:
		case STCMD_SET_JOYSTICK_KEYCODE_MODE:
        ikbdLog( "Ikbd::processGetCommand -- STCMD_SET_JOYSTICK _EVENT_REPORTING or _INTERROG_MODE or _KEYCODE_MODE");
        
		bfr[1] = joystickMode;
		send = true;
		break;
		
		case STCMD_DISABLE_JOYSTICKS:				// report if joystick is disabled
        ikbdLog( "Ikbd::processGetCommand -- STCMD_DISABLE_JOYSTICK");

		bfr[1] = joystickEnabled ? 0 : STCMD_DISABLE_JOYSTICKS;
		send = true;
		break;

		case STCMD_SET_MOUSE_BTN_ACTION:			// report mouse button actions
        ikbdLog( "Ikbd::processGetCommand -- STCMD_SET_MOUSE_BTN_ACTION");

		bfr[1] = mouseAbsBtnAct;
		send = true;
		break;
		
		case STCMD_SET_MOUSE_THRESHOLD:				// report current mouse threshold
        ikbdLog( "Ikbd::processGetCommand -- STCMD_SET_MOUSE_THRESHOLD");

		bfr[1] = relMouse.threshX;
		bfr[2] = relMouse.threshY;
		send = true;
		break;
		
		case STCMD_SET_MOUSE_SCALE:					// report current mouse scale
        ikbdLog( "Ikbd::processGetCommand -- STCMD_SET_MOUSE_SCALE");
        
		bfr[1] = absMouse.scaleX;
		bfr[2] = absMouse.scaleY;
		send = true;
		break;
	}
	
	if(send) {										// if command was handled
		fdWrite(fdUart, bfr, 8);	
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

void Ikbd::processKeyboardData(void)
{
    BYTE bfr[128];

    if(cbKeyboardData.count <= 0) {                             // no data? quit
        return;
    }

    statuses.ikbdSt.aliveTime = Utils::getCurrentMs();          // store current time, so we won't have to call this many times in the loop
    statuses.ikbdSt.aliveSign = ALIVE_KEYDOWN;                  // we don't know at this moment if it's a key down event, but store it just to make sure that user will not see it's a ST IKBD command from ST, but something from original keyboard

    while(cbKeyboardData.count > 0) {                           // while there are some data, process
        BYTE val = cbKeyboardData.peek();                       // get the data, but don't move the get pointer, because we might fail later

        if(val >= KEYBDATA_SPECIAL_LOWEST && val <= 0xff) {     // if this a special code?
            BYTE index = val - KEYBDATA_SPECIAL_LOWEST;         // convert it to table index

            BYTE len = specialCodeLen[index];                   // get length of the special sequence from table

            if(len > cbKeyboardData.count) {                    // not enough data in buffer? Maybe next time... Quit.
                return;
            }

            for(int i=0; i<len; i++) {                          // get the whole sequence
                bfr[i] = cbKeyboardData.get();
            }
			
			ceIkbdMode = CE_IKBDMODE_INJECT;					// if we got at least one data sequence from original keyboard, switch to INJECT mode
			
			bool resendTheseData = true;						// reset this flag to not resend these data
			
            BYTE stKeyboadCmd = bfr[0];                         // 0th byte is the original ST keyboard command code
            //---------------
            // just for the cause of status alive reporting
            if(stKeyboadCmd < KEYBDATA_SPECIAL_LOWEST) {                                            // lower than special command? it's key up / down event
                statuses.ikbdSt.aliveSign = ALIVE_KEYDOWN;
            } else if(stKeyboadCmd >= KEYBDATA_MOUSE_ABS && stKeyboadCmd <= KEYBDATA_MOUSE_RELB) {  // it's in the mouse command range? it's a mouse event
                statuses.ikbdSt.aliveSign = ALIVE_MOUSEVENT;
            } else if(stKeyboadCmd >= KEYBDATA_JOY_BOTH  && stKeyboadCmd <= KEYBDATA_JOY1) {        // it's in the joy command range? it's a joy event
                statuses.ikbdSt.aliveSign = ALIVE_JOYEVENT;
            }
            //---------------
            
			switch(stKeyboadCmd) {                              // handle data sent from original keyboard
				case KEYBDATA_MOUSE_ABS:						// report of absolute mouse position?
                ikbdLog( "Ikbd::processKeyboardData - keyboard says: KEYBDATA_MOUSE_ABS");

				if(gotUsbMouse()) {								// ...and we got USB mouse? Don't resend.
                    ikbdLog( "Ikbd::processKeyboardData - KEYBDATA_MOUSE_ABS -- got USB mouse, won't resend ABS pos from ST");
					resendTheseData = false;
				} else {
                    ikbdLog( "Ikbd::processKeyboardData - KEYBDATA_MOUSE_ABS -- no USB mouse, will resend ABS pos from ST");
                }
                
				break;
				
				//----------------------------------------------
				case KEYBDATA_JOY_BOTH:							// ST asked for both joystick states (interrogation) and this is the response
                ikbdLog( "Ikbd::processKeyboardData - keyboard says: KEYBDATA_JOY_BOTH");
            
				if(gotUsbJoy1()) {								// got joy 1?
					BYTE joy0state = joystick[0].lastDir | joystick[0].lastBtn;	// get state
					bfr[1] = joy0state;
				}

				if(gotUsbJoy2()) {								// got joy 2?
					BYTE joy1state = joystick[1].lastDir | joystick[1].lastBtn;	// get state
					bfr[2] = joy1state;
				}
				
				break;
				//----------------------------------------------
				
			}

			if(resendTheseData) {								// if we should resend this data
				fdWrite(fdUart, bfr, len);                      // send the whole sequence to ST
            }
            
			continue;
        }

        // if we got here, it's not a special code, just make / break keyboard code
        val = cbKeyboardData.get();                             // get data from buffer
        fdWrite(fdUart, &val, 1);                               // send byte to ST
    }
}

void Ikbd::sendBothJoyReport(void)
{
	BYTE bfr[3];
    int res;

	BYTE joy0state = joystick[0].lastDir | joystick[0].lastBtn;	// get state
	BYTE joy1state = joystick[1].lastDir | joystick[1].lastBtn;	// get state
	
	bfr[0] = KEYBDATA_JOY_BOTH;

	bfr[1] = joy0state;
	bfr[2] = joy1state;
	
	res = fdWrite(fdUart, bfr, 3); 

    if(res < 0) {
        logDebugAndIkbd(LOG_ERROR, "write to uart failed, errno: %d", errno);
    }
}

// the following works when joystick event reporting is enabled
void Ikbd::sendJoyState(int joyNumber, int dirTotal)
{
    BYTE bfr[2];
    int res;

    // first set the joystick 0 / 1 tag 
    if(joyNumber == 0) {        // joy 0
        bfr[0] = KEYBDATA_JOY0;
    } else {                    // joy 1
        bfr[0] = KEYBDATA_JOY1;
    }

    bfr[1] = dirTotal;

    res = fdWrite(fdUart, bfr, 2); 

    if(res < 0) {
        logDebugAndIkbd(LOG_ERROR, "write to uart (0) failed, errno: %d", errno);
    }
}

// this can be used to send joystick button states when joystick event reporting is not enabled (it reports joy buttons as mouse buttons)
void Ikbd::sendJoy0State(void)
{
    int bothButtons = KEYBDATA_MOUSE_REL8;     // neutral position
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
        logDebugAndIkbd(LOG_ERROR, "write to uart (1) failed, errno: %d", errno);
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
        logDebugAndIkbd(LOG_ERROR, "addToTable -- Can't add pair %d - %d - out of range.", pcKey, stKey);
        return;
    }

    tableKeysPcToSt[pcKey] = stKey;
}

void Ikbd::sendMousePosAbsolute(int fd, BYTE absButtons)
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

	// create the absolute mouse position report
	BYTE bfr[6];
	
	bfr[0] = KEYBDATA_MOUSE_ABS;
	bfr[1] = absButtons;
	bfr[2] = absMouse.x >> 8;
	bfr[3] = (BYTE) absMouse.x;
	bfr[4] = absMouse.y >> 8;
	bfr[5] = (BYTE) absMouse.y;

	int res = fdWrite(fd, bfr, 6); 

	if(res < 0) {
		logDebugAndIkbd(LOG_ERROR, "sendMousePosAbsolute failed, errno: %d", errno);
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
		logDebugAndIkbd(LOG_ERROR, "sendMousePosRelative failed, errno: %d", errno);
	}
}

