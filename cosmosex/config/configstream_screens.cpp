#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../global.h"
#include "../debug.h"
#include "../native/scsi_defs.h"
#include "../acsidatatrans.h"
#include "../mounter.h"

#include "../settings.h"
#include "../utils.h"
#include "../update.h"
#include "../downloader.h"
#include "keys.h"
#include "configstream.h"
#include "netsettings.h"

extern volatile bool do_timeSync;
extern volatile bool do_loadIkbdConfig;

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
    comp->setOnEnterFunctionCode(CS_CREATE_FLOPPY_CONF);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Network settings ",	18, 10, 14, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_NETWORK);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Other ",	        18, 10, 16, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_OTHER);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Update software ",	18, 10, 18, gotoOffset);
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

    comp = new ConfigComponent(this, ConfigComponent::label, "ID         off   sd    raw  tran", 40, 0, 3, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);

    int row;

    for(row=0; row<8; row++) {			// now make 8 rows of checkboxes
        char bfr[5];
        sprintf(bfr, "%d", row);

        comp = new ConfigComponent(this, ConfigComponent::label, bfr, 2, 1, row + 4, gotoOffset);
        screen.push_back(comp);

        for(int col=0; col<4; col++) {
            comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ", 3, 10 + (col * 6), row + 4, gotoOffset);			// create and place checkbox on screen
            comp->setCheckboxGroupIds(row, col);																// set checkbox group id to row, and checbox id to col
            screen.push_back(comp);
        }
    }

    row = 17;

    comp = new ConfigComponent(this, ConfigComponent::label, "off  - turned off, not responding here",      40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "sd   - SD card                (only one)",    40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "raw  - raw sector access (use HDDr/ICD)",     40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "tran - translated access      (only one)",	40, 0, row++, gotoOffset);
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

	int col0x = 0;
	int col1x = 3;
	int col2x = 16;
	
	int row = 4;
	
	// settings for ethernet
    comp = new ConfigComponent(this, ConfigComponent::label, "Ethernet",	10,	col0x, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Use DHCP",	10, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, " ",		1,	col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_NET_DHCP);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "IP address",	40,	col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",	15, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_NET_IP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Mask",		40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",	15, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_NET_MASK);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Gateway",		40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",	15, col2x, row, gotoOffset);
    comp->setComponentId(COMPID_NET_GATEWAY);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

	row += 2;
	
	// settings for wifi
    comp = new ConfigComponent(this, ConfigComponent::label, "Wifi",		10,	col0x, row++, gotoOffset);
    screen.push_back(comp);

	comp = new ConfigComponent(this, ConfigComponent::label, "WPA SSID",	20,	col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",	20, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_SSID);
    comp->setTextOptions(TEXT_OPTION_ALLOW_ALL);
    screen.push_back(comp);

	comp = new ConfigComponent(this, ConfigComponent::label, "WPA PSK",		20,	col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",	20, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_PSK);
    comp->setTextOptions(TEXT_OPTION_ALLOW_ALL);
    screen.push_back(comp);
	
    comp = new ConfigComponent(this, ConfigComponent::label, "Use DHCP",	10, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, " ",		1,	col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_DHCP);
    screen.push_back(comp);
	
    comp = new ConfigComponent(this, ConfigComponent::label, "IP address",	40,	col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",	15, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_IP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Mask",		40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",	15, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_MASK);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Gateway",		40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",	15, col2x, row, gotoOffset);
    comp->setComponentId(COMPID_WIFI_GATEWAY);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);
		
	// dns settings
	row += 2;
	
    comp = new ConfigComponent(this, ConfigComponent::label, "DNS",			40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",	15, col2x, row, gotoOffset);
    comp->setComponentId(COMPID_NET_DNS);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);
	
	row += 2;
	// buttons

    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ", 8,  9, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SAVE_NETWORK);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, 20, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    // get the settings, store them to components
	NetworkSettings ns;
	ns.load();						// load the current values

    setTextByComponentId(COMPID_NET_DNS,		ns.nameserver);

    setBoolByComponentId(COMPID_NET_DHCP,		ns.eth0.dhcpNotStatic);
    setTextByComponentId(COMPID_NET_IP,			ns.eth0.address);
    setTextByComponentId(COMPID_NET_MASK,		ns.eth0.netmask);
    setTextByComponentId(COMPID_NET_GATEWAY,	ns.eth0.gateway);

    setBoolByComponentId(COMPID_WIFI_DHCP,		ns.wlan0.dhcpNotStatic);
    setTextByComponentId(COMPID_WIFI_IP,		ns.wlan0.address);
    setTextByComponentId(COMPID_WIFI_MASK,		ns.wlan0.netmask);
    setTextByComponentId(COMPID_WIFI_GATEWAY,	ns.wlan0.gateway);

    setTextByComponentId(COMPID_WIFI_SSID,		ns.wlan0.wpaSsid);
    setTextByComponentId(COMPID_WIFI_PSK,		ns.wlan0.wpaPsk);
	
    setFocusToFirstFocusable();
}

void ConfigStream::createScreen_translated(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();				// destroy current components
    screenChanged		= true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, (char *) "Translated disk");

    int col1x = 4, col2x = 23, col3x = 29;
    ConfigComponent *comp;

    comp = new ConfigComponent(this, ConfigComponent::label, "        Drive letters assignment", 40, 0, 5, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "First translated drive", 23, col1x, 7, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",	1, 33, 7, gotoOffset);
    comp->setComponentId(COMPID_TRAN_FIRST);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Shared drive", 23, col1x, 9, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",	1, 33, 9, gotoOffset);
    comp->setComponentId(COMPID_TRAN_SHARED);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Config drive", 23, col1x, 10, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",	1, 33, 10, gotoOffset);
    comp->setComponentId(COMPID_TRAN_CONFDRIVE);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    //------------
    int row = 12;
    
    comp = new ConfigComponent(this, ConfigComponent::label, "Mount USB media as",							40, col1x, row, gotoOffset);
    screen.push_back(comp);
	
	comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",										3,	col2x, row, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_MOUNT_RAW_NOT_TRANS, 0);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "translated",  								40,	col3x, row++, gotoOffset);
    screen.push_back(comp);
	
	comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",										3,	col2x, row, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_MOUNT_RAW_NOT_TRANS, 1);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "RAW",						                    40,	col3x, row++, gotoOffset);
    screen.push_back(comp);

	row++;
    //------------
    
    comp = new ConfigComponent(this, ConfigComponent::label, "If you use also raw disks (Atari native ", 40, 0, 17, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "disks), you should avoid using few", 40, 0, 18, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "letters from C: to leave some space for", 40, 0, 19, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "them.",	40, 0, 20, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ", 8,  9, 15, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SAVE_TRANSLATED);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, 20, 15, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    // get the letters from settings, store them to components
    Settings s;
    char drive1, drive2, drive3;
    bool mountRawNotTrans;

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

    mountRawNotTrans = s.getBool((char *) "MOUNT_RAW_NOT_TRANS", 0);
    
 	if(mountRawNotTrans) {
		checkboxGroup_setCheckedId(COMPID_MOUNT_RAW_NOT_TRANS, 1);			// select RAW
	} else {
		checkboxGroup_setCheckedId(COMPID_MOUNT_RAW_NOT_TRANS, 0);			// select TRANS
	}

    setFocusToFirstFocusable();
}

