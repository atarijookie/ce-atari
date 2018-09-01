// vim: expandtab shiftwidth=4 tabstop=4
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>

#include <algorithm>

#include "../global.h"
#include "../debug.h"
#include "../native/scsi_defs.h"
#include "../datatrans.h"
#include "../mounter.h"
#include "../translated/translateddisk.h"
#include "../periodicthread.h"  // for SharedObjects

#include "../settings.h"
#include "../utils.h"
#include "../update.h"
#include "../downloader.h"
#include "../ikbd/keybjoys.h"

#include "keys.h"
#include "configstream.h"
#include "netsettings.h"

extern volatile bool do_timeSync;
extern volatile bool do_loadIkbdConfig;

extern THwConfig hwConfig;
extern const char *distroString;

extern RPiConfig rpiConfig;             // RPi info structure

extern SharedObjects shared;

//--------------------------
// screen creation methods
void ConfigStream::createScreen_homeScreen(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();     // destroy current components
    screenChanged       = true; // mark that the screen has changed
    showingHomeScreen   = true; // mark that we're showing the home screen

    screen_addHeaderAndFooter(screen, "Main menu");

    ConfigComponent *comp;

    int line = 4;
    
    char idConfigLabel[32];
    Utils::IFintToStringFormatted(hwConfig.hddIface, idConfigLabel, " %s IDs config ");

    comp = new ConfigComponent(this, ConfigComponent::button, idConfigLabel,        18, 10, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_ACSI);
    screen.push_back(comp);
    line += 2;

    comp = new ConfigComponent(this, ConfigComponent::button, " Translated disks ",	18, 10, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_TRANSLATED);
    screen.push_back(comp);
    line += 2;

    comp = new ConfigComponent(this, ConfigComponent::button, " Hard Disk Image ", 18, 10, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_HDDIMAGE);
    screen.push_back(comp);
    line += 2;

    comp = new ConfigComponent(this, ConfigComponent::button, " Shared drive ",     18, 10, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_SHARED);
    screen.push_back(comp);
    line += 2;

    comp = new ConfigComponent(this, ConfigComponent::button, " Floppy config ",    18, 10, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_FLOPPY_CONF);
    screen.push_back(comp);
    line += 2;

    comp = new ConfigComponent(this, ConfigComponent::button, " Network settings ", 18, 10, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_NETWORK);
    screen.push_back(comp);
    line += 2;

    comp = new ConfigComponent(this, ConfigComponent::button, " IKBD ",             18, 10, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_IKBD);
    screen.push_back(comp);
    line += 2;

    comp = new ConfigComponent(this, ConfigComponent::button, " Other ",            18, 10, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_OTHER);
    screen.push_back(comp);
    line += 2;

    comp = new ConfigComponent(this, ConfigComponent::button, " Update software ",  18, 10, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_UPDATE);
    screen.push_back(comp);
    line += 2;

    setFocusToFirstFocusable();
}

