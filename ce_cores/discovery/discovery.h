#ifndef __DISCOVERY_H__
#define __DISCOVERY_H__

#define SERVER_UDP_PORT             7200        // port where the CE discovery for reports from cores and for requests from devices
#define CLIENT_UDP_PORT             7201        // this is where the CE device will wait for discovery response

#define SERVER_TCP_PORT_HDD         7300        // port used by HDD core
#define SERVER_TCP_PORT_FDD         7301        // port used by FDD core
#define SERVER_TCP_PORT_IKBD        7302        // port used by IKBD core

typedef struct {
    uint8_t     status;         // one of the SERVER_STATUS_* values
    uint32_t    clientIp;       // IP of client that is connected to this server
    uint32_t    lastUpdate;     // time when this struct was last updated
    int         pid;            // pid of server's process
} TCEServerStatus;

#define MAX_SERVER_COUNT    8

#endif