void ConfigStream::onAcsiConfig_save(void)
{
    int devTypes[8];

    bool somethingActive = false;
    bool somethingInvalid = false;
    int tranCnt = 0, sdCnt = 0;

    for(int id=0; id<8; id++) {								// get all selected types from checkbox groups
        devTypes[id] = checkboxGroup_getCheckedId(id);

        if(devTypes[id] != DEVTYPE_OFF) {					// if found something which is not OFF
            somethingActive = true;
        }

        switch(devTypes[id]) {								// count the shared drives, network adapters, config drives
        case DEVTYPE_TRANSLATED:	tranCnt++;					break;
        case DEVTYPE_SD:            sdCnt++;                    break;
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

    if(sdCnt > 1) {										    // more than 1 of this type?
        showMessageScreen((char *) "Warning", (char *) "You have more than 1 SD cards\n\rselected. Unselect some to leave only\n\r1 active.");
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

    Utils::forceSync();                                     // tell system to flush the filesystem caches

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

    bool mountRawNotTrans = (bool) checkboxGroup_getCheckedId(COMPID_MOUNT_RAW_NOT_TRANS);
    
    // now save the settings
    Settings s;
    s.setChar((char *) "DRIVELETTER_FIRST",      letter1);
    s.setChar((char *) "DRIVELETTER_SHARED",     letter2);
    s.setChar((char *) "DRIVELETTER_CONFDRIVE",  letter3);
    s.setBool((char *) "MOUNT_RAW_NOT_TRANS",    mountRawNotTrans);

    if(reloadProxy) {                                       // if got settings reload proxy, invoke reload
        reloadProxy->reloadSettings(SETTINGSUSER_TRANSLATED);
    }

    Utils::forceSync();                                     // tell system to flush the filesystem caches

    createScreen_homeScreen();		// now back to the home screen
}

void ConfigStream::onNetwork_save(void)
{
	// for eth0
    std::string ip, mask, gateway;
    bool useDhcp;

	// for wlan0
    std::string ip2, mask2, gateway2;
    bool useDhcp2;
	
	std::string dns;

    // read the settings from components
    getBoolByComponentId(COMPID_NET_DHCP,		useDhcp);
    getTextByComponentId(COMPID_NET_IP,			ip);
    getTextByComponentId(COMPID_NET_MASK,		mask);
    getTextByComponentId(COMPID_NET_GATEWAY,	gateway);
	
	getBoolByComponentId(COMPID_WIFI_DHCP,		useDhcp2);
    getTextByComponentId(COMPID_WIFI_IP,		ip2);
    getTextByComponentId(COMPID_WIFI_MASK,		mask2);
    getTextByComponentId(COMPID_WIFI_GATEWAY,	gateway2);

    getTextByComponentId(COMPID_NET_DNS,		dns);

    // verify the settings for eth0
    if(!useDhcp) {          // but verify settings only when not using dhcp
        bool a,b,c,d;

        a = verifyAndFixIPaddress(ip,       ip,         false);
        b = verifyAndFixIPaddress(mask,     mask,       false);
        c = verifyAndFixIPaddress(dns,      dns,        true);
        d = verifyAndFixIPaddress(gateway,  gateway,    true);

        if(!a || !b || !c || !d) {
            showMessageScreen((char *) "Warning", (char *) "Some ethernet network address has invalid format.\n\rPlease fix this and try again.");
            return;
        }
    }
	
	if(!useDhcp2) {          // but verify settings only when not using dhcp
        bool a,b,c;

        a = verifyAndFixIPaddress(ip2,       ip2,         false);
        b = verifyAndFixIPaddress(mask2,     mask2,       false);
        c = verifyAndFixIPaddress(gateway2,  gateway2,    true);

        if(!a || !b || !c) {
            showMessageScreen((char *) "Warning", (char *) "Some wifi network address has invalid format.\n\rPlease fix this and try again.");
            return;
        }
    }

	//-------------------------
    // store the settings
	NetworkSettings ns;
	ns.load();						// load the current values

    ns.nameserver = dns;			
	
	ns.eth0.dhcpNotStatic = useDhcp;

    if(!useDhcp) {          		// if not using dhcp, store also the network settings
        ns.eth0.address = ip;
        ns.eth0.netmask = mask;
        ns.eth0.gateway = gateway;
    }

	getTextByComponentId(COMPID_WIFI_SSID,		ns.wlan0.wpaSsid);
	getTextByComponentId(COMPID_WIFI_PSK,		ns.wlan0.wpaPsk);

	ns.wlan0.dhcpNotStatic = useDhcp2;

    if(!useDhcp2) {          		// if not using dhcp, store also the network settings
        ns.wlan0.address = ip2;
        ns.wlan0.netmask = mask2;
        ns.wlan0.gateway = gateway2;
    }

	ns.save();						// store the new values
	
	//-------------------------
	// now request network restart
	TMounterRequest tmr;			
	tmr.action	= MOUNTER_ACTION_RESTARTNETWORK;								
	mountAdd(tmr);
	
    Utils::forceSync();                                     // tell system to flush the filesystem caches

	//-------------------------
    createScreen_homeScreen();		// now back to the home screen
}

void ConfigStream::createScreen_update(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();			// destroy current components
    screenChanged	= true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, (char *) "Software & Firmware updates");

    updateFromWebNotUsb = true;         // do update from web
    
    ConfigComponent *comp;

    comp = new ConfigComponent(this, ConfigComponent::label, " part       your version   ", 27, 0, 8, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "on web", 13, 27, 8, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_LOCATION);
    comp->setReverse(true);
    screen.push_back(comp);

	int col1 = 1, col2 = 13;
		
    comp = new ConfigComponent(this, ConfigComponent::label, "Main App", 12,	col1, 10, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 26,			col2, 10, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_COSMOSEX);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Franz", 12, 		col1, 11, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 26, 			col2, 11, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_FRANZ);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Hans", 12,		col1, 12, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 26,			col2, 12, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_HANZ);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Xilinx", 12,		col1, 13, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 26,			col2, 13, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_XILINX);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " From web ", 10,  6, 15, gotoOffset);
    comp->setOnEnterFunctionCode(CS_UPDATE_CHECK);
    comp->setComponentId(COMPID_UPDATE_BTN_CHECK);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " From USB ", 10,  22, 15, gotoOffset);
    comp->setOnEnterFunctionCode(CS_UPDATE_CHECK_USB);
    comp->setComponentId(COMPID_UPDATE_BTN_CHECK_USB);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Update  ", 10,  6, 16, gotoOffset);
    comp->setOnEnterFunctionCode(CS_UPDATE_UPDATE);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Cancel  ", 10,  22, 16, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    fillUpdateWithCurrentVersions();                // fill the version lines with versions, if we got them

    setFocusToFirstFocusable();
}