void ConfigStream::createScreen_acsiConfig(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();				// destroy current components
    screenChanged		= true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    char idHeaderLabel[32];
    Utils::IFintToStringFormatted(hwConfig.hddIface, idHeaderLabel, " %s IDs config ");
    screen_addHeaderAndFooter(screen, idHeaderLabel);

    ConfigComponent *comp;

    comp = new ConfigComponent(this, ConfigComponent::label, "ID         off   sd    raw  ce_dd", 40, 0, 3, gotoOffset);
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

    comp = new ConfigComponent(this, ConfigComponent::label, "off   - turned off, not responding here",      40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "sd    - SD card               (only one)",    40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "raw   - raw sector access (use HDDr/ICD)",     40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "ce_dd - for booting CE_DD driver",	40, 0, row++, gotoOffset);
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

    screen_addHeaderAndFooter(screen, "Network settings");

    ConfigComponent *comp;

	int col0x = 3;
	int col1x = 6;
	int col2x = 19;
	
    // hostname setting
	int row = 3;
	
	comp = new ConfigComponent(this, ConfigComponent::label, "Hostname",	10,	col0x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",	15, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_HOSTNAME);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_ALLOW_NUMBERS);
    screen.push_back(comp);

    // DNS
    comp = new ConfigComponent(this, ConfigComponent::label, "DNS",			40, col0x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",	15, col2x, row, gotoOffset);
    comp->setComponentId(COMPID_NET_DNS);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    row += 2;
    
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

    comp = new ConfigComponent(this, ConfigComponent::label, "Enable",      10, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, " ",         1, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_ENABLE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "WPA SSID",	20,	col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "     ",	31, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_SSID);
    comp->setTextOptions(TEXT_OPTION_ALLOW_ALL);
    comp->setLimitedShowSize(15);               // limit to showing only 15 characters
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "WPA PSK",	20,         col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline_pass, "      ",	63, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_PSK);
    comp->setTextOptions(TEXT_OPTION_ALLOW_ALL);
    comp->setLimitedShowSize(15);               // limit to showing only 15 characters
    screen.push_back(comp);

    row++;

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

    setTextByComponentId(COMPID_HOSTNAME,       ns.hostname);
    setTextByComponentId(COMPID_NET_DNS,		ns.nameserver);

    setBoolByComponentId(COMPID_NET_DHCP,		ns.eth0.dhcpNotStatic);
    setTextByComponentId(COMPID_NET_IP,			ns.eth0.address);
    setTextByComponentId(COMPID_NET_MASK,		ns.eth0.netmask);
    setTextByComponentId(COMPID_NET_GATEWAY,	ns.eth0.gateway);

    setBoolByComponentId(COMPID_WIFI_ENABLE,    ns.wlan0.isEnabled);
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

    screen_addHeaderAndFooter(screen, "Translated disk");

    int col1x = 4, col2x = 24, col3x = 30;
    ConfigComponent *comp;

    int row = 3;
    comp = new ConfigComponent(this, ConfigComponent::label, "        Drive letters assignment", 40, 0, row++, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);
    row++;

    comp = new ConfigComponent(this, ConfigComponent::label, "First translated drive",  23, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",	                1, col3x + 3, row++, gotoOffset);
    comp->setComponentId(COMPID_TRAN_FIRST);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);
    row++;

    comp = new ConfigComponent(this, ConfigComponent::label, "Shared drive",            23, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",	                1, col3x + 3, row++, gotoOffset);
    comp->setComponentId(COMPID_TRAN_SHARED);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Config drive",            23, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",	                1, col3x + 3, row++, gotoOffset);
    comp->setComponentId(COMPID_TRAN_CONFDRIVE);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);
    row++;
    
    //------------
    comp = new ConfigComponent(this, ConfigComponent::label, "                 Options", 40, 0, row++, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);
    row++;

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
    
    comp = new ConfigComponent(this, ConfigComponent::label, "Access ZIP files as",							40, col1x, row, gotoOffset);
    screen.push_back(comp);
	
	comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",										3,	col2x, row, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_USE_ZIP_DIR_NOT_FILE, 0);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "files",  								        40,	col3x, row++, gotoOffset);
    screen.push_back(comp);
	
	comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",										3,	col2x, row, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_USE_ZIP_DIR_NOT_FILE, 1);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "dirs", 			                            40,	col3x, row++, gotoOffset);
    screen.push_back(comp);

	row++;
    //------------
    
    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ", 8,  9, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SAVE_TRANSLATED);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, 20, row++, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);
	row++;

    //------------
    comp = new ConfigComponent(this, ConfigComponent::label, "If you use also raw disks (Atari native ",    40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "disks), you should avoid using few",          40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "letters from C: to leave some space for",     40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "them.",	40, 0, row++, gotoOffset);
    screen.push_back(comp);
    row++;
    //------------

    // get the letters from settings, store them to components
    Settings s;
    char drive1, drive2, drive3;
    bool mountRawNotTrans;

    drive1 = s.getChar("DRIVELETTER_FIRST",      0);
    drive2 = s.getChar("DRIVELETTER_SHARED",     0);
    drive3 = s.getChar("DRIVELETTER_CONFDRIVE",  0);
    
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

    //---------
    mountRawNotTrans = s.getBool("MOUNT_RAW_NOT_TRANS", 0);

 	if(mountRawNotTrans) {
		checkboxGroup_setCheckedId(COMPID_MOUNT_RAW_NOT_TRANS, 1);			// select RAW
	} else {
		checkboxGroup_setCheckedId(COMPID_MOUNT_RAW_NOT_TRANS, 0);			// select TRANS
	}
    
    //---------
    bool useZipdirNotFile = s.getBool("USE_ZIP_DIR", 1);           // use ZIP DIRs, enabled by default

 	if(useZipdirNotFile) {
		checkboxGroup_setCheckedId(COMPID_USE_ZIP_DIR_NOT_FILE, 1);			// ZIP DIRs enabled
	} else {
		checkboxGroup_setCheckedId(COMPID_USE_ZIP_DIR_NOT_FILE, 0);			// ZIP DIRs disabled
	}
    //---------

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
        showMessageScreen("Warning", "Some ACSI/SCSI ID has no selected type.\n\rGo and select something!");
        return;
    }

    if(!somethingActive) {									// if everything is set to OFF
        showMessageScreen("Warning", "All ACSI/SCSI IDs are set to 'OFF',\n\rit is invalid and would brick the device.\n\rSelect at least one active ACSI/SCSI ID.");
        return;
    }

    if(tranCnt > 1) {										// more than 1 of this type?
        showMessageScreen("Warning", "You have more than 1 CE_DD selected.\n\rUnselect some to leave only\n\r1 active.");
        return;
    }

    if(sdCnt > 1) {										    // more than 1 of this type?
        showMessageScreen("Warning", "You have more than 1 SD cards\n\rselected. Unselect some to leave only\n\r1 active.");
        return;
    }

    if(hwConfig.hddIface == HDD_IF_SCSI) {                         // running on SCSI? Show warning if ID 0 or 7 is used
        if(devTypes[0] != DEVTYPE_OFF || devTypes[7] != DEVTYPE_OFF) {
            showMessageScreen("Warning", "You assigned something to ID 0 or ID 7.\n\rThey might not work as they might be\n\rused by SCSI controller.\n\r");
        }
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
        showMessageScreen("Info", "No drive letter assigned, this is OK,\n\rbut the translated disk will be\n\runaccessible.");
    }

    if(letter1 == letter2 || letter1 == letter3 || letter2 == letter3) {
        showMessageScreen("Warning", "Drive letters must be different!\n\rPlease fix this and try again.");
        return;
    }

    if((letter1 != -1 && letter1 < 'C') || (letter2 != -1 && letter2 < 'C') || (letter3 != -1 && letter3 < 'C')) {
        showMessageScreen("Warning", "Drive letters A and B are for floppies.\n\rPlease fix this and try again.");
        return;
    }

    if(letter1 > 'P' || letter2 > 'P' || letter3 > 'P') {
        showMessageScreen("Warning", "Last allowed drive letter is 'P'.\n\rPlease fix this and try again.");
        return;
    }

    bool mountRawNotTrans = (bool) checkboxGroup_getCheckedId(COMPID_MOUNT_RAW_NOT_TRANS);
    bool useZipdirNotFile = (bool) checkboxGroup_getCheckedId(COMPID_USE_ZIP_DIR_NOT_FILE);

    //---------
    // now save the settings
    Settings s;
    s.setChar("DRIVELETTER_FIRST",      letter1);
    s.setChar("DRIVELETTER_SHARED",     letter2);
    s.setChar("DRIVELETTER_CONFDRIVE",  letter3);
    s.setBool("MOUNT_RAW_NOT_TRANS",    mountRawNotTrans);
    s.setBool("USE_ZIP_DIR",            useZipdirNotFile); // use ZIP DIRs, enabled by default

    if(reloadProxy) {                                       // if got settings reload proxy, invoke reload
        reloadProxy->reloadSettings(SETTINGSUSER_TRANSLATED);
    }

    Utils::forceSync();                                     // tell system to flush the filesystem caches

    createScreen_homeScreen();		// now back to the home screen
}

