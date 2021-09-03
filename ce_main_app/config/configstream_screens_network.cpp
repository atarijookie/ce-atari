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

void ConfigStream::createScreen_network(void)
{
    // the following 3 lines should be at start of each createScreen_ method
    destroyCurrentScreen();             // destroy current components
    screenChanged   = true;         // mark that the screen has changed
    showingHomeScreen   = false;        // mark that we're NOT showing the home screen

    screen_addHeaderAndFooter(screen, "Network settings");

    ConfigComponent *comp;

    int col0x = 3;
    int col1x = 6;
    int col2x = 19;

    // hostname setting
    int row = 3;

    comp = new ConfigComponent(this, ConfigComponent::label, "Hostname",    10, col0x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",   15, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_HOSTNAME);
    comp->setTextOptions(TEXT_OPTION_ALLOW_LETTERS | TEXT_OPTION_ALLOW_NUMBERS);
    screen.push_back(comp);

    // DNS
    comp = new ConfigComponent(this, ConfigComponent::label, "DNS",         40, col0x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",   15, col2x, row, gotoOffset);
    comp->setComponentId(COMPID_NET_DNS);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    row += 2;

    // settings for ethernet
    comp = new ConfigComponent(this, ConfigComponent::label, "Ethernet",    10, col0x, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Use DHCP",    10, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, " ",        1,  col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_NET_DHCP);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "IP address",  40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",   15, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_NET_IP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Mask",        40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",   15, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_NET_MASK);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Gateway",     40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",   15, col2x, row, gotoOffset);
    comp->setComponentId(COMPID_NET_GATEWAY);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    row += 2;

    // settings for wifi
    comp = new ConfigComponent(this, ConfigComponent::label, "Wifi",        10, col0x, row++, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Enable",      10, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, " ",         1, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_ENABLE);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "WPA SSID",    20, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "     ",    31, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_SSID);
    comp->setTextOptions(TEXT_OPTION_ALLOW_ALL);
    comp->setLimitedShowSize(15);               // limit to showing only 15 characters
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "WPA PSK", 20,         col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline_pass, "      ",  63, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_PSK);
    comp->setTextOptions(TEXT_OPTION_ALLOW_ALL);
    comp->setLimitedShowSize(15);               // limit to showing only 15 characters
    screen.push_back(comp);

    row++;

    comp = new ConfigComponent(this, ConfigComponent::label, "Use DHCP",    10, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::checkbox, " ",        1,  col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_DHCP);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "IP address",  40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",   15, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_IP);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Mask",        40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",   15, col2x, row++, gotoOffset);
    comp->setComponentId(COMPID_WIFI_MASK);
    comp->setTextOptions(TEXT_OPTION_ALLOW_NUMBERS | TEXT_OPTION_ALLOW_DOT);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::label, "Gateway",     40, col1x, row, gotoOffset);
    screen.push_back(comp);

    comp = new ConfigComponent(this, ConfigComponent::editline, "      ",   15, col2x, row, gotoOffset);
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
    ns.load();                      // load the current values

    setTextByComponentId(COMPID_HOSTNAME,       ns.hostname);
    setTextByComponentId(COMPID_NET_DNS,        ns.nameserver);

    setBoolByComponentId(COMPID_NET_DHCP,       ns.eth0.dhcpNotStatic);
    setTextByComponentId(COMPID_NET_IP,         ns.eth0.address);
    setTextByComponentId(COMPID_NET_MASK,       ns.eth0.netmask);
    setTextByComponentId(COMPID_NET_GATEWAY,    ns.eth0.gateway);

    setBoolByComponentId(COMPID_WIFI_ENABLE,    ns.wlan0.isEnabled);
    setBoolByComponentId(COMPID_WIFI_DHCP,      ns.wlan0.dhcpNotStatic);
    setTextByComponentId(COMPID_WIFI_IP,        ns.wlan0.address);
    setTextByComponentId(COMPID_WIFI_MASK,      ns.wlan0.netmask);
    setTextByComponentId(COMPID_WIFI_GATEWAY,   ns.wlan0.gateway);

    setTextByComponentId(COMPID_WIFI_SSID,      ns.wlan0.wpaSsid);
    setTextByComponentId(COMPID_WIFI_PSK,       ns.wlan0.wpaPsk);

    setFocusToFirstFocusable();
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
    getBoolByComponentId(COMPID_NET_DHCP,       eUseDhcp);
    getTextByComponentId(COMPID_NET_IP,         eIp);
    getTextByComponentId(COMPID_NET_MASK,       eMask);
    getTextByComponentId(COMPID_NET_GATEWAY,    eGateway);

    getBoolByComponentId(COMPID_WIFI_ENABLE,    wifiIsEnabled);
    getBoolByComponentId(COMPID_WIFI_DHCP,      wUseDhcp);
    getTextByComponentId(COMPID_WIFI_IP,        wIp);
    getTextByComponentId(COMPID_WIFI_MASK,      wMask);
    getTextByComponentId(COMPID_WIFI_GATEWAY,   wGateway);

    getTextByComponentId(COMPID_NET_DNS,        dns);
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
    createScreen_homeScreen();      // now back to the home screen
}
