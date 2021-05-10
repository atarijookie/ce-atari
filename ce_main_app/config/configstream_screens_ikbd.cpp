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

void ConfigStream::createScreen_ikbd(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();             // destroy current components
    screenChanged       = true;         // mark that the screen has changed
    showingHomeScreen   = false;        // mark that we're NOT showing the home screen

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
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",                 10, colButton, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY0_BUTTON);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    // up
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",                 10, colUpDown, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY0_UP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    row++;
    // left
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",                 10, colLeft, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY0_LEFT);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    // down
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",                 10, colUpDown, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY0_DOWN);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    // right
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",                 10, colRight, row, gotoOffset);
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
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",                 10, colButton, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY1_BUTTON);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    // up
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",                 10, colUpDown, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY1_UP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    row++;
    // left
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",                 10, colLeft, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY1_LEFT);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    // down
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",                 10, colUpDown, row, gotoOffset);
    comp->setComponentId(COMPID_KEYBJOY1_DOWN);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_LETTERS_ONLY_UPPERCASE);
    screen.push_back(comp);

    // right
    comp = new ConfigComponent(this, ConfigComponent::editline, "          ",                 10, colRight, row, gotoOffset);
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

    createScreen_homeScreen();      // now back to the home screen
}
