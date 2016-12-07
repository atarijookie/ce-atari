#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../debug.h"
#include "../settings.h"
#include "../utils.h"

#include "netsettings.h"

void NetworkSettings::loadOnRaspbian(void)
{
    // initialise settings, DHCP by default (if not found in 
    initNetSettings(&eth0);
    initNetSettings(&wlan0);
    
    Debug::out(LOG_DEBUG, "NetworkSettings::loadOnRaspbian() - reading from file %s", NETWORK_DHCPCD_FILE);
    FILE *f = fopen(NETWORK_DHCPCD_FILE, "rt");                 // read static IP config from DHCPCD file
    
    if(!f) {
        Debug::out(LOG_ERROR, "NetworkSettings::loadOnRaspbian - failed to open network settings file.\n");
        return;
    }
    
    #define MAX_LINE_LEN    1024
    char line[MAX_LINE_LEN];
    char tmp1[128];
    int  ires, cidr;
    
    TNetInterface *currentIface = NULL;                         // store the settings to the struct pointed by this pointer
    hostname = "CosmosEx";                                      // default hostname
    
    while(!feof(f)) {
        char *res = fgets(line, MAX_LINE_LEN, f);               // get single line
        
        if(!res) {                                              // if failed to get the line
            break;
        }
        
        if(line[0] == '#') {                                    // if it's a line with a comment, skip it
            continue;
        }
        
        if(strlen(line) < 2) {                                  // skip empty line
            continue;
        }
        
        // found start of interface section?
        if(strstr(line, "interface ") != NULL) {
            if(strstr(line, "eth0") != NULL) {                  // found eth0 section?
                currentIface = &eth0;
                initNetSettings(currentIface);                  // clear the struct
            }

            if(strstr(line, "wlan0") != NULL) {                 // found wlan0 section?
                currentIface = &wlan0;
                initNetSettings(currentIface);                  // clear the struct
            }

            continue;                                           // nothing usefull in this line
        }

        if(!currentIface) {                                     // current interface not (yet) set? skip the rest
            continue;
        }

        if(strstr(line, "static ip_address=") != NULL) {        // static IP?
            currentIface->dhcpNotStatic = false;                // static config
            
            ires = sscanf(line + 18, "%[^/]/%d", tmp1, &cidr);     // try to read IP and netmask

            if(ires == 2) {
                currentIface->address = tmp1;
                
                switch(cidr) {
                    case  8:    currentIface->netmask = "255.0.0.0";        break;
                    case 16:    currentIface->netmask = "255.255.0.0";      break;
                    case 24:    currentIface->netmask = "255.255.255.0";    break;
                    case 32:    currentIface->netmask = "255.255.255.255";  break;
                    default:    currentIface->netmask = "255.255.255.0";    break;
                }
            } else {
                currentIface->netmask = "255.255.255.0";
            }
        }
        
        if(strstr(line, "static routers=") != NULL) {               // static gateway?
            currentIface->dhcpNotStatic = false;                    // static config 
            currentIface->gateway = line + 15;
        }
        
        if(strstr(line, "static domain_name_servers=") != NULL) {   // static nameserver? 
            currentIface->dhcpNotStatic = false;                    // static config 
            nameserver = line + 27;
        }
    }
    
    fclose(f);

    loadWpaSupplicant();
    replaceIPonDhcpIface();
    
    dumpSettings();
}

void NetworkSettings::saveOnRaspbian(void)
{
    Debug::out(LOG_DEBUG, "NetworkSettings::saveOnRaspbian()");
    saveDhcpcdRaspbian();
    raspbianSaveToNetworkInterfaces();
    
    saveWpaSupplicant();
    saveNameserver();
}

void NetworkSettings::raspbianSaveToNetworkInterfaces(void)
{
    Debug::out(LOG_DEBUG, "NetworkSettings::raspbianSaveToNetworkInterfaces() - writing to file %s", NETWORK_CONFIG_FILE);
    FILE *f = fopen(NETWORK_CONFIG_FILE, "wt");
    
    if(!f) {         // could open file
        Debug::out(LOG_ERROR, "NetworkSettings::raspbianSaveToNetworkInterfaces - failed to open network settings file.\n");
        return;
    }

    // lo section
    fprintf(f, "# The loopback network interface\n");
    fprintf(f, "auto lo\n");
    fprintf(f, "iface lo inet loopback\n\n");
    
    // eth section
    fprintf(f, "# The primary network interface\n");
    writeNetInterfaceSettingsRaspbian(f, &eth0, "eth0");
    
    // wlan section
    fprintf(f, "# The wireless network interface\n");
    writeNetInterfaceSettingsRaspbian(f, &wlan0, "wlan0");

    fclose(f);
}

