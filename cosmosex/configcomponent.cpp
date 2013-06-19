#include <stdio.h>
#include <string>
#include <string.h>

#include "configcomponent.h"

ConfigComponent::ConfigComponent(ComponentType type, std::string text, int maxLen, int x, int y)
{
	onEnter = NULL;
	cursorPos = 0;
	
	this->type	= type;

	hasFocus	= false;
	isReverse	= false;
	isChecked	= false;
	
	posX	= x;
	posY	= y;
	
	this->maxLen = maxLen;
	
	this->text = text;
	this->text.resize(maxLen, ' ');
	
	textLength = text.length();
	
	changed = true;							// mark that we got new data and we should display them
}

void ConfigComponent::getStream(bool fullNotChange, char *bfr, int &len)
{
	len = 0;
	char *bfrStart = bfr;

	if(!fullNotChange && !changed) {					// if we're displaying only a change and change didn't occur, quit
		return;
	}
	
	// now we're either displaying full component, or change occured
	changed = false;									// mark that we've displayed current state and that nothing changed, until it changes ;)
	
	if(type == label) {
		terminal_addGoto(bfr, posX, posY);			// goto(x,y)
		bfr += 4;
			
		if(isReverse) {								// if reversed, start reverse
			terminal_addReverse(bfr, true);
			bfr += 2;
		}
			
		strncpy(bfr, text.c_str(), text.length());	// copy the text
		bfr += maxLen;

		if(isReverse) {								// if reversed, stop reverse
			terminal_addReverse(bfr, false);
			bfr += 2;
		}
					
		len = bfr - bfrStart;						// we printed this much
		return;
	}

	//------
	if(type == button || type == editline || type == checkbox) {
		terminal_addGoto(bfr, posX, posY);				// goto(x,y)
		bfr += 4;
		
		if(hasFocus) {									// if has focus, start reverse
			terminal_addReverse(bfr, true);
			bfr += 2;
		}

		bfr[         0] = '[';
		bfr[maxLen + 1] = ']';
			
		strncpy(bfr+1, text.c_str(), text.length());	// copy the text
		bfr += maxLen + 2;								// +2 because of [ and ]
		
		if(hasFocus) {									// if has focus, stop reverse
			terminal_addReverse(bfr, false);
			bfr += 2;
		}

		len = bfr - bfrStart;							// we printed this much
		return;
	}
}

void ConfigComponent::setOnEnterFunction(TFonEnter *onEnter)
{
	this->onEnter = onEnter;
}

void ConfigComponent::setFocus(bool hasFocus)
{
	if(type == label) {							// limimt hasFocus to anything but label
		return;
	}

	if(this->hasFocus != hasFocus) {			// if data changed
		changed = true;							// mark that we got new data and we should display them
	}

	this->hasFocus = hasFocus;
}

void ConfigComponent::setReverse(bool isReverse)
{
	if(type != label) {							// limimt isReverse only to label
		return;
	}
	
	if(this->isReverse != isReverse) {			// if data changed
		changed = true;							// mark that we got new data and we should display them
	}

	this->isReverse = isReverse;
}

void ConfigComponent::setIsChecked(bool isChecked)
{
	if(type != checkbox) {						// limimt isChecked only to checkbox
		return;
	}

	if(this->isChecked != isChecked) {			// if data changed
		changed = true;							// mark that we got new data and we should display them
	}

	this->isChecked = isChecked;
	
	text.resize(maxLen, ' ');
	for(int i=0; i<maxLen; i++) {				// fill with spaces
		text[i] = ' ';
	}
	
	if(isChecked) {								// if is checked, put a star in the middle
		int pos = (maxLen / 2);					// calculate the position of '*' in string
		text[pos] = '*';
	}
}

/*
ST keys: 
enter     - key = 13
esc       - key = 27
space     - key = 32
backspace - key = 8
delete    - key = 127

home      - vkey = 71, key = 0
left      - vkey = 75, key = 0
right     - vkey = 77, key = 0
up        - vkey = 72, key = 0
down      - vkey = 80, key = 0
*/

