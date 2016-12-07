#include <stdio.h>
#include <string>
#include <string.h>

#include "configcomponent.h"
#include "configstream.h"
#include "keys.h"

#define HEARTBEATSTATECHAR_COUNT    4
char heartBeatStateChar[HEARTBEATSTATECHAR_COUNT] = {'|', '/', '-', '\\'}; 

ConfigComponent::ConfigComponent(ConfigStream *parent, ComponentType type, std::string text, unsigned int maxLen, int x, int y, int gotoOffset)
{
    confStream = parent;

    onEnter     = 0;
    onChBEnter  = 0;

    cursorPos = 0;
    
    isLimitedShowSize   = false;
    showSize            = maxLen;           // number of characters that this editline will show (e.g. maxLen is 63 chars, but we want to show only 15 chars at any time, and scroll the rest
    showWindowStart     = 0;                // starting character where the shown window content is shown (e.g. when this is 20, maxLen is 63, showSize is 15, the component should show characters 20 to 35)

    heartBeatState  = 0;
    
    checkBoxGroup   = -1;
    checkBoxId      = -1;

    this->type  = type;

    hasFocus    = false;
    isReverse   = false;
    checked     = false;

    posX    = x;
    posY    = y;
    this->gotoOffset    = gotoOffset;
    this->maxLen        = maxLen;
    this->text          = text;

    if(text.length() > maxLen) {
        this->text.resize(maxLen);
    }

    changed = true;                         // mark that we got new data and we should display them

    componentId = -1;                       // no component id yet defined
    textOptions = TEXT_OPTION_ALLOW_ALL;    // no restrictions on chars
}

void ConfigComponent::getStream(bool fullNotChange, BYTE *bfr, int &len)
{
    len = 0;
    BYTE *bfrStart = bfr;

    if(!fullNotChange && !changed) {                // if we're displaying only a change and change didn't occur, quit
        return;
    }

    // now we're either displaying full component, or change occured
    changed = false;                                // mark that we've displayed current state and that nothing changed, until it changes ;)

    if(type == label) {
        bfr = terminal_addGoto(bfr, posX, posY);    // goto(x,y)

        if(isReverse) {                             // if reversed, start reverse
            bfr = terminal_addReverse(bfr, true);
        }

        for(int i=0; i<((int) maxLen); i++) {       // fill with spaces
            bfr[i] = ' ';
        }

        strncpy((char *) bfr, text.c_str(), text.length()); // copy the text
        bfr += maxLen;

        if(isReverse) {                             // if reversed, stop reverse
            bfr = terminal_addReverse(bfr, false);
        }

        len = bfr - bfrStart;                       // we printed this much
        return;
    }

    //------
    if(type == button || type == editline || type == editline_pass || type == checkbox) {
        bfr = terminal_addGoto(bfr, posX, posY);        // goto(x,y)

        if(hasFocus) {                                  // if has focus, start reverse
            bfr = terminal_addReverse(bfr, true);
        }

        bfr[           0] = '[';
        bfr[showSize + 1] = ']';

        for(int i=0; i<((int) showSize); i++) {         // pre-fill with spaces
            bfr[i+1] = ' ';
        }

        std::string displayText = text;                 // this text will be displayed

        if(type == editline_pass) {                     // if this is a password editline, then show just asterisks instead of real chars 
            displayText = std::string(text.length(), '*');
        }

        if(showSize == maxLen) {                        // if full text is shown
            strncpy((char *) bfr+1, displayText.c_str(), displayText.length()); // copy the text
            bfr += maxLen + 2;                                                  // +2 because of [ and ]
        } else {                                        // if partial text is shown
            const char *pText   = displayText.c_str();
            size_t textLen      = displayText.length();
            
            if(textLen > showWindowStart) {             // if the text is long enough to show after showing only part (starting at showWindowStart), show it
                pText   += showWindowStart;
                textLen -= showWindowStart;
                
                if(textLen > showSize) {                // if the text still would be long, then shorten it
                    textLen = showSize;
                }

                strncpy((char *) bfr+1, pText, textLen);    // copy the text
            }
        
            bfr += showSize + 2;                                                // +2 because of [ and ]
        }
        

        if(hasFocus) {                                  // if has focus, stop reverse
            bfr = terminal_addReverse(bfr, false);
        }

        len = bfr - bfrStart;                           // we printed this much
        return;
    }
    
    //-----
    // for heartbeat
    if(type == heartBeat) {
        changed = true;                                 // this heartbeat always changes

        bfr = terminal_addGoto(bfr, posX, posY);        // goto(x,y)
        
        if(isReverse) {                                 // if reversed, start reverse
            bfr = terminal_addReverse(bfr, true);
        }

        bfr[0] = heartBeatStateChar[heartBeatState];    // store heartbeat char
        bfr++;
        
        heartBeatState++;                               // move to next heartbeat state
        if(heartBeatState >= HEARTBEATSTATECHAR_COUNT) {
            heartBeatState = 0;
        }
        
        if(isReverse) {                                 // if reversed, stop reverse
            bfr = terminal_addReverse(bfr, false);
        }
        
        len = bfr - bfrStart;                           // we printed this much
        return;
    }
}

