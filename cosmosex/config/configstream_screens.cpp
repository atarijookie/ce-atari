#include <stdio.h>
#include <string.h>

#include "../global.h"
#include "../native/scsi_defs.h"
#include "../acsidatatrans.h"

#include "../settings.h"
#include "keys.h"
#include "configstream.h"

extern "C" void outDebugString(const char *format, ...);

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

    comp = new ConfigComponent(this, ConfigComponent::button, " ACSI IDs config ",	18, 10,  6, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_ACSI);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Translated disks ",	18, 10,  8, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_TRANSLATED);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Shared drive ",		18, 10, 10, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_SHARED);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Floppy config ",	18, 10, 12, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_FLOPPY);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Network settings ",	18, 10, 14, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_NETWORK);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Update software ",	18, 10, 16, gotoOffset);
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

    comp = new ConfigComponent(this, ConfigComponent::label, "ID             off   raw  tran", 40, 0, 3, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);

    for(int row=0; row<8; row++) {			// now make 8 rows of checkboxes
        char bfr[5];
        sprintf(bfr, "%d", row);

        comp = new ConfigComponent(this, ConfigComponent::label, bfr, 2, 1, row + 4, gotoOffset);
        screen.push_back(comp);

        for(int col=0; col<3; col++) {
            comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ", 3, 14 + (col * 6), row + 4, gotoOffset);			// create and place checkbox on screen
            comp->setCheckboxGroupIds(row, col);																// set checkbox group id to row, and checbox id to col
            screen.push_back(comp);
        }
    }

    comp = new ConfigComponent(this, ConfigComponent::label, "off  - turned off, not responding here",	40, 0, 17, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "raw  - raw sector access (use HDDr/ICD)",	40, 0, 18, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "tran - translated access      (only one)",	40, 0, 19, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ", 8,  9, 13, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SAVE_ACSI);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, 20, 13, gotoOffset);
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

    comp = new ConfigComponent(this, ConfigComponent::label, "Use DHCP",	10, 12, 6, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, " ",	1, 22, 6, gotoOffset);
    comp->setComponentId(COMPID_NET_DHCP);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "If not using DHCP, set the next params:", 40, 0, 9, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "IP address", 40, 0, 11, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ", 15, 16, 11, gotoOffset);
    comp->setComponentId(COMPID_NET_IP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Mask", 40, 0, 12, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ", 15, 16, 12, gotoOffset);
    comp->setComponentId(COMPID_NET_MASK);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Gateway", 40, 0, 13, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ", 15, 16, 13, gotoOffset);
    comp->setComponentId(COMPID_NET_GATEWAY);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "DNS", 40, 0, 14, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ", 15, 16, 14, gotoOffset);
    comp->setComponentId(COMPID_NET_DNS);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ", 8,  9, 16, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SAVE_NETWORK);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, 20, 16, gotoOffset);
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

    comp = new ConfigComponent(this, ConfigComponent::label, "        Drive letters assignment", 40, 0, 5, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "First translated drive", 23, 6, 7, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",	1, 33, 7, gotoOffset);
    comp->setComponentId(COMPID_TRAN_FIRST);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Shared drive", 23, 6, 9, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",	1, 33, 9, gotoOffset);
    comp->setComponentId(COMPID_TRAN_SHARED);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Config drive", 23, 6, 10, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",	1, 33, 10, gotoOffset);
    comp->setComponentId(COMPID_TRAN_CONFDRIVE);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "If you use also raw disks (Atari native ", 40, 0, 17, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "disks), you should avoid using few", 40, 0, 18, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "letters from C: to leave some space for", 40, 0, 19, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "them.",	40, 0, 20, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ", 8,  9, 13, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SAVE_TRANSLATED);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, 20, 13, gotoOffset);
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

    if(reloadProxy) {                                       // if got settings reload proxy, invoke reload
        reloadProxy->reloadSettings(SETTINGSUSER_ACSI);
    }

    createScreen_homeScreen();		// now back to the home screen
}

