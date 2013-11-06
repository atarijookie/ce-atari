#include <stdio.h>
#include <string.h>

#include "../global.h"
#include "../native/scsi_defs.h"
#include "../acsidatatrans.h"

#include "settings.h"
#include "keys.h"
#include "configstream.h"
#include "configscreen_main.h"

extern "C" void outDebugString(const char *format, ...);

ConfigStream::ConfigStream()
{
	showingHomeScreen	= false;
	showingMessage		= false;
	screenChanged		= true;
	
    dataTrans = NULL;

    message.clear();
	createScreen_homeScreen();
}

ConfigStream::~ConfigStream()
{
	destroyCurrentScreen();
	destroyScreen(message);
}

void ConfigStream::setAcsiDataTrans(AcsiDataTrans *dt)
{
    dataTrans = dt;
}

void ConfigStream::processCommand(BYTE *cmd)
{
    #define READ_BUFFER_SIZE    (5 * 1024)
    static BYTE readBuffer[READ_BUFFER_SIZE];
    int streamCount;

    if(cmd[1] != 'C' || cmd[2] != 'E' || cmd[3] != HOSTMOD_CONFIG) {        // not for us?
        return;
    }

    dataTrans->clear();                 // clean data transporter before handling

    switch(cmd[4]) {
        case CFG_CMD_IDENTIFY:          // identify?
        dataTrans->addData((unsigned char *)"CosmosEx config console", 23, true);       // add identity string with padding
        dataTrans->setStatus(SCSI_ST_OK);
        break;

    case CFG_CMD_KEYDOWN:
        onKeyDown(cmd[5]);                                                // first send the key down signal
        streamCount = getStream(false, readBuffer, READ_BUFFER_SIZE);     // then get current screen stream

        dataTrans->addData(readBuffer, streamCount, true);                              // add data and status, with padding to multiple of 16 bytes
        dataTrans->setStatus(SCSI_ST_OK);

        outDebugString("handleConfigStream -- CFG_CMD_KEYDOWN -- %d bytes\n", streamCount);
        break;

    case CFG_CMD_GO_HOME:
        streamCount = getStream(true, readBuffer, READ_BUFFER_SIZE);      // get homescreen stream

        dataTrans->addData(readBuffer, streamCount, true);                              // add data and status, with padding to multiple of 16 bytes
        dataTrans->setStatus(SCSI_ST_OK);

        outDebugString("handleConfigStream -- CFG_CMD_GO_HOME -- %d bytes\n", streamCount);
        break;

    default:                            // other cases: error
        dataTrans->setStatus(SCSI_ST_CHECK_CONDITION);
        break;
    }

    dataTrans->sendDataAndStatus();     // send all the stuff after handling, if we got any
}

