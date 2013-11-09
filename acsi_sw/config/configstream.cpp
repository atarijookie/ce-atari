#include <stdio.h>
#include <string.h>

#include "../global.h"
#include "../native/scsi_defs.h"
#include "../acsidatatrans.h"

#include "settings.h"
#include "keys.h"
#include "configstream.h"

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

    if(curr->isGroupCheckBox()) {                           // for group check boxes
        int groupid, chbid;
        curr->getCheckboxGroupIds(groupid, chbid);          // ge the checkbox group IDs

        if(focusNextCheckboxGroup(key, groupid, chbid)) {   // and try to move there
            curr->setFocus(false);                          // and unfocus the previous one
            return;
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

    if(key == KEY_ESC) {                        // esc as cancel
        enterKeyHandler(CS_GO_HOME);
        return;
    }

    if(key == KEY_LEFT || key == KEY_RIGHT || key == KEY_TAB) {     // in case of left, right, tab on SAVE and CANCEL
        if(curr->getComponentId() == COMPID_BTN_SAVE) {
            curr->setFocus(false);                                  // unfocus this component
            focusByComponentId(COMPID_BTN_CANCEL);
        }

        if(curr->getComponentId() == COMPID_BTN_CANCEL) {
            curr->setFocus(false);                                  // unfocus this component
            focusByComponentId(COMPID_BTN_SAVE);
        }
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

        screenChanged = true;                                           // mark that the screen has changed
    }

    if(screen.size() == 0) {							// if we wanted to show current screen, but there is nothing, just show home screen
        createScreen_homeScreen();
    }

    memset(bfr, 0, maxLen);								// clear the buffer

    std::vector<ConfigComponent *> &scr = showingMessage ? message : screen;		// if we should show message, set reference to message, otherwise set reference to screen

    // first turn off the cursor to avoid cursor blinking on the screen
    bfr[0] = 27;
    bfr[1] = 'f';       // CUR_OFF
    bfr += 2;
    totalCnt += 2;

    if(screenChanged) {									// if screen changed, clear screen (CLEAR_HOME) and draw it all
        bfr[0] = 27;
        bfr[1] = 'E';   // CLEAR_HOME

        bfr += 2;
        totalCnt += 2;
    }

    //--------
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

        bfr         += gotLen;
        totalCnt    += gotLen;
    }

    screenChanged = false;

    return totalCnt;                                    // return the count of bytes used
}

void ConfigStream::showMessageScreen(char *msgTitle, char *msgTxt)
{
    screenChanged = true;

    showingMessage = true;
    destroyScreen(message);

    screen_addHeaderAndFooter(message, msgTitle);

    ConfigComponent *comp;

    comp = new ConfigComponent(this, ConfigComponent::label, msgTxt, 240, 0, 10);
    message.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " OK ", 4, 17, 20);
    comp->setOnEnterFunctionCode(CS_HIDE_MSG_SCREEN);
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

ConfigComponent *ConfigStream::findComponentById(int compId)
{
    for(WORD i=0; i<screen.size(); i++) {			// go through the current screen
        ConfigComponent *c = screen[i];

        if(c->getComponentId() == compId) {                     // found the component?
            return c;
        }
    }

    return NULL;
}

bool ConfigStream::getTextByComponentId(int componentId, std::string &text)
{
    ConfigComponent *c = findComponentById(componentId);

    if(c == NULL) {
        return false;
    }

    c->getText(text);
    return true;
}

void ConfigStream::setTextByComponentId(int componentId, std::string &text)
{
    ConfigComponent *c = findComponentById(componentId);

    if(c == NULL) {
        return;
    }

    c->setText(text);
}

bool ConfigStream::getBoolByComponentId(int componentId, bool &val)
{
    ConfigComponent *c = findComponentById(componentId);

    if(c == NULL) {
        return false;
    }

    val = c->isChecked();

    return true;
}

void ConfigStream::setBoolByComponentId(int componentId, bool &val)
{
    ConfigComponent *c = findComponentById(componentId);

    if(c == NULL) {
        return;
    }

    c->setIsChecked(val);
}