void ConfigStream::onTranslated_save(void)
{
    std::string value;
    char letter1, letter2, letter3;

    getTextByComponentId(COMPID_TRAN_FIRST, value);

    if(value.length() < 1) {    // no drive letter
        letter1 = -1;
    } else {
        letter1 = value[0];
    }

    getTextByComponentId(COMPID_TRAN_SHARED, value);

    if(value.length() < 1) {    // no drive letter
        letter2 = -1;
    } else {
        letter2 = value[0];
    }

    getTextByComponentId(COMPID_TRAN_CONFDRIVE, value);

    if(value.length() < 1) {    // no drive letter
        letter3 = -1;
    } else {
        letter3 = value[0];
    }

    if(letter1 == -1 && letter2 == -1 && letter3 == -1) {
        showMessageScreen((char *) "Info", (char *) "No drive letter assigned, this is OK,\n\rbut the translated disk will be\n\runaccessible.");
    }

    if(letter1 == letter2 || letter1 == letter3 || letter2 == letter3) {
        showMessageScreen((char *) "Warning", (char *) "Drive letters must be different!\n\rPlease fix this and try again.");
        return;
    }

    if((letter1 != -1 && letter1 < 'C') || (letter2 != -1 && letter2 < 'C') || (letter3 != -1 && letter3 < 'C')) {
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

    if(reloadProxy) {                                       // if got settings reload proxy, invoke reload
        reloadProxy->reloadSettings(SETTINGSUSER_TRANSLATED);
    }

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

void ConfigStream::createScreen_update(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();			// destroy current components
    screenChanged	= true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, (char *) "Software & Firmware updates");

    ConfigComponent *comp;

    comp = new ConfigComponent(this, ConfigComponent::label, " part         new version available?", 40, 0, 8, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "CosmosEx", 12, 1, 10, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 3, 24, 10, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_COSMOSEX);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Franz", 12, 1, 11, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 3, 24, 11, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_FRANZ);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Hans", 12, 1, 12, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 3, 24, 12, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_HANZ);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Config image", 12, 1, 13, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 3, 24, 13, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_CONF_IMAGE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Check  ", 8,  4, 15, gotoOffset);
    comp->setOnEnterFunctionCode(CS_UPDATE_CHECK);
    comp->setComponentId(COMPID_UPDATE_BTN_CHECK);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ", 8,  15, 15, gotoOffset);
    comp->setOnEnterFunctionCode(CS_UPDATE_UPDATE);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, 27, 15, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    setFocusToFirstFocusable();
}

void ConfigStream::onUpdateCheck(void)
{

}

void ConfigStream::onUpdateUpdate(void)
{

}

void ConfigStream::createScreen_shared(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();			// destroy current components
    screenChanged	= true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, (char *) "Shared drive settings");

    ConfigComponent *comp;

    comp = new ConfigComponent(this, ConfigComponent::label, "Define what folder on which machine will", 40, 0, 4, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "be used as drive mounted through network", 40, 0, 5, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "on CosmosEx. Works in translated mode.", 40, 0, 6, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "IP address of server", 40, 11, 10, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ", 15, 12, 11, gotoOffset);
    comp->setComponentId(COMPID_SHARED_IP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Shared folder path on server", 40, 7, 13, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ", 35, 2, 14, gotoOffset);
    comp->setComponentId(COMPID_SHARED_PATH);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Test  ", 8,  4, 17, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SHARED_TEST);
    comp->setComponentId(COMPID_SHARED_BTN_TEST);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ", 8,  15, 17, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SHARED_SAVE);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, 27, 17, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    Settings s;
    std::string addr, path;

    addr = s.getString((char *) "SHARED_ADDRESS",  (char *) "");
    path = s.getString((char *) "SHARED_PATH",     (char *) "");

    setTextByComponentId(COMPID_SHARED_IP,      addr);
    setTextByComponentId(COMPID_SHARED_PATH,    path);

    setFocusToFirstFocusable();
}

void ConfigStream::onSharedTest(void)
{

}

void ConfigStream::onSharedSave(void)
{
    std::string ip, path;

    getTextByComponentId(COMPID_SHARED_IP,      ip);
    getTextByComponentId(COMPID_SHARED_PATH,    path);

    if(!verifyAndFixIPaddress(ip, ip, false)) {
        showMessageScreen((char *) "Warning", (char *) "Server address seems to be invalid.\n\rPlease fix this and try again.");
        return;
    }

    if(path.length() < 1) {
        showMessageScreen((char *) "Warning", (char *) "Path for server is empty.\n\rPlease fix this and try again.");
        return;
    }

    Settings s;

    s.setString((char *) "SHARED_ADDRESS",  (char *) ip.c_str());
    s.setString((char *) "SHARED_PATH",     (char *) path.c_str());

    createScreen_homeScreen();		// now back to the home screen
}
