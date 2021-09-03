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

extern uint8_t isUpdateStartingFlag;           // set to 1 to notify ST that the update is starting and it should show 'fake updating progress' screen
extern volatile uint32_t whenCanStartInstall;  // if this uint32_t has non-zero value, then it's a time when this app should be terminated for installing update

void ConfigStream::createScreen_acsiConfig(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();             // destroy current components
    screenChanged       = true;         // mark that the screen has changed
    showingHomeScreen   = false;        // mark that we're NOT showing the home screen

    const char *idHeaderLabel = (hwConfig.hddIface == HDD_IF_SCSI) ? " SCSI IDs config " : " ACSI IDs config ";
    screen_addHeaderAndFooter(screen, idHeaderLabel);

    ConfigComponent *comp;

    comp = new ConfigComponent(this, ConfigComponent::label, "ID         off   sd    raw  ce_dd", 40, 0, 3, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);

    int row;

    for(row=0; row<8; row++) {          // now make 8 rows of checkboxes
        char bfr[5];
        sprintf(bfr, "%d", row);

        comp = new ConfigComponent(this, ConfigComponent::label, bfr, 2, 1, row + 4, gotoOffset);
        screen.push_back(comp);

        for(int col=0; col<4; col++) {
            comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ", 3, 10 + (col * 6), row + 4, gotoOffset);         // create and place checkbox on screen
            comp->setCheckboxGroupIds(row, col);                                                                // set checkbox group id to row, and checbox id to col
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

    comp = new ConfigComponent(this, ConfigComponent::label, "ce_dd - for booting CE_DD driver",    40, 0, row++, gotoOffset);
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
    for(int id=0; id<8; id++) {                         // read the list of device types from settings
        sprintf(key, "ACSI_DEVTYPE_%d", id);            // create settings KEY, e.g. ACSI_DEVTYPE_0
        int devType = s.getInt(key, DEVTYPE_OFF);

        if(devType < 0) {
            devType = DEVTYPE_OFF;
        }

        checkboxGroup_setCheckedId(id, devType);        // set the checkboxes according to settings
    }

    setFocusToFirstFocusable();
}

void ConfigStream::onAcsiConfig_save(void)
{
    int devTypes[8];

    bool somethingActive = false;
    bool somethingInvalid = false;
    int tranCnt = 0, sdCnt = 0;

    for(int id=0; id<8; id++) {                             // get all selected types from checkbox groups
        devTypes[id] = checkboxGroup_getCheckedId(id);

        if(devTypes[id] != DEVTYPE_OFF) {                   // if found something which is not OFF
            somethingActive = true;
        }

        switch(devTypes[id]) {                              // count the shared drives, network adapters, config drives
        case DEVTYPE_TRANSLATED:    tranCnt++;                  break;
        case DEVTYPE_SD:            sdCnt++;                    break;
        case -1:                    somethingInvalid = true;    break;
        }
    }

    if(somethingInvalid) {                                  // if everything is set to OFF
        showMessageScreen("Warning", "Some ACSI/SCSI ID has no selected type.\n\rGo and select something!");
        return;
    }

    if(!somethingActive) {                                  // if everything is set to OFF
        showMessageScreen("Warning", "All ACSI/SCSI IDs are set to 'OFF',\n\rit is invalid and would brick the device.\n\rSelect at least one active ACSI/SCSI ID.");
        return;
    }

    if(tranCnt > 1) {                                       // more than 1 of this type?
        showMessageScreen("Warning", "You have more than 1 CE_DD selected.\n\rUnselect some to leave only\n\r1 active.");
        return;
    }

    if(sdCnt > 1) {                                         // more than 1 of this type?
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

    for(int id=0; id<8; id++) {                             // write all the ACSI IDs to settings
        sprintf(key, "ACSI_DEVTYPE_%d", id);                // create settings KEY, e.g. ACSI_DEVTYPE_0
        s.setInt(key, devTypes[id]);
    }

    if(reloadProxy) {                                       // if got settings reload proxy, invoke reload
        reloadProxy->reloadSettings(SETTINGSUSER_ACSI);
    }

    Utils::forceSync();                                     // tell system to flush the filesystem caches

    createScreen_homeScreen();      // now back to the home screen
}
