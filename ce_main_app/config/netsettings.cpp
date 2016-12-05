#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../debug.h"
#include "../settings.h"
#include "../utils.h"

#include "netsettings.h"

NetworkSettings::NetworkSettings(void)
{
    initNetSettings(&eth0);
    initNetSettings(&wlan0);
    
    nameserver  = "";
    hostname    = "CosmosEx";           // default hostname
}

void NetworkSettings::initNetSettings(TNetInterface *neti)
{
    neti->dhcpNotStatic = true;
    neti->address       = "";
    neti->netmask       = "";
    neti->gateway       = "";
    
    neti->wpaSsid       = "";
    neti->wpaPsk        = "";
}

void NetworkSettings::load(void)
{
    #ifdef DISTRO_YOCTO
    loadOnYocto();
    #else
    loadOnRaspbian();
    #endif    
}

void NetworkSettings::save(void)
{
    #ifdef DISTRO_YOCTO
    saveOnYocto();
    #else
    saveOnRaspbian();
    #endif
}

void NetworkSettings::replaceIPonDhcpIface(void)
{
    if(!eth0.dhcpNotStatic && !wlan0.dhcpNotStatic) {       // if eth0 is static, and wlan0 is static, just quit
        return;
    }

    BYTE tmp[10];
    Utils::getIpAdds(tmp);                                  // get real IPs
    
    if(eth0.dhcpNotStatic) {                                // eth0 is DHCP? replace config file values with real values
        char addr[32];
        sprintf(addr, "%d.%d.%d.%d", (int) tmp[1], (int) tmp[2], (int) tmp[3], (int) tmp[4]);
        
        eth0.address    = addr;
        eth0.netmask    = "";
        eth0.gateway    = "";
    }
    
    if(wlan0.dhcpNotStatic) {                                // wlan0 is DHCP? replace config file values with real values
        char addr[32];
        sprintf(addr, "%d.%d.%d.%d", (int) tmp[6], (int) tmp[7], (int) tmp[8], (int) tmp[9]);
        
        wlan0.address   = addr;
        wlan0.netmask   = "";
        wlan0.gateway   = "";
    }
}

void NetworkSettings::readString(const char *line, const char *tag, std::string &val, bool singleWordLine)
{
    const char *str = strstr(line, tag);                        // find tag position

    if(str == NULL) {                                   // tag not present?
        return;
    }
    
    int tagLen = strlen(tag);                           // get tag length
    
    char tmp[1024];
    int ires;

    if(singleWordLine) {                                // if line value is a single word - not containing spaces, use %s for reading
        ires = sscanf(str + tagLen + 1, "%s", tmp);
    } else {                                            // if line value might be more words (may contain spaces), use %[^\n] for reading of line
        ires = sscanf(str + tagLen + 1, "%[^\n]", tmp);
    }
        
    if(ires != 1) {                                     // reading value failed?
        return;
    }

    val = tmp;                                          // store value

    if(val.length() < 1) {
        return;
    }
    
    if(val.at(0) == '"') {                              // starts with double quotes? remove them
        val.erase(0, 1);
    }
    
    size_t pos = val.rfind("\"");                       // find last occurrence or double quotes
    if(pos != std::string::npos) {                      // erase last double quotes
        val.erase(pos, 1);
    }   
}

void NetworkSettings::dumpSettings(void)
{
    Debug::out(LOG_DEBUG, "Network settings");
    Debug::out(LOG_DEBUG, "eth0:");
    Debug::out(LOG_DEBUG, "      DHCP %s", eth0.dhcpNotStatic ? "enabled" : "disabled");
    Debug::out(LOG_DEBUG, "  hostname %s", hostname.c_str());
    Debug::out(LOG_DEBUG, "   address %s", eth0.address.c_str());
    Debug::out(LOG_DEBUG, "   netmask %s", eth0.netmask.c_str());
    Debug::out(LOG_DEBUG, "   gateway %s", eth0.gateway.c_str());

    Debug::out(LOG_DEBUG, "");
    Debug::out(LOG_DEBUG, "wlan0:");
    Debug::out(LOG_DEBUG, "      DHCP %s", wlan0.dhcpNotStatic ? "enabled" : "disabled");
    Debug::out(LOG_DEBUG, "  hostname %s", hostname.c_str());
    Debug::out(LOG_DEBUG, "   address %s", wlan0.address.c_str());
    Debug::out(LOG_DEBUG, "   netmask %s", wlan0.netmask.c_str());
    Debug::out(LOG_DEBUG, "   gateway %s", wlan0.gateway.c_str());
    Debug::out(LOG_DEBUG, "  wpa-ssid %s", wlan0.wpaSsid.c_str());
    Debug::out(LOG_DEBUG, "   wpa-psk %s", wlan0.wpaPsk.c_str());
    
    Debug::out(LOG_DEBUG, "nameserver %s", nameserver.c_str());
}

