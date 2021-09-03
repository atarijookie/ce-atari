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

void ConfigStream::createScreen_shared(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();         // destroy current components
    screenChanged   = true;         // mark that the screen has changed
    showingHomeScreen   = false;        // mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, "Shared drive settings");

    ConfigComponent *comp;

    int row = 3;

    int col1x = 0;
    int col2x = 10;
    int col3x = col2x + 6;

    // description on the top
    comp = new ConfigComponent(this, ConfigComponent::label, "Define what folder on which machine will",    40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "be used as drive mounted through network",    40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "on CosmosEx. Works in translated mode.",      40, 0, row++, gotoOffset);
    screen.push_back(comp);

    row++;

    // enabled checkbox
    comp = new ConfigComponent(this, ConfigComponent::label, "Enabled",                                     40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                                      3,  col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_SHARED_ENABLED);
    screen.push_back(comp);

    row++;

    // sharing protocol checkbox group
    comp = new ConfigComponent(this, ConfigComponent::label, "Sharing protocol",                            40, col1x, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                                      3,  col2x, row, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_SHARED_NFS_NOT_SAMBA, 1);                                                              // set checkbox group id COMPID_SHARED_NFS_NOT_SAMBA, and checbox id 1 for NFS (variable SHARED_NFS_NOT_SAMBA)
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "NFS",                                         40, col3x, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, "   ",                                      3,  col2x, row, gotoOffset);
    comp->setCheckboxGroupIds(COMPID_SHARED_NFS_NOT_SAMBA, 0);                                                              // set checkbox group id COMPID_SHARED_NFS_NOT_SAMBA, and checbox id 0 for Samba (variable SHARED_NFS_NOT_SAMBA)
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Samba / cifs / windows",                      40, col3x, row++, gotoOffset);
    screen.push_back(comp);

    row++;


    // ip address edit line
    comp = new ConfigComponent(this, ConfigComponent::label, "IP address of server",                        40, col1x, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",                                        15, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_SHARED_IP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    row++;

    // folder on server
    comp = new ConfigComponent(this, ConfigComponent::label, "Shared folder path on server",                40, col1x, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",                                        35, 2, row++, gotoOffset);
    comp->setComponentId(COMPID_SHARED_PATH);
    screen.push_back(comp);

    row++;

    // username and password
    comp = new ConfigComponent(this, ConfigComponent::label, "Username",                                    40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",                                        40, col2x, row, gotoOffset);
    comp->setComponentId(COMPID_USERNAME);
    comp->setLimitedShowSize(20);               // limit to showing only 20 characters
    screen.push_back(comp);

    row++;

    comp = new ConfigComponent(this, ConfigComponent::label, "Password",                                    40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline_pass, " ",                                   40, col2x, row, gotoOffset);
    comp->setComponentId(COMPID_PASSWORD);
    comp->setLimitedShowSize(20);               // limit to showing only 20 characters
    screen.push_back(comp);

    row += 2;

    // buttons
/*
    comp = new ConfigComponent(this, ConfigComponent::button, "  Test  ",                                   8,  4, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SHARED_TEST);
    comp->setComponentId(COMPID_SHARED_BTN_TEST);
    screen.push_back(comp);
*/
    comp = new ConfigComponent(this, ConfigComponent::button, "  Save  ",                                   8, /*15*/ 9, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_SHARED_SAVE);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ",                                   8, /*27*/ 21, row, gotoOffset);
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

    enabled     = s.getBool("SHARED_ENABLED",           false);
    nfsNotSamba = s.getBool("SHARED_NFS_NOT_SAMBA", true);

    setTextByComponentId(COMPID_SHARED_IP,      addr);
    setTextByComponentId(COMPID_SHARED_PATH,    path);
    setBoolByComponentId(COMPID_SHARED_ENABLED, enabled);

    setTextByComponentId(COMPID_USERNAME,       username);
    setTextByComponentId(COMPID_PASSWORD,       password);

    if(nfsNotSamba) {
        checkboxGroup_setCheckedId(COMPID_SHARED_NFS_NOT_SAMBA, 1);         // select NFS
    } else {
        checkboxGroup_setCheckedId(COMPID_SHARED_NFS_NOT_SAMBA, 0);         // select samba
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
    getBoolByComponentId(COMPID_SHARED_ENABLED, enabled);
    nfsNotSamba = (bool) checkboxGroup_getCheckedId(COMPID_SHARED_NFS_NOT_SAMBA);

    getTextByComponentId(COMPID_USERNAME,       username);
    getTextByComponentId(COMPID_PASSWORD,       password);

    if(enabled) {                                       // if enabled, do validity checks, othewise let it just pass
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

    s.setBool   ("SHARED_ENABLED",          enabled);
    s.setBool   ("SHARED_NFS_NOT_SAMBA",    nfsNotSamba);
    s.setString ("SHARED_ADDRESS",          ip.c_str());
    s.setString ("SHARED_PATH",             path.c_str());

    s.setString ("SHARED_USERNAME",         username.c_str());
    s.setString ("SHARED_PASSWORD",         password.c_str());

    if(reloadProxy) {                                       // if got settings reload proxy, invoke reload
        reloadProxy->reloadSettings(SETTINGSUSER_SHARED);
    }

    Utils::forceSync();                                     // tell system to flush the filesystem caches

    createScreen_homeScreen();      // now back to the home screen
}