void ConfigStream::fillUpdateWithCurrentVersions(void)
{
    std::string str;    

    datesToStrings(Update::versions.current.app,       Update::versions.onServer.app,     str);
    setTextByComponentId(COMPID_UPDATE_COSMOSEX, str);      // set it to component

    datesToStrings(Update::versions.current.franz,     Update::versions.onServer.franz,   str);
    setTextByComponentId(COMPID_UPDATE_FRANZ, str);         // set it to component

    datesToStrings(Update::versions.current.hans,      Update::versions.onServer.hans,    str);
    setTextByComponentId(COMPID_UPDATE_HANZ, str);          // set it to component

    datesToStrings(Update::versions.current.xilinx,    Update::versions.onServer.xilinx,  str);
    setTextByComponentId(COMPID_UPDATE_XILINX, str);        // set it to component
}

void ConfigStream::datesToStrings(Version &v1, Version &v2, std::string &str)
{
    char ver[40];
   
    v1.toString(ver);           // get single version
    str = ver;                  // put it in string
    str.resize(14, ' ');        // and expand it to length of 14 with spaces

    if(v1.isEqualTo(v2)) {      // if the other version is the same as the first, just write some info
        str += "the same";
    } else {                    // if the versions are different, write the other version
        v2.toString(ver);       // get another single version
        str += ver;             // append it to the previous version string
    }
}