void ConfigStream::onNetwork_save(void)
{
	// for eth0
    std::string eIp, eMask, eGateway;
    bool eUseDhcp;

	// for wlan0
    std::string wIp, wMask, wGateway;
    bool wUseDhcp, wifiIsEnabled;
	
	std::string dns, hostname;

    // read the settings from components
    getBoolByComponentId(COMPID_NET_DHCP,		eUseDhcp);
    getTextByComponentId(COMPID_NET_IP,			eIp);
    getTextByComponentId(COMPID_NET_MASK,		eMask);
    getTextByComponentId(COMPID_NET_GATEWAY,	eGateway);

    getBoolByComponentId(COMPID_WIFI_ENABLE,    wifiIsEnabled);
    getBoolByComponentId(COMPID_WIFI_DHCP,		wUseDhcp);
    getTextByComponentId(COMPID_WIFI_IP,		wIp);
    getTextByComponentId(COMPID_WIFI_MASK,		wMask);
    getTextByComponentId(COMPID_WIFI_GATEWAY,	wGateway);

    getTextByComponentId(COMPID_NET_DNS,		dns);
    getTextByComponentId(COMPID_HOSTNAME,       hostname);
    
    if(hostname.empty()) {
        hostname = "CosmosEx";
    }

    if(!eUseDhcp || !wUseDhcp) {    // if ethernet or wifi doesn't use DHCP, we must receive also DNS settings
        bool a = verifyAndFixIPaddress(dns, dns, false);
        
        if(!a) {
            showMessageScreen("Warning", "If ethermet or wifi doesn't use DHCP,\n\ryou must specify a valid DNS!\n\rPlease fix this and try again.");
            return;
        }
    }

    // verify the settings for eth0
    if(!eUseDhcp) {             // but verify settings only when not using dhcp
        bool a,b,c;

        a = verifyAndFixIPaddress(eIp,      eIp,        false);
        b = verifyAndFixIPaddress(eMask,    eMask,      false);
        c = verifyAndFixIPaddress(eGateway, eGateway,   false);

        if(!a || !b || !c) {
            showMessageScreen("Warning", "Some ethernet network address\n\rhas invalid format or is empty.\n\rPlease fix this and try again.");
            return;
        }
    }

    if(!wUseDhcp) {             // but verify settings only when not using dhcp
        bool a,b,c;

        a = verifyAndFixIPaddress(wIp,       wIp,         false);
        b = verifyAndFixIPaddress(wMask,     wMask,       false);
        c = verifyAndFixIPaddress(wGateway,  wGateway,    false);

        if(!a || !b || !c) {
            showMessageScreen("Warning", "Some wifi network address\n\rhas invalid format or is empty.\n\rPlease fix this and try again.");
            return;
        }
    }

    //-------------------------
    // store the settings
    NetworkSettings nsNew, nsOld;
    nsNew.load();                       // load the current values
    nsOld.load();                       // load the current values

    nsNew.nameserver   = dns;
    nsNew.hostname     = hostname;

    nsNew.eth0.dhcpNotStatic = eUseDhcp;

    if(!eUseDhcp) {                     // if not using dhcp, store also the network settings
        nsNew.eth0.address = eIp;
        nsNew.eth0.netmask = eMask;
        nsNew.eth0.gateway = eGateway;
    }

    nsNew.wlan0.isEnabled = wifiIsEnabled;

    getTextByComponentId(COMPID_WIFI_SSID,  nsNew.wlan0.wpaSsid);
    getTextByComponentId(COMPID_WIFI_PSK,   nsNew.wlan0.wpaPsk);

    nsNew.wlan0.dhcpNotStatic = wUseDhcp;

    if(!wUseDhcp) {                     // if not using dhcp, store also the network settings
        nsNew.wlan0.address = wIp;
        nsNew.wlan0.netmask = wMask;
        nsNew.wlan0.gateway = wGateway;
    }

    //-------------------------
    // check if some network setting changed, and do save and restart network (otherwise just ignore it)
    if(nsNew.isDifferentThan(nsOld)) {
        bool eth0IsDifferent    = nsNew.eth0IsDifferentThan(nsOld);
        bool wlan0IsDifferent   = nsNew.wlan0IsDifferentThan(nsOld);

        nsNew.save();                   // store the new values
        Utils::forceSync();             // tell system to flush the filesystem caches

        if(eth0IsDifferent) {           // restart eth0 if needed
            TMounterRequest tmr;
            tmr.action = MOUNTER_ACTION_RESTARTNETWORK_ETH0;
            Mounter::add(tmr);
        }

        if(wlan0IsDifferent) {          // restart wlan0 if needed
            TMounterRequest tmr;
            tmr.action = MOUNTER_ACTION_RESTARTNETWORK_WLAN0;
            Mounter::add(tmr);
        }
    }

    //-------------------------
    createScreen_homeScreen();		// now back to the home screen
}

