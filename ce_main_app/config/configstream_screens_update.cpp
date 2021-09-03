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

#define MIN_CHECK_PAUSE             (15 * 1000)
uint32_t lastUpdateCheck = 0;          // this holds the ime when we've last checked for update

void ConfigStream::createScreen_update(void)
{
    uint32_t now;
    now = Utils::getCurrentMs();

    if((now - lastUpdateCheck) >= MIN_CHECK_PAUSE) { // check for update, but not too often
        lastUpdateCheck = now;
        unlink(UPDATE_STATUS_FILE);
        system("/ce/update/check_for_update.sh 2>&1 | tee -a /var/log/ce_update.log &");
    }

    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();         // destroy current components
    screenChanged   = true;         // mark that the screen has changed
    showingHomeScreen   = false;        // mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, "Software & Firmware updates");

    ConfigComponent *comp;

    int cl1 = 5;
    int cl2 = 27;

    int line = 4;
    comp = new ConfigComponent(this, ConfigComponent::label, "Hardware version  : ", 22, cl1, line, gotoOffset);
    screen.push_back(comp);

    char hwVer[8];
    sprintf(hwVer, "v. %d", hwConfig.version);  // v1 / v2 / v3

    comp = new ConfigComponent(this, ConfigComponent::label, hwVer, 10, cl2, line, gotoOffset);
    screen.push_back(comp);
    line++;

    comp = new ConfigComponent(this, ConfigComponent::label, "HDD interface type: ", 22, cl1, line, gotoOffset);
    screen.push_back(comp);

    const char *hddIf = (hwConfig.hddIface == HDD_IF_SCSI) ? "SCSI" : "ACSI";

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
    cl1 = 5;
    cl2 = 20;
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

    comp = new ConfigComponent(this, ConfigComponent::label, "   part         your version ", 29, 5, line, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);

    line += 2;

    int col1 = 8, col2 = 21;

    comp = new ConfigComponent(this, ConfigComponent::label, "Main App", 12,    col1, line, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 26,           col2, line, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_COSMOSEX);
    screen.push_back(comp);
    line++;

    if(hwConfig.version < 3) {      // v1 and v2 - show Franz
        comp = new ConfigComponent(this, ConfigComponent::label, "Franz", 12,       col1, line, gotoOffset);
        screen.push_back(comp);

        comp = new ConfigComponent(this, ConfigComponent::label, " ", 26,           col2, line, gotoOffset);
        comp->setComponentId(COMPID_UPDATE_FRANZ);
        screen.push_back(comp);
        line++;
    }

    const char *chipName = (hwConfig.version < 3) ? "Hans" : "Horst";       // Hans for v1 and v2, Horst for v3
    comp = new ConfigComponent(this, ConfigComponent::label, chipName, 12,      col1, line, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, " ", 26,           col2, line, gotoOffset);
    comp->setComponentId(COMPID_UPDATE_HANZ);
    screen.push_back(comp);
    line++;

    if(hwConfig.version < 3) {      // v1 and v2 - show xilinx
        comp = new ConfigComponent(this, ConfigComponent::label, "Xilinx", 12,      col1, line, gotoOffset);
        screen.push_back(comp);

        comp = new ConfigComponent(this, ConfigComponent::label, " ", 26,           col2, line, gotoOffset);
        comp->setComponentId(COMPID_UPDATE_XILINX);
        screen.push_back(comp);
    }

    line += 2;

    // this updateStatus component shows content of update status file
    comp = new ConfigComponent(this, ConfigComponent::updateStatus, " ", 30, 5, line, gotoOffset);
    comp->setReverse(true);
    screen.push_back(comp);

    line += 2;

    comp = new ConfigComponent(this, ConfigComponent::button, " OnlineUp ", 10, 0, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_UPDATE_ONLINE);
    comp->setComponentId(COMPID_UPDATE_BTN_CHECK);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "   USB   ", 10, 14, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_UPDATE_USB);
    comp->setComponentId(COMPID_UPDATE_BTN_CHECK_USB);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, "  Cancel  ", 10,  28, line, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    fillUpdateWithCurrentVersions();                // fill the version lines with versions, if we got them

    setFocusToFirstFocusable();
}