int NetworkSettings::ipNetmaskToCIDRnetmask(const char *ipNetmask)
{
    int a,b,c,d, res;
    
    res = sscanf(ipNetmask, "%d.%d.%d.%d", &a, &b, &c, &d);

    int cidr = 24;      // default: 24 - means 255.255.255.0
    if(res == 4) {
        if(a == 255 && b == 255 && c == 255 && d == 255) {
            cidr = 32;
        }

        if(a == 255 && b == 255 && c == 255 && d == 0) {
            cidr = 24;
        }

        if(a == 255 && b == 255 && c == 0   && d == 0) {
            cidr = 16;
        }

        if(a == 255 && b == 0   && c == 0   && d == 0) {
            cidr = 8;
        }
    } 
    
    return cidr;
}

void NetworkSettings::saveWpaSupplicant(void)
{
    FILE *f = fopen(WPA_SUPPLICANT_FILE, "wt");
    
    if(!f) {
        Debug::out(LOG_ERROR, "NetworkSettings::saveWpaSupplicant - failed to open wpa supplication file.\n");
        return;
    }
    
#ifdef DISTRO_YOCTO
    // do this for yocto
    fprintf(f, "ctrl_interface=/var/run/wpa_supplicant\n");               // this is needed for wpa_cli to work
#else
    // do this for raspbian
    fprintf(f, "country=GB\n");
    fprintf(f, "ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev\n");
    fprintf(f, "update_config=1\n");
#endif

    fprintf(f, "network={\n");
    fprintf(f, "    ssid=\"%s\"\n", wlan0.wpaSsid.c_str()); 
    fprintf(f, "    psk=\"%s\"\n",  wlan0.wpaPsk.c_str());
    fprintf(f, "}\n\n");

    fclose(f);
}

void NetworkSettings::loadWpaSupplicant(void)
{
    FILE *f = fopen(WPA_SUPPLICANT_FILE, "rt");
    
    if(!f) {
        Debug::out(LOG_ERROR, "NetworkSettings::loadWpaSupplicant - failed to open wpa supplicant file, this might be OK\n");
        return;
    }
    
    #define MAX_LINE_LEN    1024
    char line[MAX_LINE_LEN];
    
    while(!feof(f)) {
        char *res = fgets(line, MAX_LINE_LEN, f);               // get single line
        
        if(!res) {                                              // if failed to get the line
            break;
        }
        
        char *p;
        
        p = strstr(line, "ssid");                               // it's a line with SSID?
        if(p != NULL) {
            readString(line, "ssid", wlan0.wpaSsid, false);
            continue;
        }
        
        p = strstr(line, "psk");                                // it's a line with PSK?
        if(p != NULL) {
            readString(line, "psk", wlan0.wpaPsk, false);
            continue;
        }
    }
    
    fclose(f);
}

void NetworkSettings::loadNameserver(void)
{
    Settings s;
    nameserver = s.getString("NAMESERVER", "");
}

void NetworkSettings::saveNameserver(void)
{
    Settings s;
    s.setString("NAMESERVER", nameserver.c_str());

    updateResolvConf();                                 // update resolv.conf
}

void NetworkSettings::updateResolvConf(void)
{
    load();                                             // first load the settings

    if(eth0.dhcpNotStatic && wlan0.dhcpNotStatic) {     // if eth0 and wlan0 are DCHP, don't do anything
        Debug::out(LOG_ERROR, "NetworkSettings::updateResolvConf -- didn't update resolv.conf - network settings are DHCP");
        return;
    }
    
    if(nameserver.empty()) {                            // if there's nothing to store, don't do anything
        Debug::out(LOG_ERROR, "NetworkSettings::updateResolvConf -- didn't update resolv.conf - nameserver string is empty");
        return;
    }
    
    Debug::out(LOG_ERROR, "NetworkSettings::updateResolvConf -- updated resolv.conf with %s", nameserver.c_str());
    
    // if at least one interface is static, create and execute prepend script for resolv.conf
    char cmd[128];

    // move any existing resolv.conf to tmp
    system("mv /etc/resolv.conf /tmp/resolv.old");          
    
    // now fill the (non-existing) resolv.conf with what we want
    sprintf(cmd, "echo -e 'nameserver %s\n' > /etc/resolv.conf", nameserver.c_str());
    system(cmd);
     
     // and append old resolv.conf to the new one
    system("cat /tmp/resolv.old >> /etc/resolv.conf");
}