void ConfigComponent::setOnEnterFunctionCode(int onEnter)
{
    this->onEnter = onEnter;
}

void ConfigComponent::setOnChBEnterFunctionCode(int onChBEnter)
{
    this->onChBEnter = onChBEnter;
}

void ConfigComponent::setCheckboxGroupIds(int groupId, int checkboxId)
{
    checkBoxGroup   = groupId;
    checkBoxId      = checkboxId;
}

void ConfigComponent::getCheckboxGroupIds(int& groupId, int& checkboxId)
{
    groupId     = checkBoxGroup;
    checkboxId  = checkBoxId;
}

void ConfigComponent::setFocus(bool hasFocus)
{
    if(type == label || type == heartBeat) {    // can't focus on label and heartBeat
        return;
    }

    if(this->hasFocus != hasFocus) {            // if data changed
        changed = true;                         // mark that we got new data and we should display them
    }

    this->hasFocus = hasFocus;
}

void ConfigComponent::setReverse(bool isReverse)
{
    if(type != label && type != heartBeat) {    // reverse works only on label and heartBeat
        return;
    }

    if(this->isReverse != isReverse) {          // if data changed
        changed = true;                         // mark that we got new data and we should display them
    }

    this->isReverse = isReverse;
}

void ConfigComponent::setIsChecked(bool isChecked)
{
    if(type != checkbox) {                      // limimt isChecked only to checkbox
        return;
    }

    if(checked != isChecked) {                  // if data changed
        changed = true;                         // mark that we got new data and we should display them
    }

    checked = isChecked;

    text.resize(maxLen);
    for(int i=0; i<((int) maxLen); i++) {       // fill with spaces
        text[i] = ' ';
    }

    if(checked) {                               // if is checked, put a star in the middle
        int pos = (maxLen / 2);                 // calculate the position of '*' in string
        text[pos] = '*';
    }
}

bool ConfigComponent::isChecked(void)
{
    if(type != checkbox) {                      // for other types - not checked
        return false;
    }

    return checked;
}

void ConfigComponent::onKeyPressed(BYTE key)
{
    if(type == label || type == heartBeat) {    // do nothing on key pressed for label and heartBeat
        return;
    }

    if(type == checkbox || type == button) {    // for checkbox and button
        if(key == KEY_ENTER || key == 32) {         // when ENTER or SPACE pressed on checkbox

            if(!isGroupCheckBox()) {            // for non group checkbox - just invert
                setIsChecked(!checked);         // invert isChecked
            } else {                            // for group checkbox - special handling
                confStream->onCheckboxGroupEnter(checkBoxGroup, checkBoxId);
            }

            if(onEnter != 0) {                      // if we got onEnter function, call it
                confStream->enterKeyHandlerLater(onEnter);
            }

            changed = true;                         // mark that we got new data and we should display them
        }
    }

    if(type == editline || type == editline_pass) { // for editLine better use separate function
        changed = true;                             // mark that we got new data and we should display them
        handleEditLineKeyPress(key);
        updateShownWindowPositionAccordingToCursor();   // update shown window position based on cursor position
    }
}

