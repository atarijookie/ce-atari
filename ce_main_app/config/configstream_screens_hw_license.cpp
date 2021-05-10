#include <stdio.h>
#include <string.h>

#include "../global.h"
#include "../debug.h"
#include "../native/scsi_defs.h"
#include "../acsidatatrans.h"
#include "../mounter.h"
#include "../display/displaythread.h"

#include "../settings.h"
#include "../utils.h"
#include "keys.h"
#include "configstream.h"
#include "netsettings.h"

extern THwConfig hwConfig;          // info about the current HW setup
extern TFlags    flags;             // global flags from command line (and others)

//--------------------------
// screen creation methods
void ConfigStream::createScreen_hwLicense(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();             // destroy current components
    screenChanged       = true;         // mark that the screen has changed
    showingHomeScreen   = false;        // mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, (char *) "HW License");

    ConfigComponent *comp;

    int row = 6;

    // description on the top
    comp = new ConfigComponent(this, ConfigComponent::label, "Device is missing a hardware license.", 40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "For more info see: ",                   40, 0, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "http://joo.kie.sk/cosmosex/license",    40, 0, row++, gotoOffset);
    screen.push_back(comp);

    row += 3;

    // line showing HW serial
    comp = new ConfigComponent(this, ConfigComponent::label, "Hardware serial number:",               40, 6, row++, gotoOffset);
    screen.push_back(comp);

    char tmp[32];
    Settings::binToHex(hwConfig.hwSerial, 13, tmp);   // HW serial as hexadecimal string

    comp = new ConfigComponent(this, ConfigComponent::label, tmp,                                     26, 6, row++, gotoOffset);
    screen.push_back(comp);

    row++;

    // license key
    comp = new ConfigComponent(this, ConfigComponent::label, "License key",                             40, 0, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, " ",                                    20, 15, row++, gotoOffset);
    comp->setComponentId(COMPID_HW_LICENSE);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_LETTERS);
    screen.push_back(comp);

    row++;

    row = 20;

    comp = new ConfigComponent(this, ConfigComponent::button, " Save ", 6,  9, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_HW_LICENSE_SAVE);
    comp->setComponentId(COMPID_BTN_SAVE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::button, " Cancel ", 8, 20, row, gotoOffset);
    comp->setOnEnterFunctionCode(CS_GO_HOME);
    comp->setComponentId(COMPID_BTN_CANCEL);
    screen.push_back(comp);

    Settings s;

    char keyName[64];
    Settings::generateLicenseKeyName(hwConfig.hwSerial, keyName);   // generate key name containing HW serial number

    const char *hwLicenseText = s.getString(keyName, "00000000000000000000000000");
    std::string hwLicense = std::string(hwLicenseText);
    setTextByComponentId(COMPID_HW_LICENSE, hwLicense);

    setFocusToFirstFocusable();
}

void ConfigStream::onHwLicenseConfigSave(void)
{
    // get hw license typed by user
    std::string hwLicense;
    getTextByComponentId(COMPID_HW_LICENSE, hwLicense);

    // filter license key to contain only allowed chars
    char license[32];
    memset(license, 0, 32);

    int len = hwLicense.length();
    const char* hwLicenseStr = hwLicense.c_str();

    int store = 0;
    for(int i=0; i<len; i++) {
        if( (hwLicenseStr[i] >= '0' && hwLicenseStr[i] <= '9') ||       // it's a number
            (hwLicenseStr[i] >= 'a' && hwLicenseStr[i] <= 'f') ||       // or small letter
            (hwLicenseStr[i] >= 'A' && hwLicenseStr[i] <= 'F') ) {      // or capital letter

                if(hwLicenseStr[i] >= 'a' && hwLicenseStr[i] <= 'f') {  // convert small letters to capital 
                    license[store] = hwLicenseStr[i] - 32;
                } else {                                                // use others as is
                    license[store] = hwLicenseStr[i];
                }

                store++;
            }
    }

    int licLen = strlen(license);

    // check if license key seems to be valid
    if(licLen != 20) {
        showMessageScreen("Warning", "License key seems to be invalid.\n\rPlease fix this and try again.");

        std::string hwLicense = std::string(license);
        setTextByComponentId(COMPID_HW_LICENSE, hwLicense);
        return;
    }

    // generate hw lincese key name for settings, as this one is stored with serial number in the name
    char keyName[64];
    Settings::generateLicenseKeyName(hwConfig.hwSerial, keyName);   // generate key name containing HW serial number

    // store the new hw lincese to settings
    Settings s;
    s.setString(keyName, hwLicense.c_str());

    flags.deviceGetLicense  = true;    // set to true, so device should get license again

    // now back to the home screen
    createScreen_homeScreen();
}