void ConfigStream::createScreen_update(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();			// destroy current components
    screenChanged	= true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, "Software & Firmware updates");

    updateFromWebNotUsb = true;         // do update from web
    
    ConfigComponent *comp;

    int cl1 = 5;
    int cl2 = 27;
    
    int line = 4;
    comp = new ConfigComponent(this, ConfigComponent::label, "Hardware version  : ", 22, cl1, line, gotoOffset);
    screen.push_back(comp);

    const char *hwVer = (hwConfig.version == 2) ? "v. 2" : "v. 1";

    comp = new ConfigComponent(this, ConfigComponent::label, hwVer, 10, cl2, line, gotoOffset);
    screen.push_back(comp);
    line++;

    comp = new ConfigComponent(this, ConfigComponent::label, "HDD interface type: ", 22, cl1, line, gotoOffset);
    screen.push_back(comp);

    const char *hddIf = Utils::IFintToString(hwConfig.hddIface);

    comp = new ConfigComponent(this, ConfigComponent::label, hddIf, 10, cl2, line, gotoOffset);
    screen.push_back(comp);
    line++;
    
    //-------
    comp = new ConfigComponent(this, ConfigComponent::label, "Linux distribution: ", 22, cl1, line, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, distroString, 10, cl2, line, gotoOffset);
    screen.push_back(comp);
    
    line++;
    //-------
    #ifndef DISTRO_YOCTO
    line++;
    cl1 = 2;
    cl2 = 17;
    #endif

    #ifdef DISTRO_YOCTO
    comp = new ConfigComponent(this, ConfigComponent::label, "RPi revision      : ", 22, cl1, line, gotoOffset);
    #else
    comp = new ConfigComponent(this, ConfigComponent::label, "RPi revision: ", 22, cl1, line, gotoOffset);
    #endif
    
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, rpiConfig.revision, 20, cl2, line, gotoOffset);
    screen.push_back(comp);
    
    line++;
    //-------
    #ifndef DISTRO_YOCTO
    comp = new ConfigComponent(this, ConfigComponent::label, rpiConfig.model, 40, cl1, line, gotoOffset);
    screen.push_back(comp);
    #endif

    line += 2;
    //-------
    
    comp = new ConfigComponent(this, ConfigComponent::label, " part       your version   ", 27, 0, line, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "on web", 13, 27, line, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_LOCATION);
    comp->setReverse(true);
    screen.push_back(comp);
    
    line += 2;

	int col1 = 1, col2 = 13;
		
    comp = new ConfigComponent(this, ConfigComponent::label, "Main App", 12,	col1, line, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 26,			col2, line, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_COSMOSEX);
    screen.push_back(comp);
    line++;

    comp = new ConfigComponent(this, ConfigComponent::label, "Franz", 12, 		col1, line, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 26, 			col2, line, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_FRANZ);
    screen.push_back(comp);
    line++;

    comp = new ConfigComponent(this, ConfigComponent::label, "Hans", 12,		col1, line, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 26,			col2, line, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_HANZ);
    screen.push_back(comp);
    line++;

    comp = new ConfigComponent(this, ConfigComponent::label, "Xilinx", 12,		col1, line, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 26,			col2, line, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_XILINX);
    screen.push_back(comp);
    
    line += 2;

    comp = new ConfigComponent(this, ConfigComponent::button, " From web ", 10,  6, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_UPDATE_CHECK);
    comp->setComponentId(COMPID_UPDATE_BTN_CHECK);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " From USB ", 10,  22, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_UPDATE_CHECK_USB);
    comp->setComponentId(COMPID_UPDATE_BTN_CHECK_USB);
    screen.push_back(comp);
    line++;

    comp = new ConfigComponent(this, ConfigComponent::button, "  Update  ", 10,  6, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_UPDATE_UPDATE);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Cancel  ", 10,  22, line, gotoOffset);
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
        showMessageScreen("Update from USB", "File ce_update.zip not found.\n\rCan't update from USB.\n\r");
        return;
    }
    
    // copy and unzip the update
    Update::downloadUpdateList(pathToUpdateFile.c_str());
    
    Update::versions.updateListWasProcessed = false;            // mark that the new update list wasn't updated
}