void ConfigStream::onKeyDown(BYTE key)
{
	std::vector<ConfigComponent *> &scr = showingMessage ? message : screen;		// if we should show message, set reference to message, otherwise set reference to screen

	int focused = -1, firstFocusable = -1, lastFocusable = -1;

	// go through the current screen and find focused component, also first focusable component
    for(WORD i=0; i<scr.size(); i++) {
		ConfigComponent *c = scr[i];
		
		if(c->isFocused()) {						// if found focused component, store index
			focused = i;
		}
		
		if(firstFocusable == -1) {					// if found focusable component, store index
			if(c->canFocus()) {
				firstFocusable = i;
			}
		}
		
		if(c->canFocus()) {							// if this is focusable, then store it as last focusable (at the end it will contain the last focusable)
			lastFocusable = i;
		}
	}
	
	if(firstFocusable == -1) {						// nothing focusable? do nothing
		return;
	}
	
	if(focused == -1) {								// there is something focusable, but nothing has focus? focus it
		focused = firstFocusable;
	}
	
	ConfigComponent *curr = scr[focused];		// focus this component
	curr->setFocus(true);

	int prevFocusable = -1, nextFocusable = -1;		// now find previous and next focusable item in the list of components
    for(WORD i=0; i<scr.size(); i++) {
		ConfigComponent *c = scr[i];
		
		if(!c->canFocus()) {						// can't focus? fuck you!						
			continue;
		}
		
		if(i < focused) {							// if we're bellow currently focused item, store each found index (go near focused component)
			prevFocusable = i;
		}
		
		if(i > focused) {							// if we're above currently focused item, store only first found index (don't go far from focused component)
			if(nextFocusable == -1) {
				nextFocusable = i;
			}
		}
	}
	
    if(key == KEY_UP) {							// arrow up
        curr->setFocus(false);					// unfocus this component
			
        if(prevFocusable != -1) {				// got previous focusable item?
            curr = scr[prevFocusable];          // move to the previous component
        } else if(lastFocusable != -1) {		// got last focusable?
            curr = scr[lastFocusable];          // move to the last component (wrap around)
        }
			
        curr->setFocus(true);					// focus this component
			
        return;
    }
		
    if(key == KEY_DOWN) {							// arrow down
        curr->setFocus(false);					// unfocus this component
			
        if(nextFocusable != -1) {				// got next focusable item?
            curr = scr[nextFocusable];		// move to the next component
        } else if(firstFocusable != -1) {		// got first focusable?
            curr = scr[firstFocusable];		// move to the first component (wrap around)
        }
			
        curr->setFocus(true);					// focus this component

        return;
    }

	// if it got here, we didn't handle it, let the component handle it
    curr->onKeyPressed(key);
}

int ConfigStream::getStream(bool homeScreen, BYTE *bfr, int maxLen)
{
	int totalCnt = 0;

	if(showingMessage) {								// if we're showing the message
		if(homeScreen) {								// but we should show home screen
			hideMessageScreen();						// hide the message
		}
	}
	
	if(homeScreen) {									// if we should show the stream for homescreen
		if(!showingHomeScreen) {						// and we're not showing it yet
			createScreen_homeScreen();					// create homescreen
		}
	}
	
	if(screen.size() == 0) {							// if we wanted to show current screen, but there is nothing, just show home screen
		createScreen_homeScreen();
	}

	memset(bfr, 0, maxLen);								// clear the buffer

	std::vector<ConfigComponent *> &scr = showingMessage ? message : screen;		// if we should show message, set reference to message, otherwise set reference to screen
	
	if(screenChanged) {									// if screen changed, clear screen (CLEAR_HOME) and draw it all
		bfr[0] = 27;		
		bfr[1] = 'E';
		
		bfr += 2;
		totalCnt += 2;
	}

	int focused = -1;
	
    for(WORD i=0; i<scr.size(); i++) {				// go through all the components of screen and gather their streams
		ConfigComponent *c = scr[i];
		
		if(c->isFocused()) {							// if this component has focus, store it's index
			focused = i;
		}
		
		int gotLen;
		c->getStream(screenChanged, bfr, gotLen);		// if screenChanged, will get full stream, not only change
		bfr += gotLen;
		
		totalCnt += gotLen;
	}

	if(focused != -1) {									// if got some component with focus
		int gotLen;
		ConfigComponent *c = scr[focused];
		c->terminal_addGotoCurrentCursor(bfr, gotLen);	// position the cursor at the right place

		bfr			+= gotLen;
		totalCnt	+= gotLen;
	}
	
	screenChanged = false;

    return totalCnt;                                    // return the count of bytes used
}

void onMessageOk(ConfigComponent *sender)
{
	ConfigStream::instance().hideMessageScreen();
}

