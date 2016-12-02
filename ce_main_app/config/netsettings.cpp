#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../debug.h"
#include "../settings.h"
#include "../utils.h"

#include "netsettings.h"

#define NETWORK_CONFIG_FILE		"/etc/network/interfaces"
#define NAMESERVER_FILE			"/etc/resolv.conf"
#define NETWORK_DHCPCD_FILE     "/etc/dhcpcd.conf"

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
    #ifdef DISTRO_YOCTO
    loadOnYocto();
    #else
    loadOnRaspbian();
    #endif    
}

void NetworkSettings::loadOnYocto(void)
{
	initNetSettings(&eth0);
	initNetSettings(&wlan0);
	
	FILE *f = fopen(NETWORK_CONFIG_FILE, "rt");
	
	if(!f) {
		Debug::out(LOG_ERROR, "NetworkSettings::loadOnYocto - failed to open network settings file.\n");
		return;
	}
	
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
            readString(line, "hostname", hostname, true);
        }

		if(!currentIface) {										// current interface not (yet) set? skip the rest
			continue;
		}

		readString(line, "address",	currentIface->address, true);
		readString(line, "netmask",	currentIface->netmask, true);
		readString(line, "gateway",	currentIface->gateway, true);
	}
	
	fclose(f);

    loadWpaSupplicant();
	loadNameserver();
    
    replaceIPonDhcpIface();
    
	dumpSettings();
}

void NetworkSettings::loadOnRaspbian(void)
{
	initNetSettings(&eth0);
	initNetSettings(&wlan0);
	
	FILE *f = fopen(NETWORK_CONFIG_FILE, "rt");
	
	if(!f) {
		Debug::out(LOG_ERROR, "NetworkSettings::loadOnRaspbian - failed to open network settings file.\n");
		return;
	}
	
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
            readString(line, "hostname", hostname, true);
        }

		if(!currentIface) {										// current interface not (yet) set? skip the rest
			continue;
		}

		readString(line, "address",	currentIface->address, true);
		readString(line, "netmask",	currentIface->netmask, true);
		readString(line, "gateway",	currentIface->gateway, true);
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

void NetworkSettings::readString(const char *line, const char *tag, std::string &val, bool singleWordLine)
{
	const char *str = strstr(line, tag);					    // find tag position

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

void NetworkSettings::save(void)
{
    #ifdef DISTRO_YOCTO
    saveOnYocto();
    #else
    saveOnRaspbian();
    #endif
}

void NetworkSettings::saveOnYocto(void)
{
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

void NetworkSettings::saveOnRaspbian(void)
{
	FILE *f = fopen(NETWORK_CONFIG_FILE, "wt");
	
	if(f) {         // could open file
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
    } else {        // couldn't open file
		Debug::out(LOG_ERROR, "NetworkSettings::saveOnRaspbian - failed to open network settings file.\n");
		return;
	}
    
    saveWpaSupplicant();
	saveNameserver();
    
    saveDhcpcdRaspbian();
}

void NetworkSettings::saveDhcpcdRaspbian(void)
{
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

void NetworkSettings::writeNetInterfaceSettingsRaspbian(FILE *f, TNetInterface *iface, const char *ifaceName)
{
    bool isWlan = (iface == &wlan0);

    if(isWlan) {
        fprintf(f, "allow-hotplug %s\n", ifaceName);
    }
    
	fprintf(f, "iface %s inet %s\n", ifaceName, iface->dhcpNotStatic ? "dhcp" : "manual");

    if(isWlan) {
        fprintf(f, "    wpa-conf %s\n", WPA_SUPPLICANT_FILE);
    }
	
	fprintf(f, "\n");
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
	fprintf(f, "    ssid=\"%s\"\n",	wlan0.wpaSsid.c_str()); 
	fprintf(f, "    psk=\"%s\"\n",	wlan0.wpaPsk.c_str());
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


