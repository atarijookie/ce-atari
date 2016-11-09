#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../debug.h"
#include "../settings.h"
#include "../utils.h"

#include "netsettings.h"

#define NETWORK_CONFIG_FILE		"/etc/network/interfaces"
#define NAMESERVER_FILE			"/etc/resolv.conf"

#ifdef DISTRO_YOCTO
    // for yocto
    #define WPA_SUPPLICANT_FILE     "/etc/wpa_supplicant.conf"
#else
    // for raspbian
    #define WPA_SUPPLICANT_FILE     "/etc/wpa_supplicant/wpa_supplicant.conf"
#endif

NetworkSettings::NetworkSettings(void)
{
	initNetSettings(&eth0);
	initNetSettings(&wlan0);
	
	nameserver  = "";
    hostname    = "CosmosEx";           // default hostname
}

void NetworkSettings::initNetSettings(TNetInterface *neti)
{
	neti->dhcpNotStatic	= true;
	neti->address		= "";
    neti->netmask		= "";
    neti->gateway		= "";
	
	neti->wpaSsid		= "";
	neti->wpaPsk		= "";
}

void NetworkSettings::load(void)
{
	FILE *f = fopen(NETWORK_CONFIG_FILE, "rt");
	
	if(!f) {
		Debug::out(LOG_ERROR, (char *) "NetworkSettings::load - failed to open network settings file.\n");
		return;
	}
	
	initNetSettings(&eth0);
	initNetSettings(&wlan0);
	
	#define MAX_LINE_LEN	1024
	char line[MAX_LINE_LEN];
	
	TNetInterface *currentIface = NULL;							// store the settings to the struct pointed by this pointer
	hostname = "CosmosEx";                                      // default hostname
    
	while(!feof(f)) {
		char *res = fgets(line, MAX_LINE_LEN, f);				// get single line
		
		if(!res) {												// if failed to get the line
			break;
		}
		
		// found start of iface section?
		if(strstr(line, "iface") != NULL) {
			if(strstr(line, "eth0") != NULL) {					// found eth0 section?
				currentIface = &eth0;
				initNetSettings(currentIface);					// clear the struct
			}

			if(strstr(line, "wlan0") != NULL) {					// found wlan0 section?
				currentIface = &wlan0;
				initNetSettings(currentIface);					// clear the struct
			}

			if(!currentIface) {									// it wasn't eth0 and it wasn't wlan0?
				continue;
			}
		
			if(strstr(line, "inet dhcp") != NULL) {				// dhcp config?
				currentIface->dhcpNotStatic = true;
			}

			if(strstr(line, "inet static") != NULL) {			// static config?
				currentIface->dhcpNotStatic = false;
			}
		}

        if(strstr(line, "hostname") != NULL) {			        // found hostname?
            readString(line, (char *) "hostname", hostname, true);
        }

		if(!currentIface) {										// current interface not (yet) set? skip the rest
			continue;
		}

		readString(line, (char *) "address",	currentIface->address, true);
		readString(line, (char *) "netmask",	currentIface->netmask, true);
		readString(line, (char *) "gateway",	currentIface->gateway, true);
	}
	
	fclose(f);

    loadWpaSupplicant();
	loadNameserver();
    
    replaceIPonDhcpIface();
    
	dumpSettings();
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
        eth0.netmask	= "";
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

void NetworkSettings::readString(char *line, char *tag, std::string &val, bool singleWordLine)
{
	char *str = strstr(line, tag);					    // find tag position

	if(str == NULL) {									// tag not present?
		return;
	}
	
	int tagLen = strlen(tag);							// get tag length
	
	char tmp[1024];
	int ires;

    if(singleWordLine) {                                // if line value is a single word - not containing spaces, use %s for reading
        ires = sscanf(str + tagLen + 1, "%s", tmp);
    } else {                                            // if line value might be more words (may contain spaces), use %[^\n] for reading of line
        ires = sscanf(str + tagLen + 1, "%[^\n]", tmp);
    }
		
	if(ires != 1) {										// reading value failed?
		return;
	}

	val = tmp;											// store value

	if(val.length() < 1) {
		return;
	}
	
	if(val.at(0) == '"') {								// starts with double quotes? remove them
		val.erase(0, 1);
	}
	
	size_t pos = val.rfind("\"");						// find last occurrence or double quotes
	if(pos != std::string::npos) {						// erase last double quotes
		val.erase(pos, 1);
	}	
}

void NetworkSettings::dumpSettings(void)
{
	Debug::out(LOG_DEBUG, (char *) "Network settings");
	Debug::out(LOG_DEBUG, (char *) "eth0:");
	Debug::out(LOG_DEBUG, (char *) "      DHCP %s", eth0.dhcpNotStatic ? "enabled" : "disabled");
	Debug::out(LOG_DEBUG, (char *) "  hostname %s", (char *) hostname.c_str());
	Debug::out(LOG_DEBUG, (char *) "   address %s", (char *) eth0.address.c_str());
	Debug::out(LOG_DEBUG, (char *) "   netmask %s", (char *) eth0.netmask.c_str());
	Debug::out(LOG_DEBUG, (char *) "   gateway %s", (char *) eth0.gateway.c_str());

	Debug::out(LOG_DEBUG, (char *) "");
	Debug::out(LOG_DEBUG, (char *) "wlan0:");
	Debug::out(LOG_DEBUG, (char *) "      DHCP %s", wlan0.dhcpNotStatic ? "enabled" : "disabled");
	Debug::out(LOG_DEBUG, (char *) "  hostname %s", (char *) hostname.c_str());
	Debug::out(LOG_DEBUG, (char *) "   address %s", (char *) wlan0.address.c_str());
	Debug::out(LOG_DEBUG, (char *) "   netmask %s", (char *) wlan0.netmask.c_str());
	Debug::out(LOG_DEBUG, (char *) "   gateway %s", (char *) wlan0.gateway.c_str());
	Debug::out(LOG_DEBUG, (char *) "  wpa-ssid %s", (char *) wlan0.wpaSsid.c_str());
	Debug::out(LOG_DEBUG, (char *) "   wpa-psk %s", (char *) wlan0.wpaPsk.c_str());
	
	Debug::out(LOG_DEBUG, (char *) "nameserver %s", (char *) nameserver.c_str());
}

void NetworkSettings::save(void)
{
	FILE *f = fopen(NETWORK_CONFIG_FILE, "wt");
	
	if(!f) {
		Debug::out(LOG_ERROR, (char *) "NetworkSettings::save - failed to open network settings file.\n");
		return;
	}
	
	// lo section
	fprintf(f, "# The loopback network interface\n");
	fprintf(f, "auto lo\n");
	fprintf(f, "iface lo inet loopback\n\n");
	
	// eth section
	fprintf(f, "# The primary network interface\n");
    writeNetInterfaceSettings(f, eth0, (char *) "eth0");
	
	// wlan section
	fprintf(f, "# The wireless network interface\n");
    writeNetInterfaceSettings(f, wlan0, (char *) "wlan0");

  	fprintf(f, "wpa-conf %s \n\n", WPA_SUPPLICANT_FILE);
	
	fclose(f);
	
    saveWpaSupplicant();
	saveNameserver();
}

void NetworkSettings::writeNetInterfaceSettings(FILE *f, TNetInterface &iface, char *ifaceName)
{
	fprintf(f, "auto %s\n", ifaceName);
	fprintf(f, "iface %s inet ", ifaceName);
	
	if(iface.dhcpNotStatic) {
		fprintf(f, "dhcp\n");
        fprintf(f, "hostname %s\n", (char *) hostname.c_str());         // this should appear in dhcp client list 
	} else {
		fprintf(f, "static\n");
        fprintf(f, "hostname %s\n", (char *) hostname.c_str());         // not really used when not dhcp, but storing it to preserve it
	    fprintf(f, "address %s\n", (char *) iface.address.c_str());
		fprintf(f, "netmask %s\n", (char *) iface.netmask.c_str()); 
		fprintf(f, "gateway %s\n", (char *) iface.gateway.c_str());
	}
    
	fprintf(f, "\n");
}

void NetworkSettings::saveWpaSupplicant(void)
{
	FILE *f = fopen(WPA_SUPPLICANT_FILE, "wt");
	
	if(!f) {
		Debug::out(LOG_ERROR, (char *) "NetworkSettings::saveWpaSupplicant - failed to open wpa supplication file.\n");
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
	fprintf(f, "    ssid=\"%s\"\n",	(char *) wlan0.wpaSsid.c_str()); 
	fprintf(f, "    psk=\"%s\"\n",	(char *) wlan0.wpaPsk.c_str());
    fprintf(f, "}\n\n");

    fclose(f);
}

void NetworkSettings::loadWpaSupplicant(void)
{
	FILE *f = fopen(WPA_SUPPLICANT_FILE, "rt");
	
	if(!f) {
		Debug::out(LOG_ERROR, (char *) "NetworkSettings::loadWpaSupplicant - failed to open wpa supplicant file, this might be OK\n");
		return;
	}
	
	#define MAX_LINE_LEN	1024
	char line[MAX_LINE_LEN];
	
	while(!feof(f)) {
		char *res = fgets(line, MAX_LINE_LEN, f);				// get single line
		
		if(!res) {												// if failed to get the line
			break;
		}
		
        char *p;
        
        p = strstr(line, "ssid");                               // it's a line with SSID?
        if(p != NULL) {
            readString(line, (char *) "ssid", wlan0.wpaSsid, false);
            continue;
        }
        
        p = strstr(line, "psk");                                // it's a line with PSK?
        if(p != NULL) {
            readString(line, (char *) "psk", wlan0.wpaPsk, false);
            continue;
        }
    }
    
    fclose(f);
}

void NetworkSettings::loadNameserver(void)
{
    Settings s;
    nameserver = s.getString((char *) "NAMESERVER", (char *) "");
}

void NetworkSettings::saveNameserver(void)
{
    Settings s;
    s.setString((char *) "NAMESERVER", (char *) nameserver.c_str());

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
    sprintf(cmd, "echo -e 'nameserver %s\n' > /etc/resolv.conf", (char *) nameserver.c_str());
    system(cmd);
     
     // and append old resolv.conf to the new one
    system("cat /tmp/resolv.old >> /etc/resolv.conf");
}