void ConfigStream::onUpdateUpdate(void)
{
    if(shownOn == CONFIGSTREAM_IN_LINUX_CONSOLE) {               // when trying to do update from from linux console
        showMessageScreen("STOP!", "Don't update the firmware using\n\rlinux console config tool!\n\r\n\rInstead run /ce/ce_update.sh or\n\rdo it using CE_CONF.TOS on Atari.");
        return;
    }

    if(shownOn == CONFIGSTREAM_THROUGH_WEB) {                   // when trying to do update from from web console
        showMessageScreen("STOP!", "Don't update the firmware using\n\rweb interface config tool!\n\r\n\rInstead run /ce/ce_update.sh or\n\rdo it using CE_CONF.TOS on Atari.");
        return;
    }

    if(!Update::versions.updateListWasProcessed) {              // didn't process the update list yet? show message
        showMessageScreen("No updates info", "No update info was downloaded,\n\rplease press web / usb button and wait.");
        return;
    }

    if(!Update::versions.gotUpdate) {
        showMessageScreen("No update needed", "All your components are up to date.");
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

    screen_addHeaderAndFooter(screen, "Download of update");

    ConfigComponent *comp;

    int col = 7;
    int row = 9;

    comp = new ConfigComponent(this, ConfigComponent::label, "Downloading",                 40, col, row, gotoOffset);
    comp->setComponentId(COMPID_DL_TITLE);
    screen.push_back(comp);

    row += 2;

    comp = new ConfigComponent(this, ConfigComponent::label, "ce_update.zip          0 %",  30, col, row, gotoOffset);
    comp->setComponentId(COMPID_DL1);
    screen.push_back(comp);

    row += 2;

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ",                    8, col, row, gotoOffset);
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

    screen_addHeaderAndFooter(screen, "Other settings");

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
    comp = new ConfigComponent(this, ConfigComponent::label, "Screen res in DESKTOP.INF",  40, col, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Low",                         40, col + 2, row, gotoOffset);
    screen.push_back(comp);
    
    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                       3, col2, row++, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_SCREEN_RESOLUTION, 1);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "mid",                         40, col + 2, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                       3, col2, row++, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_SCREEN_RESOLUTION, 2);
    screen.push_back(comp);

    //----------------------
    row++;

/*
    comp = new ConfigComponent(this, ConfigComponent::button, " Send your settings to Jookie ", 30, 5, row++, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SEND_SETTINGS);
    screen.push_back(comp);
*/

    comp = new ConfigComponent(this, ConfigComponent::button, " Reset all settings ",       19, 10, row++, gotoOffset);
    comp->setOnEnterFunctionCode(CS_RESET_SETTINGS);
    screen.push_back(comp);
    //----------------------
    
    row++;
    comp = new ConfigComponent(this, ConfigComponent::button, "   Save   ",                 10,  6, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_OTHER_SAVE);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Cancel  ",                 10,  22, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    //------------------------
    Settings s;
    
    bool        setDateTime;
    float       utcOffset;
    std::string ntpServer;
    int			frameSkip;
    int         screenRes;
    
    setDateTime = s.getBool     ("TIME_SET",             true);
    utcOffset   = s.getFloat    ("TIME_UTC_OFFSET",      0);
    ntpServer   = s.getString   ("TIME_NTP_SERVER",      "200.20.186.76");
    frameSkip   = s.getInt      ("SCREENCAST_FRAMESKIP", 20);
    screenRes   = s.getInt      ("SCREEN_RESOLUTION",    1);
    
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
    checkboxGroup_setCheckedId(COMPID_SCREEN_RESOLUTION, screenRes);
    //------------------------
    
    setFocusToFirstFocusable();
}

void ConfigStream::onOtherSave(void)
{
    Settings s;

    bool        setDateTime = false;
    float       utcOffset   = 0;
    std::string ntpServer;
    int			frameSkip;
    int         screenRes;
    
    getBoolByComponentId(COMPID_TIMESYNC_ENABLE,        setDateTime);
    getFloatByComponentId(COMPID_TIMESYNC_UTC_OFFSET,   utcOffset);
    getTextByComponentId(COMPID_TIMESYNC_NTP_SERVER,    ntpServer);
    getIntByComponentId(COMPID_SCREENCAST_FRAMESKIP,    frameSkip);
    screenRes = checkboxGroup_getCheckedId(COMPID_SCREEN_RESOLUTION);
    
    if( frameSkip<10 )
    {
        frameSkip=10;
    }
    if( frameSkip>255 )
    {
        frameSkip=255;
    }
    
    s.setBool     ("TIME_SET",             setDateTime);
    s.setFloat    ("TIME_UTC_OFFSET",      utcOffset);
    s.setString   ("TIME_NTP_SERVER",      ntpServer.c_str());
    s.setInt      ("SCREENCAST_FRAMESKIP", frameSkip);
    s.setInt      ("SCREEN_RESOLUTION",    screenRes);

    do_timeSync         = true;     // do time sync again

    Utils::forceSync();             // tell system to flush the filesystem caches
    Utils::setTimezoneVariable_inProfileScript();   // create the timezone setting script, because TIME_UTC_OFFSET could possibly change
    Utils::setTimezoneVariable_inThisContext();     // and also set the TZ variable for this context, so the change for this app would be immediate
    
    createScreen_homeScreen();		// now back to the home screen
}