void ConfigComponent::handleEditLineKeyPress(BYTE key)
{
    if(key == KEY_LEFT) {           // arrow left?
        if(cursorPos > 0) {
            cursorPos--;
        }
        return;
    }

    if(key == KEY_RIGHT) {          // arrow right?
        if(cursorPos < text.length()) {
            cursorPos++;
        }
        return;
    }

    if(key == KEY_HOME) {           // home?
        cursorPos = 0;
        return;
    }

    if(key == KEY_END) {            // END pressed? (not present on Atari keyboard)
        cursorPos = text.length();
        return;
    }

    //-----
    // now for the other keys
    if(key == KEY_BACKSP) {                                 // backspace?
        if(text.length() > 0) {

            if(cursorPos < text.length()) {                 // cursor IN text
                if(cursorPos > 0) {                         // and we're not at the start of the line
                    text.erase(cursorPos - 1, 1);
                    cursorPos--;
                }
            } else {                                        // cursor BEHIND text
                text.resize(text.length() - 1);             // just remove the last char
                cursorPos--;
            }
        }
        return;
    }

    if(key == KEY_DELETE) {                                     // delete?
        if(text.length() > 0 && cursorPos < text.length()) {    // we got some text and we're not at the end of the line?
            text.erase(cursorPos, 1);                           // delete char at cursor
        }
        return;
    }

    //-------
    // now for the other chars - add them after filtering
    key = filterTextKey(key);

    if(key == 0) {          // the key was filtered out??? quit
        return;
    }

    if(cursorPos < text.length()) {                         // cursor IN text
        text.insert(cursorPos, 1, key);                     // insert somewhere in the middle
    } else {                                                // cursor BEHIND text
        text.push_back(key);                                // insert char at the end
    }

    cursorPos++;

    if(text.length() > maxLen) {                            // if too long
        text.resize(maxLen);                                // cut string to maxLen
    }

    if(cursorPos >= maxLen) {                               // if cursor too far
        cursorPos = maxLen -1;
    }
}

void ConfigComponent::updateShownWindowPositionAccordingToCursor(void)
{
    if(cursorPos < showWindowStart) {               // if cursor would go LEFT out of shown window, move shown window start to left
        showWindowStart--;
    }

    if(cursorPos > (showWindowStart + showSize)) {  // if cursor would go RIGHT out of shown window, move shown window start to right
        showWindowStart++;
    }

    if(cursorPos == 0) {                            // cursor on start of line? shown window start to start of line
        showWindowStart = 0;
    }

    if(cursorPos == text.length()) {                // cursor on the end of line? show window start to (end of line - showSize)
        if(text.length() > showSize) {              // text is longer than what we can show? Show just last part.
            showWindowStart = text.length() - showSize;
        } else {                                    // text is shorter than what we can show? Show it all.
            showWindowStart = 0;
        }
    }
}

void ConfigComponent::setTextOptions(int newOpts)
{
    textOptions = newOpts;
}

// set how many characters we can see at one time
void ConfigComponent::setLimitedShowSize(unsigned int newShowSize)
{
    showSize            = newShowSize;      // number of characters that this editline will show (e.g. maxLen is 63 chars, but we want to show only 15 chars at any time, and scroll the rest
    showWindowStart     = 0;                // starting character where the shown window content is shown (e.g. when this is 20, maxLen is 63, showSize is 15, the component should show characters 20 to 35)
    isLimitedShowSize   = true;
}

BYTE ConfigComponent::filterTextKey(BYTE key)
{
    // if any of these controll keys were pressed, filter them out - they are not a valid TEXT keys
    if( (key >= KEY_UP      && key <= KEY_RIGHT)    ||
        (key >= KEY_ENTER   && key <= KEY_UNDO)     ||
        (key >= KEY_F1      && key <= KEY_F10) ) {
        return 0;
    }

    // if the dot is allowed (for IP addresses)
    if(key == '.' && textOptionSet(TEXT_OPTION_ALLOW_DOT)) {
        return key;
    }

    // if it's a letter and we have it enabled
    if(isLetter(key) && textOptionSet(TEXT_OPTION_ALLOW_LETTERS)) {
        // if we should allow only uppercase letters and it's a lower case letter, convert it
        if(textOptionSet(TEXT_OPTION_LETTERS_ONLY_UPPERCASE) && isSmallLetter(key)) {
            key = key - 32;
        }

        return key;
    }

    // if numbers are enabled, let it pass
    if(isNumber(key) && textOptionSet(TEXT_OPTION_ALLOW_NUMBERS)) {
        return key;
    }

    // if it's something other and we have it enabled, pass
    if(isOther(key) && textOptionSet(TEXT_OPTION_ALLOW_OTHER)) {
        return key;
    }

    return 0;
}

