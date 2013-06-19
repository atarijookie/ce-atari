#include <stdio.h>
#include <string.h>

#include "configstream.h"

ConfigStream::ConfigStream(void)
{

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

void ConfigStream::getStream(char *bfr, int maxLen)
{
	memset(bfr, 0, maxLen);

	for(int i=0; i<screen.size(); i++) {
		ConfigComponent *c = screen[i];
		
		int gotLen;
		c->getStream(true, bfr, gotLen);
		bfr += gotLen;
	}
}

void ConfigStream::goToHomeScreen(void)
{
	ConfigComponent *comp;
	
	comp = new ConfigComponent(ConfigComponent::label, "Home Screen", 12, 0, 0);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::checkbox, "", 3, 0, 1);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::editline, "", 16, 0, 2);
	screen.push_back(comp);

	comp = new ConfigComponent(ConfigComponent::button, " OK ", 4, 0, 3);
	screen.push_back(comp);
}