void ConfigStream::fillUpdateWithCurrentVersions(void)
{
    std::string str;

    if(hwConfig.version < 3) {  // for v1 and v2
        datesToStrings(Update::versions.franz, str);
        setTextByComponentId(COMPID_UPDATE_FRANZ, str);         // set it to component

        datesToStrings(Update::versions.xilinx, str);
        setTextByComponentId(COMPID_UPDATE_XILINX, str);        // set it to component
    }

    datesToStrings(Update::versions.app, str);
    setTextByComponentId(COMPID_UPDATE_COSMOSEX, str);      // set it to component

    datesToStrings(Update::versions.hans, str);
    setTextByComponentId(COMPID_UPDATE_HANZ, str);          // set it to component
}

void ConfigStream::datesToStrings(Version &v1, std::string &str)
{
    char ver[40];

    v1.toString(ver);           // get single version
    str = ver;                  // put it in string
    str.resize(14, ' ');        // and expand it to length of 14 with spaces
}

void ConfigStream::updateOnline(void)
{
    Update::removeSimpleTextFile(UPDATE_USBFILE);       // remove this file to avoid trying to update from USB

    if(Utils::fileExists("/tmp/UPDATE_PENDING_NO")) {   // check_for_update.sh returned NO update pending
        showMessageScreen("Online Update", "Everything seems to be up-to-date.\n\rNot doing update.\n\r");
        return;
    }

    if(Utils::fileExists("/tmp/UPDATE_PENDING_YES")) {  // check_for_update.sh returned that there is update pending
        updateStart();                                  // start the update
    } else {                                            // check_for_update.sh not returned anything yet
        uint32_t now = Utils::getCurrentMs();

        if((now - lastUpdateCheck) < FAILED_UPDATE_CHECK_TIME) {    // if within valid time for checking for update
            showMessageScreen("Online Update", "Still checking for update.\n\rPlease try again in a while.\n\r");
        } else {                // if checking for update probably failed, proceed anyway
            updateStart();      // start the update
        }
    }
}

void ConfigStream::updateFromFile(void)
{
    // try to find the update
    std::string pathToUpdateFile;

    bool found = Update::checkForUpdateListOnUsb(pathToUpdateFile);

    if(!found) {
        char msg[256];
        sprintf(msg, "File %s not found.\n\rCan't update from USB.\n\r", Update::getUsbArchiveName());
        showMessageScreen("Update from USB", msg);
        return;
    }

    // if found, do the actual update (and path to it will be stored in UPDATE_USBFILE)
    updateStart();
}

void ConfigStream::updateStart(void)
{
    if(shownOn == CONFIGSTREAM_IN_LINUX_CONSOLE) {  // when trying to do update from from linux console
        showMessageScreen("STOP!", "Don't update the firmware using\n\rlinux console config tool!\n\r\n\rInstead run /ce/ce_update.sh or\n\rdo it using CE_CONF.TOS on Atari.");
        return;
    }

    if(shownOn == CONFIGSTREAM_THROUGH_WEB) {       // when trying to do update from from web console
        showMessageScreen("STOP!", "Don't update the firmware using\n\rweb interface config tool!\n\r\n\rInstead run /ce/ce_update.sh or\n\rdo it using CE_CONF.TOS on Atari.");
        return;
    }

    // write update script, no linux reboot, don't force xilinx flash
    Update::createUpdateScript(false, false);

    // terminate app and do the update
    isUpdateStartingFlag = 1;                       // set this flag, but don't terminate yet so ST will be able to ask for update status and retrieves this flag
    whenCanStartInstall = Utils::getEndTime(3000);  // we can start install after 3 seconds from now on

    Debug::out(LOG_INFO, ">>> User requests update, the app will be terminated in a 3 seconds. <<<\n");
}

void ConfigStream::createScreen_update_download(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();             // destroy current components
    screenChanged       = true;         // mark that the screen has changed
    showingHomeScreen   = false;        // mark that we're NOT showing the home screen

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

    setFocusToFirstFocusable();
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
    ConfigComponent *c = findComponentById(COMPID_DL1); // find a component which is on update download page

    if(c == NULL) {                                     // not on update download page? 
        return false;
    }

    // we're on that update download page
    return true;
}
