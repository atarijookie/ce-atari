#include <stdio.h>
#include <string.h>

#include "configstream.h"

#include "configscreen_main.h"


ConfigStream::ConfigStream()
{
	showingHomeScreen	= false;
	showingMessage		= false;
	screenChanged		= true;
	
	message.clear();
	createScreen_homeScreen();
}

ConfigStream::~ConfigStream()
{
	destroyCurrentScreen();
	destroyScreen(message);
}

void ConfigStream::onKeyDown(char vkey, char key)
{
	std::vector<ConfigComponent *> &scr = showingMessage ? message : screen;		// if we should show message, set reference to message, otherwise set reference to screen

	int focused = -1, firstFocusable = -1, lastFocusable = -1;

	// go through the current screen and find focused component, also first focusable component
	for(int i=0; i<scr.size(); i++) {			
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
	for(int i=0; i<scr.size(); i++) {			
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
	
	if(key == 0) {									// if it's some non-char key
		if(vkey == 72) {							// arrow up
			curr->setFocus(false);					// unfocus this component
			
			if(prevFocusable != -1) {				// got previous focusable item?
				curr = scr[prevFocusable];		// move to the previous component	
			} else if(lastFocusable != -1) {		// got last focusable? 
				curr = scr[lastFocusable];		// move to the last component (wrap around)
			}
			
			curr->setFocus(true);					// focus this component
			
			return;
		}
		
		if(vkey == 80) {							// arrow down
			curr->setFocus(false);					// unfocus this component
			
			if(nextFocusable != -1) {				// got next focusable item?
				curr = scr[nextFocusable];		// move to the next component	
			} else if(firstFocusable != -1) {		// got first focusable? 
				curr = scr[firstFocusable];		// move to the first component (wrap around)
			} 
			
			curr->setFocus(true);					// focus this component
		
			return;
		}
	}

	// if it got here, we didn't handle it, let the component handle it
	curr->onKeyPressed(vkey, key);
}

void ConfigStream::getStream(bool homeScreen, char *bfr, int maxLen)
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
	
	for(int i=0; i<scr.size(); i++) {				// go through all the components of screen and gather their streams
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

	comp = new ConfigComponent(ConfigComponent::label, msgTxt,	40, 0, 10);
	message.push_back(comp);
	
	comp = new ConfigComponent(ConfigComponent::button, " OK ",	4, 17, 20);
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
	for(int i=0; i<scr.size(); i++) {				// go through this screen, delete all components
		ConfigComponent *c = scr[i];
		delete c;
	}
	
	scr.clear();									// now clear the list
}


void ConfigStream::setFocusToFirstFocusable(void)
{
	for(int i=0; i<screen.size(); i++) {			// go through the current screen
		ConfigComponent *c = screen[i];

		if(c->canFocus()) {
			c->setFocus(true);
			return;
		}
	}
}

int ConfigStream::checkboxGroup_getCheckedId(int groupId) 
{
	for(int i=0; i<screen.size(); i++) {					// go through the current screen and find the checked checkbox
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
	for(int i=0; i<screen.size(); i++) {					// go through the current screen and find the checked checkbox
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
	
	comp = new ConfigComponent(ConfigComponent::button, " ACSI config ",		18, 10,  7);
	comp->setOnEnterFunction(onMainMenu_acsiConfig);
	screen.push_back(comp);
	
	comp = new ConfigComponent(ConfigComponent::button, " Floppy config ",		18, 10,  9);
	comp->setOnEnterFunction(onMainMenu_floppyConfig);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::button, " Network settings ",	18, 10, 11);
	comp->setOnEnterFunction(onMainMenu_networkSettings);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::button, " Shared drive ",		18, 10, 13);
	comp->setOnEnterFunction(onMainMenu_sharedDrive);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::button, " Update software ",	18, 10, 15);
	comp->setOnEnterFunction(onMainMenu_updateSoftware);
	screen.push_back(comp);
	
	setFocusToFirstFocusable();
}