void ConfigStream::showMessageScreen(char *msgTitle, char *msgTxt)
{
	screenChanged = true;
	
	showingMessage = true;
	destroyScreen(message);

	screen_addHeaderAndFooter(message, msgTitle);
	
	ConfigComponent *comp;

	comp = new ConfigComponent(ConfigComponent::label, msgTxt, 240, 0, 10);
	message.push_back(comp);
	
	comp = new ConfigComponent(ConfigComponent::button, " OK ", 4, 17, 20);
	comp->setOnEnterFunction(onMessageOk);
	comp->setFocus(true);
	message.push_back(comp);
}

void ConfigStream::hideMessageScreen(void)
{
	screenChanged = true;

	showingMessage = false;
	destroyScreen(message);
}

void ConfigStream::destroyCurrentScreen(void)
{
	destroyScreen(screen);
}

void ConfigStream::destroyScreen(std::vector<ConfigComponent *> &scr)
{
    for(WORD i=0; i<scr.size(); i++) {				// go through this screen, delete all components
		ConfigComponent *c = scr[i];
		delete c;
	}
	
	scr.clear();									// now clear the list
}


void ConfigStream::setFocusToFirstFocusable(void)
{
    for(WORD i=0; i<screen.size(); i++) {			// go through the current screen
		ConfigComponent *c = screen[i];

		if(c->canFocus()) {
			c->setFocus(true);
			return;
		}
	}
}

int ConfigStream::checkboxGroup_getCheckedId(int groupId) 
{
    for(WORD i=0; i<screen.size(); i++) {					// go through the current screen and find the checked checkbox
		ConfigComponent *c = screen[i];
		
		int thisGroupId, checkboxId;
		c->getCheckboxGroupIds(thisGroupId, checkboxId);	// get the IDs

		if(thisGroupId != groupId) {        				// if the group ID doesn't match, skip
			continue;
		}

        if(c->isChecked()) {								// is checked and from the group?
			return checkboxId;
		}
	}
	
	return -1;
}

void ConfigStream::checkboxGroup_setCheckedId(int groupId, int checkedId)
{
    for(WORD i=0; i<screen.size(); i++) {					// go through the current screen and find the checked checkbox
		ConfigComponent *c = screen[i];
		
		int thisGroupId, checkboxId;
		c->getCheckboxGroupIds(thisGroupId, checkboxId);	// get the IDs

		if(thisGroupId != groupId) {        				// if the group ID doesn't match, skip
			continue;
		}

		if(checkboxId == checkedId) {						// for the matching id - check
			c->setIsChecked(true);
		} else {											// for mismatching id - uncheck
			c->setIsChecked(false);
		}
	}
}

void onCheckboxGroupEnter(int groupId, int checkboxId)
{
	ConfigStream::instance().checkboxGroup_setCheckedId(groupId, checkboxId);
}

void ConfigStream::screen_addHeaderAndFooter(std::vector<ConfigComponent *> &scr, char *screenName)
{
	ConfigComponent *comp;

	// insert header
	comp = new ConfigComponent(ConfigComponent::label, ">> CosmosEx config tool - Jookie 2013 <<", 40, 0, 0);
	comp->setReverse(true);
	scr.push_back(comp);

	// insert footer
	comp = new ConfigComponent(ConfigComponent::label, "        To quit - press Ctrl + C        ", 40, 0, 22);
	comp->setReverse(true);
	scr.push_back(comp);

	// insert screen name as part of header
	char bfr[41];
	memset(bfr, 32, 40);					// fill with spaces (0 - 39)
	bfr[40] = 0;							// terminate with a zero
	
	int len = strlen(screenName);
	int pos = (40 / 2) - (len / 2);			// calculate the position in the middle of screen
	strncpy(bfr + pos, screenName, len);	// copy the string in the middle, withouth the terminating zero
	
	comp = new ConfigComponent(ConfigComponent::label, bfr, 40, 0, 1);
	comp->setReverse(true);
	scr.push_back(comp);
}

