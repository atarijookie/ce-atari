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
#include "../acsidatatrans.h"
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

extern TFlags    flags;                 // global flags from command line
extern THwConfig hwConfig;
extern const char *distroString;

extern RPiConfig rpiConfig;             // RPi info structure

extern SharedObjects shared;

extern BYTE isUpdateStartingFlag;           // set to 1 to notify ST that the update is starting and it should show 'fake updating progress' screen
extern volatile DWORD whenCanStartInstall;  // if this DWORD has non-zero value, then it's a time when this app should be terminated for installing update

void ConfigStream::createScreen_translated(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();             // destroy current components
    screenChanged       = true;         // mark that the screen has changed
    showingHomeScreen   = false;        // mark that we're NOT showing the home screen

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

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",                    1, col3x + 3, row++, gotoOffset);
    comp->setComponentId(COMPID_TRAN_FIRST);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);
    row++;

    comp = new ConfigComponent(this, ConfigComponent::label, "Shared drive",            23, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",                    1, col3x + 3, row++, gotoOffset);
    comp->setComponentId(COMPID_TRAN_SHARED);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Config drive",            23, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",                    1, col3x + 3, row++, gotoOffset);
    comp->setComponentId(COMPID_TRAN_CONFDRIVE);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);
    row++;

    //------------
    comp = new ConfigComponent(this, ConfigComponent::label, "                 Options", 40, 0, row++, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);
    row++;

    comp = new ConfigComponent(this, ConfigComponent::label, "Mount USB media as",                          40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                                      3,  col2x, row, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_MOUNT_RAW_NOT_TRANS, 0);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "translated",                                  40, col3x, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                                      3,  col2x, row, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_MOUNT_RAW_NOT_TRANS, 1);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "RAW",                                         40, col3x, row++, gotoOffset);
    screen.push_back(comp);

    row++;
    //------------

    comp = new ConfigComponent(this, ConfigComponent::label, "Access ZIP files as",                         40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                                      3,  col2x, row, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_USE_ZIP_DIR_NOT_FILE, 0);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "files",                                       40, col3x, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                                      3,  col2x, row, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_USE_ZIP_DIR_NOT_FILE, 1);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "dirs",                                        40, col3x, row++, gotoOffset);
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

    comp = new ConfigComponent(this, ConfigComponent::label, "them.",   40, 0, row++, gotoOffset);
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
        checkboxGroup_setCheckedId(COMPID_MOUNT_RAW_NOT_TRANS, 1);          // select RAW
    } else {
        checkboxGroup_setCheckedId(COMPID_MOUNT_RAW_NOT_TRANS, 0);          // select TRANS
    }

    //---------
    bool useZipdirNotFile = s.getBool("USE_ZIP_DIR", 1);           // use ZIP DIRs, enabled by default

    if(useZipdirNotFile) {
        checkboxGroup_setCheckedId(COMPID_USE_ZIP_DIR_NOT_FILE, 1);         // ZIP DIRs enabled
    } else {
        checkboxGroup_setCheckedId(COMPID_USE_ZIP_DIR_NOT_FILE, 0);         // ZIP DIRs disabled
    }
    //---------

    setFocusToFirstFocusable();
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

    createScreen_homeScreen();      // now back to the home screen
}