void ConfigStream::createScreen_ikbd(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();			    // destroy current components
    screenChanged	    = true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, "IKBD settings");

    ConfigComponent *comp;

    int row     = 3;
    int col     = 2;
    int col2    = 33;
    
    //-----------
    comp = new ConfigComponent(this, ConfigComponent::label, "Attach 1st joy as JOY 0",     40, col, row, gotoOffset);
    screen.push_back(comp);
    
    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                      3, col2, row++, gotoOffset);
    comp->setComponentId(COMPID_JOY0_FIRST);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "(hotkey: CTRL+any SHIFT+HELP/F11)",     40, col, row, gotoOffset);
    screen.push_back(comp);

    //----------------------

    row += 2;
    comp = new ConfigComponent(this, ConfigComponent::label, "Mouse wheel as arrow UP / DOWN", 40, col, row, gotoOffset);
    screen.push_back(comp);
    
    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                          3, col2, row++, gotoOffset);
    comp->setComponentId(COMPID_MOUSEWHEEL_ENABLED);
    screen.push_back(comp);

    //----------------------

    int colButton   = col + 0;
    int colLeft     = col + 0;
    int colUpDown   = col + 12;
    int colRight    = col + 24;

    row += 2;
    comp = new ConfigComponent(this, ConfigComponent::label, "Keyboard Joy 0 enabled",         40, col, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                          3, col2, row++, gotoOffset);
    comp->setComponentId(COMPID_KEYB_JOY0);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "(hotkey: CTRL+LSHIFT+UNDO/F12)",     40, col, row, gotoOffset);
    screen.push_back(comp);

    row += 2;
    // button
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",	              10, colButton, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY0_BUTTON);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    // up
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",	              10, colUpDown, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY0_UP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    row++;
    // left
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",	              10, colLeft, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY0_LEFT);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    // down
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",	              10, colUpDown, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY0_DOWN);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    // right
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",	              10, colRight, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY0_RIGHT);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    //----------------------

    row += 3;
    comp = new ConfigComponent(this, ConfigComponent::label, "Keyboard Joy 1 enabled",         40, col, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                          3, col2, row++, gotoOffset);
    comp->setComponentId(COMPID_KEYB_JOY1);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "(hotkey: CTRL+RSHIFT+UNDO/F12)",     40, col, row, gotoOffset);
    screen.push_back(comp);

    row += 2;
    // button
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",	              10, colButton, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY1_BUTTON);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    // up
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",	              10, colUpDown, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY1_UP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    row++;
    // left
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",	              10, colLeft, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY1_LEFT);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    // down
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",	              10, colUpDown, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY1_DOWN);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    // right
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",	              10, colRight, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY1_RIGHT);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);
    
    //----------------------
    
    row += 2;
    comp = new ConfigComponent(this, ConfigComponent::button, "   Save   ",                 10,  6, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_IKBD_SAVE);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Cancel  ",                 10,  22, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    //------------------------
    Settings s;
    
    bool joy0First;
    bool mouseWheelEnabled;
    bool keybJoy0, keybJoy1;
    
    joy0First           = s.getBool("JOY_FIRST_IS_0",       false);
    mouseWheelEnabled   = s.getBool("MOUSE_WHEEL_AS_KEYS",  true);
    keybJoy0            = s.getBool("KEYBORD_JOY0",         false);
    keybJoy1            = s.getBool("KEYBORD_JOY1",         false);
    
    setBoolByComponentId(COMPID_JOY0_FIRST,         joy0First);
    setBoolByComponentId(COMPID_MOUSEWHEEL_ENABLED, mouseWheelEnabled);
    setBoolByComponentId(COMPID_KEYB_JOY0,          keybJoy0);
    setBoolByComponentId(COMPID_KEYB_JOY1,          keybJoy1);
    
    //------------------------
    // now fill the keyb joys config
    KeybJoyKeys     keyJoyKeys;
    KeyTranslator   keyTranslator;
    
    keyJoyKeys.setKeyTranslator(&keyTranslator);        // first set the translator
    keyJoyKeys.loadKeys();                              // then load the keys

    std::string joyString;

    joyString = keyJoyKeys.joyKeys[0].human.button;
    setTextByComponentId(COMPID_KEYBJOY0_BUTTON, joyString);
    
    joyString = keyJoyKeys.joyKeys[0].human.left;
    setTextByComponentId(COMPID_KEYBJOY0_LEFT, joyString);

    joyString = keyJoyKeys.joyKeys[0].human.right;
    setTextByComponentId(COMPID_KEYBJOY0_RIGHT, joyString);

    joyString = keyJoyKeys.joyKeys[0].human.up;
    setTextByComponentId(COMPID_KEYBJOY0_UP, joyString);

    joyString = keyJoyKeys.joyKeys[0].human.down;
    setTextByComponentId(COMPID_KEYBJOY0_DOWN, joyString);

    //----------
    joyString = keyJoyKeys.joyKeys[1].human.button;
    setTextByComponentId(COMPID_KEYBJOY1_BUTTON, joyString);
    
    joyString = keyJoyKeys.joyKeys[1].human.left;
    setTextByComponentId(COMPID_KEYBJOY1_LEFT, joyString);

    joyString = keyJoyKeys.joyKeys[1].human.right;
    setTextByComponentId(COMPID_KEYBJOY1_RIGHT, joyString);

    joyString = keyJoyKeys.joyKeys[1].human.up;
    setTextByComponentId(COMPID_KEYBJOY1_UP, joyString);

    joyString = keyJoyKeys.joyKeys[1].human.down;
    setTextByComponentId(COMPID_KEYBJOY1_DOWN, joyString);
    //------------------------
    
    setFocusToFirstFocusable();
}

void ConfigStream::onIkbdSave(void)
{
    Settings s;

    bool joy0First          = false;
    bool mouseWheelEnabled  = true;
    bool keybJoy0           = false;
    bool keybJoy1           = false;
    
    //------------------------
    // now get the keyb joys config
    KeybJoyKeys     keyJoyKeys;
    KeyTranslator   keyTranslator;
    
    keyJoyKeys.setKeyTranslator(&keyTranslator);        // first set the translator
    keyJoyKeys.loadKeys();                              // then load the keys

    std::string joyString;

    //----------
    // get settings for joy 0 
    getTextByComponentId(COMPID_KEYBJOY0_BUTTON, joyString);
    keyJoyKeys.joyKeys[0].human.button = joyString;
    
    getTextByComponentId(COMPID_KEYBJOY0_LEFT, joyString);
    keyJoyKeys.joyKeys[0].human.left = joyString;

    getTextByComponentId(COMPID_KEYBJOY0_RIGHT, joyString);
    keyJoyKeys.joyKeys[0].human.right = joyString;

    getTextByComponentId(COMPID_KEYBJOY0_UP, joyString);
    keyJoyKeys.joyKeys[0].human.up = joyString;

    getTextByComponentId(COMPID_KEYBJOY0_DOWN, joyString);
    keyJoyKeys.joyKeys[0].human.down = joyString;

    //----------
    // get settings for joy 1
    getTextByComponentId(COMPID_KEYBJOY1_BUTTON, joyString);
    keyJoyKeys.joyKeys[1].human.button = joyString;
    
    getTextByComponentId(COMPID_KEYBJOY1_LEFT, joyString);
    keyJoyKeys.joyKeys[1].human.left = joyString;

    getTextByComponentId(COMPID_KEYBJOY1_RIGHT, joyString);
    keyJoyKeys.joyKeys[1].human.right = joyString;

    getTextByComponentId(COMPID_KEYBJOY1_UP, joyString);
    keyJoyKeys.joyKeys[1].human.up = joyString;

    getTextByComponentId(COMPID_KEYBJOY1_DOWN, joyString);
    keyJoyKeys.joyKeys[1].human.down = joyString;
    
    //----------
    // validate new settings
    if(!keyJoyKeys.keybJoyHumanSettingsValidForSingleJoy(0)) {  // keyb joy 0 invalid?
        showMessageScreen("Warning", "Keyboard Joy 0 settings are invalid!\n\rPlease fix this and try again.");
        return;
    }
    
    if(!keyJoyKeys.keybJoyHumanSettingsValidForSingleJoy(1)) {  // keyb joy 1 invalid?
        showMessageScreen("Warning", "Keyboard Joy 1 settings are invalid!\n\rPlease fix this and try again.");
        return;
    }
    
    if(!keyJoyKeys.keybJoyHumanSettingsValidBetweenJoys()) {    // keyb joy 0 + joy 1 invalid when used together?
        showMessageScreen("Warning", "Keyboard Joy 0 and Joy 1 settings invalid!\n\rPlease fix this and try again.");
        return;
    }
    
    keyJoyKeys.saveKeys();              // save settings
    //------------------------
    
    getBoolByComponentId(COMPID_JOY0_FIRST,         joy0First);
    getBoolByComponentId(COMPID_MOUSEWHEEL_ENABLED, mouseWheelEnabled);
    getBoolByComponentId(COMPID_KEYB_JOY0,          keybJoy0);
    getBoolByComponentId(COMPID_KEYB_JOY1,          keybJoy1);

    s.setBool("JOY_FIRST_IS_0",        joy0First);
    s.setBool("MOUSE_WHEEL_AS_KEYS",   mouseWheelEnabled);
    s.setBool("KEYBORD_JOY0",          keybJoy0);
    s.setBool("KEYBORD_JOY1",          keybJoy1);
    
    do_loadIkbdConfig   = true;     // reload ikbd config

    createScreen_homeScreen();		// now back to the home screen
}