void ConfigComponent::onKeyPressed(char vkey, char key)
{
	if(type == label) {							// do nothing on key pressed for label 
		return;
	}
	
	if(type == checkbox || type == button) {	// for checkbox and button
		if(key == 13 || key == 32) {			// when ENTER or SPACE pressed on checkbox
			setIsChecked(!isChecked);			// invert isChecked
			
			if(onEnter != NULL) {				// if we got onEnter function, call it
				(*onEnter)(this);
			}
			
			changed = true;						// mark that we got new data and we should display them
		}	
	}
	
	if(type == editline) {						// for editLine better use separate function
		changed = true;							// mark that we got new data and we should display them
		handleEditLineKeyPress(vkey, key);
	}
}

void ConfigComponent::handleEditLineKeyPress(char vkey, char key)
{
	if(key == 0) {					// handle special keys
		if(vkey == 75) {			// arrow left?
			if(cursorPos > 0) {
				cursorPos--;
			}
		}
		
		if(vkey == 77) {			// arrow right?
			if(cursorPos < textLength) {
				cursorPos++;
			}
		}

		if(vkey == 71) {			// home?
			cursorPos = 0;
		}
		
		return;
	}
	//-----
	// now for the other keys
	if(key == 8) {											// backspace?
		if(textLength > 0 && cursorPos > 0) {				// we got some text and we're not at the start of the line?
			text.erase(cursorPos-1, 1);					// delete char before cursor
			text.resize(maxLen, ' ');						// stretch string to maxLen
		
			cursorPos--;
			textLength--;
		}
		
		return;
	}
	
	if(key == 127) {										// delete?
		if(textLength > 0 && cursorPos < textLength) {		// we got some text and we're not at the end of the line?
			text.erase(cursorPos, 1);						// delete char at cursor
			text.resize(maxLen, ' ');						// stretch string to maxLen
		
			textLength--;
		}
		
		return;
	}
	
	//-------
	// now for the other chars - just add them
	if(cursorPos < textLength) {							// if should insert
		text.insert(cursorPos, 1, key);
	} else {												// if should append
		text.push_back(key);
	}
	
	textLength++;
	cursorPos++;

	if(textLength > maxLen) {								// if too long
		text.resize(maxLen, ' ');							// cut string to maxLen
		textLength = maxLen;
	}

	if(cursorPos >= maxLen) {								// if cursor too far
		cursorPos = maxLen -1;
	}
}

void ConfigComponent::setText(std::string text)
{
	if(this->text != text) {					// if data changed
		changed = true;							// mark that we got new data and we should display them
	}

	this->text = text;
	this->text.resize(maxLen, ' ');
	
	textLength = text.length();
}

bool ConfigComponent::isFocused(void)
{
	return hasFocus;
}

bool ConfigComponent::canFocus(void)
{
	return (type != label);						// if not label, then can focus
}

void ConfigComponent::terminal_addGoto(char *bfr, int x, int y)
{
	bfr[0] = 27;		
	bfr[1] = 'Y';
	bfr[2] = ' ' + x;
	bfr[3] = ' ' + y;
}

void ConfigComponent::terminal_addReverse(char *bfr, bool onNotOff)
{
	bfr[0] = 27;		
	
	if(onNotOff) {
		bfr[1] = 'p';
	} else {
		bfr[1] = 'q';
	}
}

void ConfigComponent::terminal_addGotoCurrentCursor(char *bfr, int &cnt)
{
	cnt = 0;
	
	if(type != editline) {			// if it's not editline, skip adding current cursor
		return;
	}
	
	if(!hasFocus) {					// if this editline doesn't have focus, skip adding current cursor
		return;
	}
	
	terminal_addGoto(bfr, posX + 1 + cursorPos, posY);		// add goto(x,y) at cursor position
	cnt = 4;
}