void ConfigStream::onUpdateCheck(void)
{
    updateFromWebNotUsb = true;         // do update from web

    // update config screen
    std::string empty;
    std::string location = "on web";
    Update::deleteLocalUpdateComponents();
    setTextByComponentId(COMPID_UPDATE_COSMOSEX,    empty);
    setTextByComponentId(COMPID_UPDATE_FRANZ,       empty);
    setTextByComponentId(COMPID_UPDATE_HANZ,        empty);
    setTextByComponentId(COMPID_UPDATE_XILINX,      empty);
    setTextByComponentId(COMPID_UPDATE_LOCATION,    location);

    // download the stuff again
    Update::versions.updateListWasProcessed = false;    // mark that the new update list wasn't updated
    Update::downloadUpdateList(NULL);                   // download the list of components with the newest available versions
}

void ConfigStream::onUpdateCheckUsb(void)
{
    updateFromWebNotUsb = false;         // do update from usb

    // update config screen
    std::string empty;
    std::string location = "on USB";
    Update::deleteLocalUpdateComponents();
    setTextByComponentId(COMPID_UPDATE_COSMOSEX,    empty);
    setTextByComponentId(COMPID_UPDATE_FRANZ,       empty);
    setTextByComponentId(COMPID_UPDATE_HANZ,        empty);
    setTextByComponentId(COMPID_UPDATE_XILINX,      empty);
    setTextByComponentId(COMPID_UPDATE_LOCATION,    location);

    // try to find the update
    std::string pathToUpdateFile;
    
    bool found = Update::checkForUpdateListOnUsb(pathToUpdateFile);

    if(!found) {
        showMessageScreen((char *) "Update from USB", (char *) "File ce_update.zip not found.\n\rCan't update from USB.\n\r");
        return;
    }
    
    // copy and unzip the update
    Update::downloadUpdateList((char *) pathToUpdateFile.c_str());
    
    Update::versions.updateListWasProcessed = false;            // mark that the new update list wasn't updated
}