//--------------------------
// screen creation methods
void ConfigStream::createScreen_homeScreen(void)
{
	// the following 3 lines should be at start of each createScreen_ method
	destroyCurrentScreen();				// destroy current components
	screenChanged		= true;			// mark that the screen has changed
	showingHomeScreen	= true;			// mark that we're showing the home screen
	
	screen_addHeaderAndFooter(screen, (char *) "Main menu");
	
	ConfigComponent *comp;
	
	comp = new ConfigComponent(ConfigComponent::button, " ACSI IDs config ",	18, 10,  6);
	comp->setOnEnterFunction(onMainMenu_acsiConfig);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::button, " Translated disks ",	18, 10,  8);
	comp->setOnEnterFunction(onMainMenu_translatedDisks);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::button, " Shared drive ",		18, 10, 10);
	comp->setOnEnterFunction(onMainMenu_sharedDrive);
	screen.push_back(comp);
	
	comp = new ConfigComponent(ConfigComponent::button, " Floppy config ",		18, 10, 12);
	comp->setOnEnterFunction(onMainMenu_floppyConfig);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::button, " Network settings ",	18, 10, 14);
	comp->setOnEnterFunction(onMainMenu_networkSettings);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::button, " Update software ",	18, 10, 16);
	comp->setOnEnterFunction(onMainMenu_updateSoftware);
	screen.push_back(comp);
	
	setFocusToFirstFocusable();
}

void onGoToHomeScreen(ConfigComponent *sender)
{
	ConfigStream::instance().createScreen_homeScreen();
}

void ConfigStream::createScreen_acsiConfig(void)
{
	// the following 3 lines should be at start of each createScreen_ method
	destroyCurrentScreen();				// destroy current components
	screenChanged		= true;			// mark that the screen has changed
	showingHomeScreen	= false;		// mark that we're NOT showing the home screen
	
	screen_addHeaderAndFooter(screen, (char *) "ACSI config");
	
	ConfigComponent *comp;

	comp = new ConfigComponent(ConfigComponent::label, "ID       off   raw  tran   net", 40, 0, 3);
	comp->setReverse(true);
	screen.push_back(comp);

	for(int row=0; row<8; row++) {			// now make 8 * 7 checkboxes
		char bfr[5];
		sprintf(bfr, "%d", row);
		
		comp = new ConfigComponent(ConfigComponent::label, bfr, 2, 1, row + 4);
		screen.push_back(comp);

		for(int col=0; col<4; col++) {
			comp = new ConfigComponent(ConfigComponent::checkbox, "   ", 3, 8 + (col * 6), row + 4);			// create and place checkbox on screen
			comp->setCheckboxGroupIds(row, col);																// set checkbox group id to row, and checbox id to col
			comp->setOnChBEnterFunction(onCheckboxGroupEnter);													// set this as general checkbox group handler, which switched checkboxed in group
			screen.push_back(comp);
		}
	}
	
	comp = new ConfigComponent(ConfigComponent::label, "off  - turned off, not responding here",	40, 0, 16);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::label, "raw  - raw sector access (use HDDr/ICD)",	40, 0, 17);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::label, "tran - translated access      (only one)",	40, 0, 18);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::label, "net  - network card interface (only one)",	40, 0, 19);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::button, "  Save  ", 8,  9, 13);
	comp->setOnEnterFunction(onAcsiConfig_save);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::button, " Cancel ", 8, 20, 13);
	comp->setOnEnterFunction(onGoToHomeScreen);
	screen.push_back(comp);
	
	Settings s;
	 
	char key[32];
	for(int id=0; id<8; id++) {							// read the list of device types from settings
		sprintf(key, "ACSI_DEVTYPE_%d", id);			// create settings KEY, e.g. ACSI_DEVTYPE_0
		int devType = s.getInt(key, DEVTYPE_OFF);		
		
		if(devType < 0) {
			devType = DEVTYPE_OFF;
		}
		
		checkboxGroup_setCheckedId(id, devType);		// set the checkboxes according to settings
	}
	
	setFocusToFirstFocusable();
}