void ConfigStream::focusByComponentId(int componentId)
{
    ConfigComponent *c = findComponentById(componentId);

    if(c == NULL) {
        return;
    }

    c->setFocus(true);
}

bool ConfigStream::focusNextCheckboxGroup(BYTE key, int groupid, int chbid)
{
    for(WORD i=0; i<screen.size(); i++) {			// go through the current screen
        ConfigComponent *c = screen[i];

        if(c->isGroupCheckBox()) {
            int groupid2, chbid2;

            c->getCheckboxGroupIds(groupid2, chbid2);

            // on key UP find groupid which is smaller by 1
            if(key == KEY_UP && groupid == (groupid2 + 1) && chbid == chbid2) {
                c->setFocus(true);
                return true;
            }

            // on key DOWN find groupid which is greater by 1
            if(key == KEY_DOWN && groupid == (groupid2 - 1) && chbid == chbid2) {
                c->setFocus(true);
                return true;
            }

            // on key LEFT just find smaller chbid
            if(key == KEY_LEFT && groupid == groupid2 && chbid == (chbid2 + 1)) {
                c->setFocus(true);
                return true;
            }

            // on key RIGHT just find greater chbid
            if(key == KEY_RIGHT && groupid == groupid2 && chbid == (chbid2 - 1)) {
                c->setFocus(true);
                return true;
            }
        }
    }

    return false;
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

void ConfigStream::onCheckboxGroupEnter(int groupId, int checkboxId)
{
    checkboxGroup_setCheckedId(groupId, checkboxId);
}

void ConfigStream::screen_addHeaderAndFooter(std::vector<ConfigComponent *> &scr, char *screenName)
{
    ConfigComponent *comp;

    // insert header
    comp = new ConfigComponent(this, ConfigComponent::label, ">> CosmosEx config tool - Jookie 2013 <<", 40, 0, 0);
    comp->setReverse(true);
    scr.push_back(comp);

    // insert footer
    comp = new ConfigComponent(this, ConfigComponent::label, "          To quit - press F10           ", 40, 0, 22);
    comp->setReverse(true);
    scr.push_back(comp);

    // insert screen name as part of header
    char bfr[41];
    memset(bfr, 32, 40);					// fill with spaces (0 - 39)
    bfr[40] = 0;							// terminate with a zero

    int len = strlen(screenName);
    int pos = (40 / 2) - (len / 2);			// calculate the position in the middle of screen
    strncpy(bfr + pos, screenName, len);	// copy the string in the middle, withouth the terminating zero

    comp = new ConfigComponent(this, ConfigComponent::label, bfr, 40, 0, 1);
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

    comp = new ConfigComponent(this, ConfigComponent::button, " ACSI IDs config ",	18, 10,  6);
    comp->setOnEnterFunctionCode(CS_CREATE_ACSI);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Translated disks ",	18, 10,  8);
    comp->setOnEnterFunctionCode(CS_CREATE_TRANSLATED);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Shared drive ",		18, 10, 10);
    comp->setOnEnterFunctionCode(CS_CREATE_SHARED);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Floppy config ",		18, 10, 12);
    comp->setOnEnterFunctionCode(CS_CREATE_FLOPPY);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Network settings ",	18, 10, 14);
    comp->setOnEnterFunctionCode(CS_CREATE_NETWORK);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Update software ",	18, 10, 16);
    comp->setOnEnterFunctionCode(CS_CREATE_UPDATE);
    screen.push_back(comp);

    setFocusToFirstFocusable();
}

void ConfigStream::createScreen_acsiConfig(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();				// destroy current components
    screenChanged		= true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, (char *) "ACSI config");

    ConfigComponent *comp;

    comp = new ConfigComponent(this, ConfigComponent::label, "ID             off   raw  tran", 40, 0, 3);
    comp->setReverse(true);
    screen.push_back(comp);

    for(int row=0; row<8; row++) {			// now make 8 rows of checkboxes
        char bfr[5];
        sprintf(bfr, "%d", row);

        comp = new ConfigComponent(this, ConfigComponent::label, bfr, 2, 1, row + 4);
        screen.push_back(comp);

        for(int col=0; col<3; col++) {
            comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ", 3, 14 + (col * 6), row + 4);			// create and place checkbox on screen
            comp->setCheckboxGroupIds(row, col);																// set checkbox group id to row, and checbox id to col
            screen.push_back(comp);
        }
    }

    comp = new ConfigComponent(this, ConfigComponent::label, "off  - turned off, not responding here",	40, 0, 17);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "raw  - raw sector access (use HDDr/ICD)",	40, 0, 18);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "tran - translated access      (only one)",	40, 0, 19);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ", 8,  9, 13);
    comp->setOnEnterFunctionCode(CS_SAVE_ACSI);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, 20, 13);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
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