void ConfigStream::onUpdateUpdate(void)
{
    if(!Update::versions.updateListWasProcessed) {     // didn't process the update list yet? show message
        showMessageScreen((char *) "No updates info", (char *) "No update info was downloaded,\n\rplease press web / usb button and wait.");
        return;
    }

    if(!Update::versions.gotUpdate) {
        showMessageScreen((char *) "No update needed", (char *) "All your components are up to date.");
        return;
    }

    if(updateFromWebNotUsb) {           // if should update from web
        createScreen_update_download();
    } else {                            // if should update from usb
        Update::stateGoDownloadOK();    // tell the core thread that we got the files already
    }
}

void ConfigStream::createScreen_update_download(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();			    // destroy current components
    screenChanged	    = true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, (char *) "Updates download");

    ConfigComponent *comp;

    int col = 7;
    int row = 9;

    comp = new ConfigComponent(this, ConfigComponent::label, "Downloading", 40, col + 2, row, gotoOffset);
    screen.push_back(comp);

    row += 2;

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 30, col, row++, gotoOffset);
    comp->setComponentId(COMPID_DL1);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 30, col, row++, gotoOffset);
    comp->setComponentId(COMPID_DL2);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 30, col, row++, gotoOffset);
    comp->setComponentId(COMPID_DL3);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 30, col, row++, gotoOffset);
    comp->setComponentId(COMPID_DL4);
    screen.push_back(comp);

    row++;

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, col + 3, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    Update::downloadNewComponents();            // start the download

    setFocusToFirstFocusable();
}

void ConfigStream::createScreen_other(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();			    // destroy current components
    screenChanged	    = true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, (char *) "Other settings");

    ConfigComponent *comp;

    int row     = 5;
    int col     = 7;
    int col2    = 20;
    
    //----------------------
    comp = new ConfigComponent(this, ConfigComponent::label, "Update time from internet",   40, col, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Enable",                      40, col + 2, row, gotoOffset);
    screen.push_back(comp);
    
    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                      3,  col2, row++, gotoOffset);
    comp->setComponentId(COMPID_TIMESYNC_ENABLE);
    screen.push_back(comp);
    
    comp = new ConfigComponent(this, ConfigComponent::label, "NTP server",                  40, col + 2, row, gotoOffset);
    screen.push_back(comp);
    
    comp = new ConfigComponent(this, ConfigComponent::editline, " ",					    15, col2, row++, gotoOffset);
    comp->setComponentId(COMPID_TIMESYNC_NTP_SERVER);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "UTC offset",                  40, col + 2, row, gotoOffset);
    screen.push_back(comp);
    
    comp = new ConfigComponent(this, ConfigComponent::editline, " ",					    4, col2, row++, gotoOffset);
    comp->setComponentId(COMPID_TIMESYNC_UTC_OFFSET);
    screen.push_back(comp);

    //-----------
    row++;
    comp = new ConfigComponent(this, ConfigComponent::label, "Screencast Frameskip (10-255)",   40, col, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",                      3,  col2, row++, gotoOffset);
    comp->setComponentId(COMPID_SCREENCAST_FRAMESKIP);
    screen.push_back(comp);

    //-----------
    row++;
    comp = new ConfigComponent(this, ConfigComponent::label, "Attach 1st joy as JOY 0",     40, col, row, gotoOffset);
    screen.push_back(comp);
    
    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                      3,  32, row++, gotoOffset);
    comp->setComponentId(COMPID_JOY0_FIRST);
    screen.push_back(comp);

    //----------------------

    comp = new ConfigComponent(this, ConfigComponent::button, " Reset all settings ",       19, 10, 16, gotoOffset);
    comp->setOnEnterFunctionCode(CS_RESET_SETTINGS);
    screen.push_back(comp);
    
    comp = new ConfigComponent(this, ConfigComponent::button, "   Save   ",                 10,  6, 18, gotoOffset);
    comp->setOnEnterFunctionCode(CS_OTHER_SAVE);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Cancel  ",                 10,  22, 18, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    //------------------------
    Settings s;
    
    bool        setDateTime;
    bool        joy0First;
    float       utcOffset;
    std::string ntpServer;
    int			frameSkip;
    
    setDateTime = s.getBool     ((char *) "TIME_SET",             true);
    utcOffset   = s.getFloat    ((char *) "TIME_UTC_OFFSET",      0);
    ntpServer   = s.getString   ((char *) "TIME_NTP_SERVER",      (char *) "200.20.186.76");
    frameSkip   = s.getInt      ((char *) "SCREENCAST_FRAMESKIP", 20);
    joy0First   = s.getBool     ((char *) "JOY_FIRST_IS_0",       false);

    if( frameSkip<10 )
    {
        frameSkip=10;
    }
    if( frameSkip>255 )
    {
        frameSkip=255;
    }
    
    setBoolByComponentId(COMPID_TIMESYNC_ENABLE,        setDateTime);
    setFloatByComponentId(COMPID_TIMESYNC_UTC_OFFSET,   utcOffset);
    setTextByComponentId(COMPID_TIMESYNC_NTP_SERVER,    ntpServer);
    setIntByComponentId(COMPID_SCREENCAST_FRAMESKIP,    frameSkip);
    setBoolByComponentId(COMPID_JOY0_FIRST,             joy0First);
    //------------------------
    
    setFocusToFirstFocusable();
}