void ConfigStream::fillUpdateDownloadWithProgress(void)
{
    std::string status, l1, l2, l3, l4;

    // get the current download status
    Downloader::status(status, DWNTYPE_UPDATE_COMP);

    // split it to lines
    getProgressLine(0, status, l1);

    // set it to components
    setTextByComponentId(COMPID_DL1, l1);
}

void ConfigStream::fillUpdateDownloadWithFinish(void)
{
    std::string s1 = "Update downloaded.";
    std::string s2 = "Starting install...";

    setTextByComponentId(COMPID_DL_TITLE, s1);
    setTextByComponentId(COMPID_DL1, s2);
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
    showMessageScreen("Update download fail", "Failed to download the update,\nplease try again later.");
}

void ConfigStream::showUpdateError(void)
{
    // check if we're on update download page
    if(!isUpdateDownloadPageShown()) {
        return;
    }

    // ok, so we're on update donload page... go back to update page and show error
    createScreen_update();
    showMessageScreen("Update fail", "Failed to do the update.\n");
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

void ConfigStream::createScreen_hddimage(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();			// destroy current components
    screenChanged	= true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, "Disk Image settings");

    ConfigComponent *comp;

    int row = 3;

    // HDD Image path
    comp = new ConfigComponent(this, ConfigComponent::label, "HDD Image path on RPi",
                               40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",
                               255, 0, row++, gotoOffset);
    comp->setLimitedShowSize(38);   /* only show 38 characters */
    comp->setComponentId(COMPID_HDDIMAGE_PATH);
    screen.push_back(comp);

    row++;

    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ",
                               8, 3, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_HDDIMAGE_SAVE);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ",
                               8, 15, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Clear ",
                               8, 27, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_HDDIMAGE_CLEAR);
    comp->setComponentId(COMPID_BTN_CLEAR);
    screen.push_back(comp);
    row += 2;

    comp = new ConfigComponent(this, ConfigComponent::label, "Enter here full path to .IMG file. Path",
                               40, 0, row++, gotoOffset);
    screen.push_back(comp);
    comp = new ConfigComponent(this, ConfigComponent::label, "beginning with shared or usb will be",
                               40, 0, row++, gotoOffset);
    screen.push_back(comp);
    comp = new ConfigComponent(this, ConfigComponent::label, "autocompleted. * wildcards are supported",
                               40, 0, row++, gotoOffset);
    screen.push_back(comp);
    comp = new ConfigComponent(this, ConfigComponent::label, "HDD Image will be mounted as RAW disk.  ",
                               40, 0, row++, gotoOffset);
    screen.push_back(comp);
    comp = new ConfigComponent(this, ConfigComponent::label, "Ensure you have at least one ACSI ID",
                               40, 0, row++, gotoOffset);
    screen.push_back(comp);
    comp = new ConfigComponent(this, ConfigComponent::label, "configured as raw.",
                               40, 0, row++, gotoOffset);
    screen.push_back(comp);
    row++;
    comp = new ConfigComponent(this, ConfigComponent::label, "  Mounting is even easier from Atari :  ",
                               40, 0, row++, gotoOffset);
    screen.push_back(comp);
    comp = new ConfigComponent(this, ConfigComponent::label, "Double-click on .IMG on translated drive",
                               40, 0, row++, gotoOffset);
    screen.push_back(comp);
    comp = new ConfigComponent(this, ConfigComponent::label, "to mount image using CE_HDIMG.TTP tool.",
                               40, 0, row++, gotoOffset);
    screen.push_back(comp);

    Settings s;
    std::string path;

    path = s.getString("HDDIMAGE", "");
    setTextByComponentId(COMPID_HDDIMAGE_PATH, path);

    setFocusToFirstFocusable();
}