void NetworkSettings::writeNetInterfaceSettingsRaspbian(FILE *f, TNetInterface *iface, const char *ifaceName)
{
    bool isWlan = (iface == &wlan0);

    fprintf(f, "auto %s\n", ifaceName);                 // not sure if this is still needed, but in the past if eth0 didn't had this, it didn't start automatically
    fprintf(f, "allow-hotplug %s\n", ifaceName);

    fprintf(f, "iface %s inet %s\n", ifaceName, iface->dhcpNotStatic ? "dhcp" : "manual");

    // this section doesn't need to go here, but in case you will stop using dhcp, then the old scripts might find the config here
    if(iface->dhcpNotStatic) {
        fprintf(f, "hostname %s\n", hostname.c_str());         // this should appear in dhcp client list 
    } else {
        fprintf(f, "hostname %s\n", hostname.c_str());         // not really used when not dhcp, but storing it to preserve it
        fprintf(f, "address %s\n",  iface->address.c_str());
        fprintf(f, "netmask %s\n",  iface->netmask.c_str()); 
        fprintf(f, "gateway %s\n",  iface->gateway.c_str());
    }

    if(isWlan) {
        fprintf(f, "    wpa-conf %s\n", WPA_SUPPLICANT_FILE);
    }
    
    fprintf(f, "\n");
}

void NetworkSettings::saveDhcpcdRaspbian(void)
{
    Debug::out(LOG_DEBUG, "NetworkSettings::saveDhcpcdRaspbian() - writing to file %s", NETWORK_DHCPCD_FILE);
    FILE *f = fopen(NETWORK_DHCPCD_FILE, "wt");

    if(!f) {         // could open file
        Debug::out(LOG_ERROR, "NetworkSettings::saveDhcpcdRaspbian - failed to open network settings file.\n");
        return;
    }
    
    // first some default things
    fprintf(f, "# Inform the DHCP server of our hostname for DDNS.\n");
    fprintf(f, "hostname\n\n");
    fprintf(f, "# Use the hardware address of the interface for the Client ID.\n");
    fprintf(f, "clientid\n\n");
    fprintf(f, "# Persist interface configuration when dhcpcd exits.\n");
    fprintf(f, "persistent\n\n");
    fprintf(f, "# Rapid commit support.\n");
    fprintf(f, "option rapid_commit\n\n");
    fprintf(f, "# A list of options to request from the DHCP server.\n");
    fprintf(f, "option domain_name_servers, domain_name, domain_search, host_name\n");
    fprintf(f, "option classless_static_routes\n\n");
    fprintf(f, "# Most distributions have NTP support.\n");
    fprintf(f, "option ntp_servers\n\n");
    fprintf(f, "# A ServerID is required by RFC2131.\n");
    fprintf(f, "require dhcp_server_identifier\n\n");
    fprintf(f, "# Generate Stable Private IPv6 Addresses instead of hardware based ones\n");
    fprintf(f, "slaac private\n\n");
    fprintf(f, "# A hook script is provided to lookup the hostname if not set by the DHCP\n");
    fprintf(f, "# server, but it should not be run by default.\n");
    fprintf(f, "nohook lookup-hostname\n\n");

    writeDhcpcdSettingsRaspbian(f, &eth0,   "eth0");
    writeDhcpcdSettingsRaspbian(f, &wlan0,  "wlan0");

    fclose(f);
}

void NetworkSettings::writeDhcpcdSettingsRaspbian(FILE *f, TNetInterface *iface, const char *ifaceName)
{
    if(iface->dhcpNotStatic) {  // if dhcp, don't write anything
        return;
    }

    // got here - means static ip
    fprintf(f, "interface %s\n",                    ifaceName);
    fprintf(f, "static ip_address=%s/%d\n",         iface->address.c_str(), ipNetmaskToCIDRnetmask(iface->netmask.c_str()));
    fprintf(f, "static routers=%s\n",               iface->gateway.c_str());
    fprintf(f, "static domain_name_servers=%s\n\n", nameserver.c_str());
}