void ConfigStream::onOtherSave(void)
{
    Settings s;

    bool        setDateTime = false;
    bool        joy0First   = false;
    float       utcOffset   = 0;
    std::string ntpServer;
    int			frameSkip;
    
    getBoolByComponentId(COMPID_TIMESYNC_ENABLE,        setDateTime);
    getFloatByComponentId(COMPID_TIMESYNC_UTC_OFFSET,   utcOffset);
    getTextByComponentId(COMPID_TIMESYNC_NTP_SERVER,    ntpServer);
    getIntByComponentId(COMPID_SCREENCAST_FRAMESKIP,    frameSkip);
    getBoolByComponentId(COMPID_JOY0_FIRST,             joy0First);
    
    if( frameSkip<10 )
    {
        frameSkip=10;
    }
    if( frameSkip>255 )
    {
        frameSkip=255;
    }
    
    s.setBool     ((char *) "TIME_SET",             setDateTime);
    s.setFloat    ((char *) "TIME_UTC_OFFSET",      utcOffset);
    s.setString   ((char *) "TIME_NTP_SERVER",      (char *) ntpServer.c_str());
    s.setInt      ((char *) "SCREENCAST_FRAMESKIP", frameSkip);
    s.setBool     ((char *) "JOY_FIRST_IS_0",       joy0First);

    do_timeSync         = true;     // do time sync again
    do_loadIkbdConfig   = true;     // reload ikbd config

    Utils::forceSync();             // tell system to flush the filesystem caches

    createScreen_homeScreen();		// now back to the home screen
}

void ConfigStream::fillUpdateDownloadWithProgress(void)
{
    std::string status, l1, l2, l3, l4;

    // get the current download status
    Downloader::status(status, DWNTYPE_UPDATE_COMP);

    // split it to lines
    getProgressLine(0, status, l1);
    getProgressLine(1, status, l2);
    getProgressLine(2, status, l3);
    getProgressLine(3, status, l4);

    // set it to components
    setTextByComponentId(COMPID_DL1, l1);
    setTextByComponentId(COMPID_DL2, l2);
    setTextByComponentId(COMPID_DL3, l3);
    setTextByComponentId(COMPID_DL4, l4);
}

