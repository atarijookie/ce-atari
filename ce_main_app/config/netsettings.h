#ifndef _NETSETTINGS_H_
#define _NETSETTINGS_H_

#include <string>

#define NETWORK_CONFIG_FILE     "/etc/network/interfaces"
#define NAMESERVER_FILE         "/etc/resolv.conf"
#define NETWORK_DHCPCD_FILE     "/etc/dhcpcd.conf"

// for raspbian
#define WPA_SUPPLICANT_FILE     "/etc/wpa_supplicant/wpa_supplicant.conf"

typedef struct {
    bool        isEnabled;

    bool        dhcpNotStatic;
    std::string address;
    std::string netmask;
    std::string gateway;
    
    std::string wpaSsid;
    std::string wpaPsk;
} TNetInterface;

class NetworkSettings 
{
public:
    NetworkSettings(void);

    void load(void);
    void save(void);

    bool isDifferentThan        (NetworkSettings &other);
    bool eth0IsDifferentThan    (NetworkSettings &other);
    bool wlan0IsDifferentThan   (NetworkSettings &other);

    void updateResolvConf(bool autoLoadBeforeSave);
    
    TNetInterface   eth0;
    TNetInterface   wlan0;
    std::string     nameserver;
    std::string     hostname;

private:
    void loadOnRaspbian(void);
    void saveOnRaspbian(void);

    void raspbianSaveToNetworkInterfaces(void);
    void initNetSettings(TNetInterface *neti);
    void readString(const char *line, const char *tag, std::string &val, bool singleWordLine);
    void dumpSettings(void);
    
    void loadNameserver(void);
    void saveNameserver(void);
    
    void loadHostname(void);
    void saveHostname(void);
    
    void saveDhcpcdRaspbian(void);
    void writeDhcpcdSettingsRaspbian(FILE *f, TNetInterface *iface, const char *ifaceName);
    
    void loadWpaSupplicant(void);
    void saveWpaSupplicant(void);
    
    void writeNetInterfaceSettingsRaspbian(FILE *f, TNetInterface *iface, const char *ifaceName);
    
    void replaceIPonDhcpIface(void);
    
    int  ipNetmaskToCIDRnetmask(const char *ipNetmask);
    
    void removeEol(char *str);
};

#endif