void ConfigStream::createScreen_network(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();				// destroy current components
    screenChanged	= true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, (char *) "Network settings");

    ConfigComponent *comp;

    comp = new ConfigComponent(this, ConfigComponent::label, "Use DHCP",	10, 12, 6);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, " ",	1, 22, 6);
    comp->setComponentId(COMPID_NET_DHCP);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "If not using DHCP, set the next params:", 40, 0, 9);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "IP address", 40, 0, 11);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ", 15, 16, 11);
    comp->setComponentId(COMPID_NET_IP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Mask", 40, 0, 12);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ", 15, 16, 12);
    comp->setComponentId(COMPID_NET_MASK);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Gateway", 40, 0, 13);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ", 15, 16, 13);
    comp->setComponentId(COMPID_NET_GATEWAY);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "DNS", 40, 0, 14);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ", 15, 16, 14);
    comp->setComponentId(COMPID_NET_DNS);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ", 8,  9, 16);
    comp->setOnEnterFunctionCode(CS_SAVE_NETWORK);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, 20, 16);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);


    // get the settings, store them to components
    Settings s;
    std::string str;
    bool val;

    val = s.getBool((char *) "NET_USE_DHCP", true);
    setBoolByComponentId(COMPID_NET_DHCP, val);

    str = s.getString((char *) "NET_IP", (char *) "");
    setTextByComponentId(COMPID_NET_IP, str);

    str = s.getString((char *) "NET_MASK", (char *) "");
    setTextByComponentId(COMPID_NET_MASK, str);

    str = s.getString((char *) "NET_DNS", (char *) "");
    setTextByComponentId(COMPID_NET_DNS, str);

    str = s.getString((char *) "NET_GATEWAY", (char *) "");
    setTextByComponentId(COMPID_NET_GATEWAY, str);

    setFocusToFirstFocusable();
}

void ConfigStream::createScreen_translated(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();				// destroy current components
    screenChanged		= true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, (char *) "Translated disk");

    ConfigComponent *comp;

    comp = new ConfigComponent(this, ConfigComponent::label, "        Drive letters assignment", 40, 0, 5);
    comp->setReverse(true);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "First translated drive", 23, 6, 7);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",	1, 33, 7);
    comp->setComponentId(COMPID_TRAN_FIRST);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Shared drive", 23, 6, 9);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",	1, 33, 9);
    comp->setComponentId(COMPID_TRAN_SHARED);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Config drive", 23, 6, 10);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",	1, 33, 10);
    comp->setComponentId(COMPID_TRAN_CONFDRIVE);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "If you use also raw disks (Atari native ", 40, 0, 17);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "disks), you should avoid using few", 40, 0, 18);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "letters from C: to leave some space for", 40, 0, 19);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "them.",	40, 0, 20);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ", 8,  9, 13);
    comp->setOnEnterFunctionCode(CS_SAVE_TRANSLATED);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, 20, 13);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    // get the letters from settings, store them to components
    Settings s;
    char drive1, drive2, drive3;

    drive1 = s.getChar((char *) "DRIVELETTER_FIRST",      0);
    drive2 = s.getChar((char *) "DRIVELETTER_SHARED",     0);
    drive3 = s.getChar((char *) "DRIVELETTER_CONFDRIVE",  0);

    char driveStr[2] = {0, 0};
    std::string driveString;

    driveStr[0] = drive1;
    driveString = driveStr;
    setTextByComponentId(COMPID_TRAN_FIRST,     driveString);

    driveStr[0] = drive2;
    driveString = driveStr;
    setTextByComponentId(COMPID_TRAN_SHARED,    driveString);

    driveStr[0] = drive3;
    driveString = driveStr;
    setTextByComponentId(COMPID_TRAN_CONFDRIVE, driveString);

    setFocusToFirstFocusable();
}