void ConfigStream::getProgressLine(int index, std::string &lines, std::string &line)
{
    line.clear();

    int c = 0;
    std::size_t pos = 0;

    while(1) {
        if(c == index) {                        // the current count matches the line we're looking for? good
            break;
        }

        pos = lines.find("\n", pos);            // try to find the next \n character

        if(pos == std::string::npos) {          // not found? failed to find the right line, quit
            return;
        }

        pos++;                                  // pos was pointing to \n, the next find would return the same, so move forward
        c++;                                    // update the count of found \n chars
    }

    std::size_t len;
    std::size_t pos2 = lines.find("\n", pos);   // find the end of this line (might return string::npos)

    if(pos2 != std::string::npos) {             // the terminating \n was found?
        len = pos2 - pos;                       // calculate the length of the string 
    } else {                                    // terminating \n was not found? copy until the end of string
        len = std::string::npos;
    }

    line = lines.substr(pos, len);              // get just this line
}

void ConfigStream::showUpdateDownloadFail(void)
{
    // check if we're on update download page
    if(!isUpdateDownloadPageShown()) {
        return;
    }

    // ok, so we're on update donload page... go back to update page and show error
    createScreen_update();
    showMessageScreen((char *) "Update download fail", (char *) "Failed to download the update,\nplease try again later.");
}

void ConfigStream::showUpdateError(void)
{
    // check if we're on update download page
    if(!isUpdateDownloadPageShown()) {
        return;
    }

    // ok, so we're on update donload page... go back to update page and show error
    createScreen_update();
    showMessageScreen((char *) "Update fail", (char *) "Failed to do the update.\n");
}

bool ConfigStream::isUpdateDownloadPageShown(void)
{
    if(!updateFromWebNotUsb) {                          // if updating from USB key, there's no download page, so say yes
        return true;
    }

    ConfigComponent *c = findComponentById(COMPID_DL1); // find a component which is on update download page

    if(c == NULL) {                                     // not on update download page? 
        return false;
    }

    // we're on that update download page
    return true;
}

