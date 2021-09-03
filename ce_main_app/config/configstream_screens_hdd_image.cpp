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

void ConfigStream::createScreen_hddimage(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();         // destroy current components
    screenChanged   = true;         // mark that the screen has changed
    showingHomeScreen   = false;        // mark that we're NOT showing the home screen

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