void ConfigStream::enterKeyHandler(int event)
{
    switch(event) {
    case CS_GO_HOME:            createScreen_homeScreen();  break;
    case CS_CREATE_ACSI:        createScreen_acsiConfig();  break;
    case CS_CREATE_TRANSLATED:  createScreen_translated();  break;
    case CS_CREATE_SHARED:      break;
    case CS_CREATE_FLOPPY:      break;
    case CS_CREATE_NETWORK:     createScreen_network();     break;
    case CS_CREATE_UPDATE:      break;

    case CS_HIDE_MSG_SCREEN:    hideMessageScreen();        break;

    case CS_SAVE_ACSI:          onAcsiConfig_save();        break;
    case CS_SAVE_TRANSLATED:    onTranslated_save();        break;
    case CS_SAVE_NETWORK:       onNetwork_save();           break;
    }
}

void ConfigStream::onAcsiConfig_save(void)
{
    int devTypes[8];

    bool somethingActive = false;
    bool somethingInvalid = false;
    int tranCnt = 0;

    for(int id=0; id<8; id++) {								// get all selected types from checkbox groups
        devTypes[id] = checkboxGroup_getCheckedId(id);

        if(devTypes[id] != DEVTYPE_OFF) {					// if found something which is not OFF
            somethingActive = true;
        }

        switch(devTypes[id]) {								// count the shared drives, network adapters, config drives
        case DEVTYPE_TRANSLATED:	tranCnt++;					break;
        case -1:					somethingInvalid = true;	break;
        }
    }

    if(somethingInvalid) {									// if everything is set to OFF
        showMessageScreen((char *) "Warning", (char *) "Some ACSI ID has no selected type.\n\rGo and select something!");
        return;
    }

    if(!somethingActive) {									// if everything is set to OFF
        showMessageScreen((char *) "Warning", (char *) "All ACSI IDs are set to 'OFF',\n\rthis is invalid and would brick the device.\n\rSelect at least one active ACSI ID.");
        return;
    }

    if(tranCnt > 1) {										// more than 1 of this type?
        showMessageScreen((char *) "Warning", (char *) "You have more than 1 translated drives\n\rselected. Unselect some to leave only\n\r1 active.");
        return;
    }

    // if we got here, everything seems to be fine and we can write values to settings
    Settings s;
    char key[32];

    for(int id=0; id<8; id++) {								// write all the ACSI IDs to settings
        sprintf(key, "ACSI_DEVTYPE_%d", id);				// create settings KEY, e.g. ACSI_DEVTYPE_0
        s.setInt(key, devTypes[id]);
    }

    createScreen_homeScreen();		// now back to the home screen
}