bool ConfigComponent::textOptionSet(WORD textOption)
{
    if((textOptions & textOption) == textOption) {
        return true;
    }

    return false;
}

bool ConfigComponent::isLetter(BYTE key)
{
    if(key >= 'A' && key <= 'Z') {
        return true;
    }

    if(key >= 'a' && key <= 'z') {
        return true;
    }

    return false;
}

bool ConfigComponent::isSmallLetter(BYTE key)
{
    if(key >= 'a' && key <= 'z') {
        return true;
    }

    return false;
}


bool ConfigComponent::isOther(BYTE key)
{
    if(isLetter(key)) {
        return false;
    }

    if(isNumber(key)) {
        return false;
    }

    return true;
}

bool ConfigComponent::isNumber(BYTE key)
{
    if(key >= '0' && key <= '9') {
        return true;
    }

    return false;
}

void ConfigComponent::setText(std::string text)
{
    if(this->text != text) {                    // if data changed
        changed = true;                         // mark that we got new data and we should display them
    }

    this->text = text;

    if(text.length() > maxLen) {
        this->text.resize(maxLen);
    }
}

void ConfigComponent::getText(std::string &text)
{
    text = this->text;
}

bool ConfigComponent::isFocused(void)
{
    return hasFocus;
}

bool ConfigComponent::canFocus(void)
{
    return (type != label && type != heartBeat);        // can focus if not label and not hearBeat
}

BYTE *ConfigComponent::terminal_addGoto(BYTE *bfr, int x, int y)
{
    bfr[0] = 27;
    bfr[1] = 'Y';
    bfr[2] = ' ' + y;
    bfr[3] = ' ' + x + gotoOffset;
    
    return (bfr + 4);
}

BYTE *ConfigComponent::terminal_addReverse(BYTE *bfr, bool onNotOff)
{
    bfr[0] = 27;

    if(onNotOff) {
        bfr[1] = 'p';
    } else {
        bfr[1] = 'q';
    }
    
    return (bfr + 2);
}

BYTE *ConfigComponent::terminal_addCursorOn(BYTE *bfr, bool on)
{
    bfr[0] = 27;

    if(on) {
        bfr[1] = 'e';       // CUR_ON
    } else {
        bfr[1] = 'f';       // CUR_OFF
    }
    
    return (bfr + 2);
}

BYTE *ConfigComponent::terminal_addGotoCurrentCursor(BYTE *bfr, int &cnt)
{
    cnt = 0;

    if(type != editline && type != editline_pass) { // if it's not editline, skip adding current cursor
        bfr = terminal_addCursorOn(bfr, false);
        cnt += 2;
        return bfr;
    }

    if(!hasFocus) {                                 // if this editline doesn't have focus, skip adding current cursor
        bfr = terminal_addCursorOn(bfr, false);
        cnt += 2;
        return bfr;
    }

    bfr = terminal_addCursorOn(bfr, true);
    cnt += 2;

    int gotoCursorPos;
    if(isLimitedShowSize) {                         // shown only limited text? calculate the on-screen cursor position
        gotoCursorPos = cursorPos - showWindowStart;
    } else {                                        // shown whole text? just go to cursor position
        gotoCursorPos = cursorPos;
    }
    
    bfr = terminal_addGoto(bfr, posX + 1 + gotoCursorPos, posY);      // add goto(x,y) at cursor position
    cnt += 4;
    
    return bfr;
}

bool ConfigComponent::isGroupCheckBox(void)
{
    return (checkBoxGroup != -1);       // if the group ID is not -1, then it's a group checkbox
}

void ConfigComponent::setComponentId(int newId)
{
    componentId = newId;
}

int  ConfigComponent::getComponentId(void)
{
    return componentId;
}

int ConfigComponent::getComponentType(void)
{
    return type;
}

