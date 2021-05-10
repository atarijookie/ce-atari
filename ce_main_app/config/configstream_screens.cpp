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

//--------------------------
// screen creation methods
void ConfigStream::createScreen_homeScreen(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();             // destroy current components
    screenChanged       = true;         // mark that the screen has changed
    showingHomeScreen   = true;         // mark that we're showing the home screen

    screen_addHeaderAndFooter(screen, "Main menu");

    ConfigComponent *comp;

    int line = 4;

    // show license menu only for devices versions which support licenses and if the license is not valid
    bool showLicenseMenu = ((hwConfig.version == 3) && !hwConfig.hwLicenseValid);

    if(showLicenseMenu) {   // should show license menu?
        comp = new ConfigComponent(this, ConfigComponent::button, " ! License key  ! ", 18, 10, line, gotoOffset);
        comp->setOnEnterFunctionCode(CS_CREATE_HW_LICENSE);
        screen.push_back(comp);
        line += 2;
    }

    const char *idConfigLabel = (hwConfig.hddIface == HDD_IF_SCSI) ? " SCSI IDs config " : " ACSI IDs config ";
    comp = new ConfigComponent(this, ConfigComponent::button, idConfigLabel,        18, 10, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_ACSI);
    screen.push_back(comp);
    line += 2;

    comp = new ConfigComponent(this, ConfigComponent::button, " Translated disks ", 18, 10, line, gotoOffset);
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

    if(!showLicenseMenu) {  // show 'Other' only if we're not showing license menu, as we want to fit it all on the screen
        comp = new ConfigComponent(this, ConfigComponent::button, " Other ",            18, 10, line, gotoOffset);
        comp->setOnEnterFunctionCode(CS_CREATE_OTHER);
        screen.push_back(comp);
        line += 2;
    }

    comp = new ConfigComponent(this, ConfigComponent::button, " Update software ",  18, 10, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_CREATE_UPDATE);
    screen.push_back(comp);
    line += 2;

    setFocusToFirstFocusable();
}

void ConfigStream::createScreen_other(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();             // destroy current components
    screenChanged       = true;         // mark that the screen has changed
    showingHomeScreen   = false;        // mark that we're NOT showing the home screen

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

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",                        15, col2, row++, gotoOffset);
    comp->setComponentId(COMPID_TIMESYNC_NTP_SERVER);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "UTC offset",                  40, col + 2, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",                        4, col2, row++, gotoOffset);
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
    int         frameSkip;
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
    int         frameSkip;
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

    createScreen_homeScreen();      // now back to the home screen
}

void ConfigStream::onResetSettings(void)
{
    createScreen_homeScreen();      // now back to the home screen

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
