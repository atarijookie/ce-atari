#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../debug.h"
#include "../settings.h"
#include "../utils.h"

#include "netsettings.h"

void NetworkSettings::loadOnYocto(void)
{
    initNetSettings(&eth0);
    initNetSettings(&wlan0);
    
    Debug::out(LOG_DEBUG, "NetworkSettings::loadOnYocto() - reading from file %s", NETWORK_CONFIG_FILE);
    FILE *f = fopen(NETWORK_CONFIG_FILE, "rt");
    
    if(!f) {
        Debug::out(LOG_ERROR, "NetworkSettings::loadOnYocto - failed to open network settings file.\n");
        return;
    }
    
    #define MAX_LINE_LEN    1024
    char line[MAX_LINE_LEN];
    
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
        
        // found start of iface section?
        if(strstr(line, "iface") != NULL) {
            if(strstr(line, "eth0") != NULL) {                  // found eth0 section?
                currentIface = &eth0;
                initNetSettings(currentIface);                  // clear the struct
            }

            if(strstr(line, "wlan0") != NULL) {                 // found wlan0 section?
                currentIface = &wlan0;
                initNetSettings(currentIface);                  // clear the struct
            }

            if(!currentIface) {                                 // it wasn't eth0 and it wasn't wlan0?
                continue;
            }
        
            if(strstr(line, "inet dhcp") != NULL) {             // dhcp config?
                currentIface->dhcpNotStatic = true;
            }

            if(strstr(line, "inet static") != NULL) {           // static config?
                currentIface->dhcpNotStatic = false;
            }
        }

        if(strstr(line, "hostname") != NULL) {                  // found hostname?
            readString(line, "hostname", hostname, true);
        }

        if(!currentIface) {                                     // current interface not (yet) set? skip the rest
            continue;
        }

        readString(line, "address", currentIface->address, true);
        readString(line, "netmask", currentIface->netmask, true);
        readString(line, "gateway", currentIface->gateway, true);
    }
    
    fclose(f);

    loadWpaSupplicant();
    loadNameserver();
    
    replaceIPonDhcpIface();
    
    dumpSettings();
}

void NetworkSettings::saveOnYocto(void)
{
    Debug::out(LOG_DEBUG, "NetworkSettings::saveOnYocto() - writing file %s", NETWORK_CONFIG_FILE);
    FILE *f = fopen(NETWORK_CONFIG_FILE, "wt");
    
    if(!f) {
        Debug::out(LOG_ERROR, "NetworkSettings::saveOnYocto - failed to open network settings file.\n");
        return;
    }
    
    // lo section
    fprintf(f, "# The loopback network interface\n");
    fprintf(f, "auto lo\n");
    fprintf(f, "iface lo inet loopback\n\n");
    
    // eth section
    fprintf(f, "# The primary network interface\n");
    writeNetInterfaceSettingsYocto(f, &eth0, "eth0");
    
    // wlan section
    fprintf(f, "# The wireless network interface\n");
    writeNetInterfaceSettingsYocto(f, &wlan0, "wlan0");

    fprintf(f, "wpa-conf %s \n\n", WPA_SUPPLICANT_FILE);
    
    fclose(f);
    
    saveWpaSupplicant();
    saveNameserver();
}

void NetworkSettings::writeNetInterfaceSettingsYocto(FILE *f, TNetInterface *iface, const char *ifaceName)
{
    fprintf(f, "auto %s\n", ifaceName);
    fprintf(f, "iface %s inet ", ifaceName);
    
    if(iface->dhcpNotStatic) {
        fprintf(f, "dhcp\n");
        fprintf(f, "hostname %s\n", hostname.c_str());         // this should appear in dhcp client list 
    } else {
        fprintf(f, "static\n");
        fprintf(f, "hostname %s\n", hostname.c_str());         // not really used when not dhcp, but storing it to preserve it
        fprintf(f, "address %s\n",  iface->address.c_str());
        fprintf(f, "netmask %s\n",  iface->netmask.c_str()); 
        fprintf(f, "gateway %s\n",  iface->gateway.c_str());
    }
    
    fprintf(f, "\n");
}



