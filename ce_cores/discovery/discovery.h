#ifndef __DISCOVERY_H__
#define __DISCOVERY_H__

#define SERVER_UDP_PORT             7200        // port where the CE discovery for reports from cores and for requests from devices
#define CLIENT_UDP_PORT             7201        // this is where the CE device will wait for discovery response

#define SERVER_TCP_PORT_HDD         7300        // port used by HDD core
#define SERVER_TCP_PORT_FDD         7301        // port used by FDD core
#define SERVER_TCP_PORT_IKBD        7302        // port used by IKBD core

typedef struct {
    uint32_t clientIp;
    uint8_t mac[6];
    time_t lastTime;
} TClientIpToMac;

void getMacForIp(uint32_t clientIp, uint8_t* mac);

#endif