void ConfigStream::createScreen_shared(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();			// destroy current components
    screenChanged	= true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, (char *) "Shared drive settings");

    ConfigComponent *comp;

	int row = 3;
	
	int col1x = 0;
	int col2x = 10;
	int col3x = col2x + 6;
	
	// description on the top
    comp = new ConfigComponent(this, ConfigComponent::label, "Define what folder on which machine will",	40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "be used as drive mounted through network",	40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "on CosmosEx. Works in translated mode.",		40, 0, row++, gotoOffset);
    screen.push_back(comp);

	row++;
	
	// enabled checkbox
    comp = new ConfigComponent(this, ConfigComponent::label, "Enabled",										40,	col1x, row, gotoOffset);
    screen.push_back(comp);
	
    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ", 										3,	col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_SHARED_ENABLED);
    screen.push_back(comp);

	row++;
	
	// sharing protocol checkbox group
    comp = new ConfigComponent(this, ConfigComponent::label, "Sharing protocol",							40, col1x, row++, gotoOffset);
    screen.push_back(comp);
	
	comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",										3,	col2x, row, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_SHARED_NFS_NOT_SAMBA, 1);																// set checkbox group id COMPID_SHARED_NFS_NOT_SAMBA, and checbox id 1 for NFS (variable SHARED_NFS_NOT_SAMBA)
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "NFS",											40,	col3x, row++, gotoOffset);
    screen.push_back(comp);
	
	comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",										3,	col2x, row, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_SHARED_NFS_NOT_SAMBA, 0);																// set checkbox group id COMPID_SHARED_NFS_NOT_SAMBA, and checbox id 0 for Samba (variable SHARED_NFS_NOT_SAMBA)
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Samba / cifs / windows",						40,	col3x, row++, gotoOffset);
    screen.push_back(comp);

	row++;
	
	
	// ip address edit line
    comp = new ConfigComponent(this, ConfigComponent::label, "IP address of server", 						40, col1x, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",										15, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_SHARED_IP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

	row++;
	
	// folder on server
    comp = new ConfigComponent(this, ConfigComponent::label, "Shared folder path on server",				40, col1x, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",										35, 2, row++, gotoOffset);
    comp->setComponentId(COMPID_SHARED_PATH);
    screen.push_back(comp);
	
	row++;
    
    // username and password
    comp = new ConfigComponent(this, ConfigComponent::label, "Username",				                    40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",										15, col2x, row, gotoOffset);
    comp->setComponentId(COMPID_USERNAME);
    screen.push_back(comp);

	row++;

    comp = new ConfigComponent(this, ConfigComponent::label, "Password",				                    40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",										15, col2x, row, gotoOffset);
    comp->setComponentId(COMPID_PASSWORD);
    screen.push_back(comp);

	row += 2;
	
	// buttons
/*
    comp = new ConfigComponent(this, ConfigComponent::button, "  Test  ",									8,  4, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SHARED_TEST);
    comp->setComponentId(COMPID_SHARED_BTN_TEST);
    screen.push_back(comp);
*/
    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ",									8, /*15*/ 9, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SHARED_SAVE);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ",									8, /*27*/ 21, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    Settings s;
    std::string addr, path, username, password;
	bool enabled, nfsNotSamba;

    addr = s.getString((char *) "SHARED_ADDRESS",  (char *) "");
    path = s.getString((char *) "SHARED_PATH",     (char *) "");

    username = s.getString((char *) "SHARED_USERNAME",  (char *) "");
    password = s.getString((char *) "SHARED_PASSWORD",     (char *) "");

	enabled		= s.getBool((char *) "SHARED_ENABLED",			false);
	nfsNotSamba	= s.getBool((char *) "SHARED_NFS_NOT_SAMBA",	true);

    setTextByComponentId(COMPID_SHARED_IP,      addr);
    setTextByComponentId(COMPID_SHARED_PATH,    path);
	setBoolByComponentId(COMPID_SHARED_ENABLED,	enabled);

    setTextByComponentId(COMPID_USERNAME,       username);
    setTextByComponentId(COMPID_PASSWORD,       password);
	
	if(nfsNotSamba) {
		checkboxGroup_setCheckedId(COMPID_SHARED_NFS_NOT_SAMBA, 1);			// select NFS
	} else {
		checkboxGroup_setCheckedId(COMPID_SHARED_NFS_NOT_SAMBA, 0);			// select samba
	}

    setFocusToFirstFocusable();
}

void ConfigStream::onSharedTest(void)
{

}

void ConfigStream::onSharedSave(void)
{
    std::string ip, path, username, password;
	bool enabled, nfsNotSamba;

    getTextByComponentId(COMPID_SHARED_IP,      ip);
    getTextByComponentId(COMPID_SHARED_PATH,    path);
	getBoolByComponentId(COMPID_SHARED_ENABLED,	enabled);
	nfsNotSamba = (bool) checkboxGroup_getCheckedId(COMPID_SHARED_NFS_NOT_SAMBA);

    getTextByComponentId(COMPID_USERNAME,       username);
    getTextByComponentId(COMPID_PASSWORD,       password);

	if(enabled) {										// if enabled, do validity checks, othewise let it just pass
		if(!verifyAndFixIPaddress(ip, ip, false)) {
			showMessageScreen((char *) "Warning", (char *) "Server address seems to be invalid.\n\rPlease fix this and try again.");
			return;
		}

		if(path.length() < 1) {
			showMessageScreen((char *) "Warning", (char *) "Path for server is empty.\n\rPlease fix this and try again.");
			return;
		}
	}

    Settings s;

	s.setBool	((char *) "SHARED_ENABLED",			enabled);
	s.setBool	((char *) "SHARED_NFS_NOT_SAMBA",	nfsNotSamba);
    s.setString	((char *) "SHARED_ADDRESS",  		(char *) ip.c_str());
    s.setString	((char *) "SHARED_PATH",     		(char *) path.c_str());

    s.setString	((char *) "SHARED_USERNAME",  		(char *) username.c_str());
    s.setString	((char *) "SHARED_PASSWORD",     	(char *) password.c_str());

    if(reloadProxy) {                                       // if got settings reload proxy, invoke reload
        reloadProxy->reloadSettings(SETTINGSUSER_SHARED);
	}

    Utils::forceSync();                                     // tell system to flush the filesystem caches

    createScreen_homeScreen();		// now back to the home screen
}

void ConfigStream::onResetSettings(void)
{
    createScreen_homeScreen();		// now back to the home screen

    showMessageScreen((char *) "Reset all settings", (char *) "All settings have been reset to default.\n\rReseting your ST might be a good idea...");

    system("rm -f /ce/settings/*");
    Utils::forceSync();                                     // tell system to flush the filesystem caches
}

