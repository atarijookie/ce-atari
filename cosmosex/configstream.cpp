#include <stdio.h>
#include <string.h>

#include "configstream.h"

ConfigStream::ConfigStream()
{
	showingHomeScreen	= false;
	screenChanged		= true;
	
	createScreen_homeScreen();
}

ConfigStream::~ConfigStream()
{
	destroyCurrentScreen();
}

void ConfigStream::onKeyDown(char vkey, char key)
{
	int focused = -1, firstFocusable = -1, lastFocusable = -1;
	ConfigComponent *c;
	
	// go through the current screen and find focused component, also first focusable component
	for(int i=0; i<screen.size(); i++) {			
		c = screen[i];
		
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
	
	c = screen[focused];							// focus this component
	c->setFocus(true);

	int prevFocusable = -1, nextFocusable = -1;		// now find previous and next focusable item in the list of components
	for(int i=0; i<screen.size(); i++) {			
		c = screen[i];
		
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
			c->setFocus(false);						// unfocus this component
			
			if(prevFocusable != -1) {				// got previous focusable item?
				c = screen[prevFocusable];			// move to the previous component	
			} else if(lastFocusable != -1) {		// got last focusable? 
				c = screen[lastFocusable];			// move to the last component (wrap around)
			}
			
			c->setFocus(true);						// focus this component
			
			return;
		}
		
		if(vkey == 80) {							// arrow down

			c->setFocus(false);						// unfocus this component
			
			if(nextFocusable != -1) {				// got next focusable item?
				c = screen[nextFocusable];			// move to the next component	
			} else if(firstFocusable != -1) {		// got first focusable? 
				c = screen[firstFocusable];			// move to the first component (wrap around)
			}
			
			c->setFocus(true);						// focus this component
		
			return;
		}
	}

	// if it got here, we didn't handle it, let the component handle it
	c->onKeyPressed(vkey, key);
}

void ConfigStream::getStream(bool homeScreen, char *bfr, int maxLen)
{
	int totalCnt = 0;

	if(homeScreen) {									// if we should show the stream for homescreen
		if(!showingHomeScreen) {						// and we're not showing it yet
			createScreen_homeScreen();					// create homescreen
		}
	}
	
	if(screen.size() == 0) {							// if we wanted to show current screen, but there is nothing, just show home screen
		createScreen_homeScreen();
	}

	memset(bfr, 0, maxLen);								// clear the buffer

	if(screenChanged) {									// if screen changed, clear screen (CLEAR_HOME) and draw it all
		bfr[0] = 27;		
		bfr[1] = 'E';
		
		bfr += 2;
		totalCnt += 2;
	}

	int focused = -1;
	
	for(int i=0; i<screen.size(); i++) {				// go through all the components of screen and gather their streams
		ConfigComponent *c = screen[i];
		
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
		ConfigComponent *c = screen[focused];
		c->terminal_addGotoCurrentCursor(bfr, gotLen);	// position the cursor at the right place

		bfr			+= gotLen;
		totalCnt	+= gotLen;
	}
	
	screenChanged = false;
}

void ConfigStream::createScreen_homeScreen(void)
{
	// the following 2 lines should be at start of each createScreen_ method
	destroyCurrentScreen();				// destroy current components
	screenChanged = true;				// mark that the screen has changed

	showingHomeScreen = true;
	
	ConfigComponent *comp;
	
	comp = new ConfigComponent(ConfigComponent::label, "Home Screen", 12, 0, 0);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::checkbox, "", 3, 0, 1);
	screen.push_back(comp);

   	comp = new ConfigComponent(ConfigComponent::checkbox, "", 3, 0, 1);
   	comp->setCheckboxGroupIds(1,1);
	comp->setOnChBEnterFunction(onCheckboxGroupEnter);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::checkbox, "", 3, 0, 1);
   	comp->setCheckboxGroupIds(1,2);
	comp->setOnChBEnterFunction(onCheckboxGroupEnter);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::editline, "", 16, 0, 2);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::button, " OK ", 4, 0, 3);
	screen.push_back(comp);
}

void ConfigStream::destroyCurrentScreen(void)
{
	for(int i=0; i<screen.size(); i++) {			// go through the current screen, delete all components
		ConfigComponent *c = screen[i];
		delete c;
	}
	
	screen.clear();									// now clear the list
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