void ConfigStream::createScreen_shared(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();			// destroy current components
    screenChanged	= true;			// mark that the screen has changed
    showingHomeScreen	= false;		// mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, "Shared drive settings");

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

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",										40, col2x, row, gotoOffset);
    comp->setComponentId(COMPID_USERNAME);
    comp->setLimitedShowSize(20);               // limit to showing only 20 characters
    screen.push_back(comp);

	row++;

    comp = new ConfigComponent(this, ConfigComponent::label, "Password",				                    40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline_pass, " ",									40, col2x, row, gotoOffset);
    comp->setComponentId(COMPID_PASSWORD);
    comp->setLimitedShowSize(20);               // limit to showing only 20 characters
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

    addr = s.getString("SHARED_ADDRESS",  "");
    path = s.getString("SHARED_PATH",     "");

    username = s.getString("SHARED_USERNAME",  "");
    password = s.getString("SHARED_PASSWORD",  "");

	enabled		= s.getBool("SHARED_ENABLED",			false);
	nfsNotSamba	= s.getBool("SHARED_NFS_NOT_SAMBA",	true);

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

void ConfigStream::onHddImageSave(void)
{
    struct stat st;
    std::string path;

    getTextByComponentId(COMPID_HDDIMAGE_PATH, path);

    if(path.substr(0, 3) == "usb") {
        std::string subpath = path.substr(3);
        for(int i=MAX_DRIVES-1; i>= 2; i--) {
            if(TranslatedDisk::getInstance()->driveIsEnabled(i)) {
                const char * rootpath = TranslatedDisk::getInstance()->driveGetHostPath(i);
                if(rootpath) {
                    path = rootpath + subpath;
                    if(stat(path.c_str(), &st) >= 0) {
                        // file exists
                        break;
                    }
                }
            }
        }
    } else if(path.substr(0, 6) == "shared") {
        path = "/mnt" + path;
    }

    if(!path.empty() && path.find("*") != std::string::npos) {
        glob_t g;
        memset(&g, 0, sizeof(g));
        if(glob(path.c_str(), GLOB_NOSORT, NULL, &g) == 0) {
            char msg[512];
            snprintf(msg, sizeof(msg), "Resolved path %s\r\nto %s", path.c_str(), g.gl_pathv[0]);
            showMessageScreen("Info", msg);
            path = g.gl_pathv[0];
        }
        globfree(&g);
    }

    if(!path.empty()) {
        if(stat(path.c_str(), &st) < 0) {
            showMessageScreen("Warning", "Cannot access file.\n\rPlease fix this and try again.");
            return;
        } else {
            if(!S_ISREG(st.st_mode)) {
                showMessageScreen("Warning", "File is not regular file.\n\rPlease fix this and try again.");
                return;
            }
        }
    }
    if(path.find("/mnt/shared/") == 0) {
        showMessageScreen("Warning", "It is not safe to mount HDD Image from\r\nnetwork.");
    }
    Settings s;
    s.setString("HDDIMAGE", path.c_str());

    if(reloadProxy) {                                       // if got settings reload proxy, invoke reload
        reloadProxy->reloadSettings(SETTINGSUSER_ACSI);     // reload SCSI / ACSI settings
    }

    Utils::forceSync();                                     // tell system to flush the filesystem caches

    createScreen_homeScreen();      // now back to the home screen
}

void ConfigStream::onHddImageClear(void)
{
    std::string path("");
    setTextByComponentId(COMPID_HDDIMAGE_PATH, path);
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
			showMessageScreen("Warning", "Server address seems to be invalid.\n\rPlease fix this and try again.");
			return;
		}

		if(path.length() < 1) {
			showMessageScreen("Warning", "Path for server is empty.\n\rPlease fix this and try again.");
			return;
		}
	}
    
    #ifndef DISTRO_YOCTO
    // on Raspbian - warn about NFS
    if(nfsNotSamba) {
        showMessageScreen("Warning", "You are using NFS on Raspbian, which\n\ris currently not working well on\n\rRaspbian and causes system to hang when\n\rnetwork connection is bad or slow.\n\rPlease consider using CIFS instead.");
    }    
    #endif

    std::replace( path.begin(), path.end(), '\\', '/');  // replace all slashes for the right slashes

    Settings s;

	s.setBool	("SHARED_ENABLED",			enabled);
	s.setBool	("SHARED_NFS_NOT_SAMBA",	nfsNotSamba);
    s.setString	("SHARED_ADDRESS",  		ip.c_str());
    s.setString	("SHARED_PATH",     		path.c_str());

    s.setString	("SHARED_USERNAME",  		username.c_str());
    s.setString	("SHARED_PASSWORD",     	password.c_str());

    if(reloadProxy) {                                       // if got settings reload proxy, invoke reload
        reloadProxy->reloadSettings(SETTINGSUSER_SHARED);
	}

    Utils::forceSync();                                     // tell system to flush the filesystem caches

    createScreen_homeScreen();		// now back to the home screen
}

void ConfigStream::onResetSettings(void)
{
    createScreen_homeScreen();		// now back to the home screen

    showMessageScreen("Reset all settings", "All settings have been reset to default.\n\rReseting your ST might be a good idea...");

    system("rm -f /ce/settings/*");
    Utils::forceSync();                                     // tell system to flush the filesystem caches
}

void ConfigStream::onSendSettings(void)
{
    ConfigStream cs(CONFIGSTREAM_THROUGH_WEB);
    cs.createConfigDump();
    
    // add request for download of the update list
    TDownloadRequest tdr;
    
    tdr.srcUrl          = CONFIG_TEXT_FILE;
    tdr.dstDir          = "http://joo.kie.sk/cosmosex/sendconfig/sendconfig.php";
    tdr.downloadType    = DWNTYPE_SEND_CONFIG;
    tdr.checksum        = 0;                        // special case - don't check checsum
    tdr.pStatusByte     = NULL;                     // don't update this status byte
    Downloader::add(tdr);
    
    showMessageScreen("Send config", "Sending your configuration to Jookie.\n\rThis will take a while.\n\rInternet connection needed.");    
}


