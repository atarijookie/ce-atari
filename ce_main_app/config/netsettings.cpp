#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    Debug::out(LOG_INFO, "NetworkSettings::load() - starting to load settings");

    #ifdef DISTRO_YOCTO
    loadOnYocto();
    #else
    loadOnRaspbian();
    #endif

    Settings s;
    wlan0.isEnabled = s.getBool("WIFI_ENABLED", true);          // wifi enabled by default
}

void NetworkSettings::save(void)
{
    Debug::out(LOG_INFO, "NetworkSettings::save() - starting to save settings");

    #ifdef DISTRO_YOCTO
    saveOnYocto();
    #else
    saveOnRaspbian();
    #endif

    Settings s;
    s.setBool("WIFI_ENABLED", wlan0.isEnabled);                 // store wifi enabled flag
}

void NetworkSettings::replaceIPonDhcpIface(void)
{
    if(!eth0.dhcpNotStatic && !wlan0.dhcpNotStatic) {       // if eth0 is static, and wlan0 is static, just quit
        return;
    }

    BYTE ips[10];
    BYTE masks[10];
    Utils::getIpAdds(ips, masks);                           // get real IPs
    
    if(eth0.dhcpNotStatic) {                                // eth0 is DHCP? replace config file values with real values
        char addr[32], mask[32];
        sprintf(addr, "%d.%d.%d.%d", (int) ips[1], (int) ips[2], (int) ips[3], (int) ips[4]);
        sprintf(mask, "%d.%d.%d.%d", (int) masks[1], (int) masks[2], (int) masks[3], (int) masks[4]);
        
        eth0.address    = addr;
        eth0.netmask    = mask;
        eth0.gateway    = "";
    }
    
    if(wlan0.dhcpNotStatic) {                                // wlan0 is DHCP? replace config file values with real values
        char addr[32], mask[32];
        sprintf(addr, "%d.%d.%d.%d", (int) ips[6], (int) ips[7], (int) ips[8], (int) ips[9]);
        sprintf(mask, "%d.%d.%d.%d", (int) masks[6], (int) masks[7], (int) masks[8], (int) masks[9]);
        
        wlan0.address   = addr;
        wlan0.netmask   = mask;
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

        removeEol(line);                                        // remove EOL from string
        
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

    updateResolvConf(false);                            // update resolv.conf, no auto load
}

void NetworkSettings::updateResolvConf(bool autoLoadBeforeSave)
{
    if(autoLoadBeforeSave) {
        load();                                         // first load the settings
    }

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

bool NetworkSettings::isDifferentThan(NetworkSettings &other)
{
    // check general settings
    if(nameserver   != other.nameserver)                    return true;
    if(hostname     != other.hostname)                      return true;

    bool eht0IsDifferent    = eth0IsDifferentThan(other);   // check ethernet settings
    bool wlan0IsDifferent   = wlan0IsDifferentThan(other);  // check wifi settings

    return (eht0IsDifferent | wlan0IsDifferent);            // if eth0 or wlan0 is different, then whole setting is different
}

bool NetworkSettings::eth0IsDifferentThan(NetworkSettings &other)
{
    if(eth0.dhcpNotStatic != other.eth0.dhcpNotStatic)      return true;        // use DHCP settings changed?

    if(!eth0.dhcpNotStatic) {       // if using static settings
        if(eth0.address != other.eth0.address)              return true;
        if(eth0.netmask != other.eth0.netmask)              return true;
        if(eth0.gateway != other.eth0.gateway)              return true;
    }

    // if came here, nothing changed
    return false;
}
bool NetworkSettings::wlan0IsDifferentThan(NetworkSettings &other)
{
    if(wlan0.isEnabled      != other.wlan0.isEnabled)       return true;
    if(wlan0.dhcpNotStatic  != other.wlan0.dhcpNotStatic)   return true;
    if(wlan0.wpaSsid        != other.wlan0.wpaSsid)         return true;
    if(wlan0.wpaPsk         != other.wlan0.wpaPsk)          return true;

    if(!wlan0.dhcpNotStatic) {      // if using static settings
        if(wlan0.address != other.wlan0.address)            return true;
        if(wlan0.netmask != other.wlan0.netmask)            return true;
        if(wlan0.gateway != other.wlan0.gateway)            return true;
    }

    // if came here, nothing changed
    return false;
}

void NetworkSettings::loadHostname(void)
{
    hostname = "CosmosEx";                  // default hostname

    FILE *f = fopen("/etc/hostname", "rt");

    if(!f) {                                // failed to open file? fail
        return;
    }

    char tmpStr[33];
    memset(tmpStr, 0, sizeof(tmpStr));
    
    char *pRes = fgets(tmpStr, 32, f);      // try to read the hostname
    fclose(f);

    if(!pRes) {                             // failed to get the line? fail
        return;
    }

    removeEol(tmpStr);                      // remove EOL from string
    hostname = tmpStr;                      // store the hostname    
}

void NetworkSettings::saveHostname(void)
{
    //----------------------------
    // update hostname
    FILE *f = fopen("/etc/hostname", "wt");

    if(!f) {                                    // failed to open file? fail
        return;
    }

    fprintf(f, "%s", hostname.c_str());         // write the hostname
    fclose(f);

    //----------------------------
    // update hosts
    f = fopen("/etc/hosts", "rt");              // current file for reading

    if(!f) {
        return;
    }

    FILE *g = fopen("/etc/hosts.new", "wt");    // new file for writing

    if(!g) {
        fclose(f);
        return;
    }

    // first write two ip/names for our localhost
    fprintf(g, "127.0.0.1\tlocalhost\n");
    fprintf(g, "127.0.0.1\t%s\n", hostname.c_str());

    // then copy the rest from current /etc/hosts
    char inLine[256];
    while(!feof(f)) {                           // while there is something in the file   
        memset(inLine, 0, sizeof(inLine));  
        char *pRes = fgets(inLine, 255, f);     // read it

        if(!pRes) {                             // failed to read? try again
            continue;
        }

        if(strlen(inLine) < 2) {                // line seems to be too short? try again
            continue;            
        }

        if(strncmp(inLine, "127.0.0.1", 9) == 0) {  // if it's the old localhosts, skip it
            continue;            
        }

        fputs(inLine, g);                       // if came here, it's a valid line, reuse it
    }

    fclose(f);
    fclose(g);
    
    //-----------------------
    unlink("/etc/hosts");                       // delete current hosts file
    rename("/etc/hosts.new", "/etc/hosts");     // rename .new hosts file to correct name
}

void NetworkSettings::removeEol(char *str)
{
    while(*str != 0) {                          // while we're not at the end of string
        if(*str == '\r' || *str == '\n') {      // if it's new line / end of line
            *str = 0;                           // it's the new end of this string, quit
            break;
        }

        str++;                                  // move to next char
    }
}