void ConfigStream::onTranslated_save(void)
{
    std::string value;
    char letter1, letter2, letter3;

    getTextByComponentId(COMPID_TRAN_FIRST, value);

    if(value.length() < 1) {    // no drive letter
        letter1 = 0;
    } else {
        letter1 = value[0];
    }

    getTextByComponentId(COMPID_TRAN_SHARED, value);

    if(value.length() < 1) {    // no drive letter
        letter2 = 0;
    } else {
        letter2 = value[0];
    }

    getTextByComponentId(COMPID_TRAN_CONFDRIVE, value);

    if(value.length() < 1) {    // no drive letter
        letter3 = 0;
    } else {
        letter3 = value[0];
    }

    if(letter1 == 0 && letter2 == 0 && letter3 == 0) {
        showMessageScreen((char *) "Info", (char *) "No drive letter assigned, this is OK,\n\rbut the translated disk will be\n\runaccessible.");
    }

    if(letter1 == letter2 || letter1 == letter3 || letter2 == letter3) {
        showMessageScreen((char *) "Warning", (char *) "Drive letters must be different!\n\rPlease fix this and try again.");
        return;
    }

    if((letter1 != 0 && letter1 < 'C') || (letter2 != 0 && letter2 < 'C') || (letter3 != 0 && letter3 < 'C')) {
        showMessageScreen((char *) "Warning", (char *) "Drive letters A and B are for floppies.\n\rPlease fix this and try again.");
        return;
    }

    if(letter1 > 'P' || letter2 > 'P' || letter3 > 'P') {
        showMessageScreen((char *) "Warning", (char *) "Last allowed drive letter is 'P'.\n\rPlease fix this and try again.");
        return;
    }

    // now save the settings
    Settings s;
    s.setChar((char *) "DRIVELETTER_FIRST",      letter1);
    s.setChar((char *) "DRIVELETTER_SHARED",     letter2);
    s.setChar((char *) "DRIVELETTER_CONFDRIVE",  letter3);

    createScreen_homeScreen();		// now back to the home screen
}

void ConfigStream::onNetwork_save(void)
{
    std::string ip, mask, dns, gateway;
    bool useDhcp;

    // read the settings from components
    getBoolByComponentId(COMPID_NET_DHCP, useDhcp);
    getTextByComponentId(COMPID_NET_IP, ip);
    getTextByComponentId(COMPID_NET_MASK, mask);
    getTextByComponentId(COMPID_NET_DNS, dns);
    getTextByComponentId(COMPID_NET_GATEWAY, gateway);

    // verify the settings
    if(!useDhcp) {          // but verify settings only when not using dhcp
        bool a,b,c,d;

        a = verifyAndFixIPaddress(ip,       ip,         false);
        b = verifyAndFixIPaddress(mask,     mask,       false);
        c = verifyAndFixIPaddress(dns,      dns,        true);
        d = verifyAndFixIPaddress(gateway,  gateway,    true);

        if(!a || !b || !c || !d) {
            showMessageScreen((char *) "Warning", (char *) "Some network address has invalid format.\n\rPlease fix this and try again.");
            return;
        }
    }

    // store the settings
    Settings s;
    s.setBool((char *) "NET_USE_DHCP", useDhcp);

    if(!useDhcp) {          // if not using dhcp, store also the network settings
        s.setString((char *) "NET_IP",      (char *) ip.c_str());
        s.setString((char *) "NET_MASK",    (char *) mask.c_str());
        s.setString((char *) "NET_DNS",     (char *) dns.c_str());
        s.setString((char *) "NET_GATEWAY", (char *) gateway.c_str());
    }

    createScreen_homeScreen();		// now back to the home screen
}

bool ConfigStream::verifyAndFixIPaddress(std::string &in, std::string &out, bool emptyIsOk)
{
    char ip[40];
    strcpy(ip, in.c_str());

    if(in.length() == 0) {      // empty string might be OK
        if(emptyIsOk) {
            return true;
        } else {
            return false;
        }
    }

    // try to read the numbers
    int i1, i2, i3, i4;
    bool res = sscanf(ip, "%d.%d.%d.%d", &i1, &i2, &i3, &i4);

    if(!res) {          // couldn't read the numbers?
        return false;
    }

    // numbers out of range?
    if(i1 < 0 || i1 > 255 || i2 < 0 || i2 > 255 || i3 < 0 || i3 > 255 || i4 < 0 || i4 > 255) {
        return false;
    }

    // format it back (in case there would be something extra in the field)
    sprintf(ip, "%d.%d.%d.%d", i1, i2, i3, i4);
    out = ip;

    return true;
}
