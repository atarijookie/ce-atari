#ifndef __MAIN_NETSERVER_H__
#define __MAIN_NETSERVER_H__

#define SERVER_UDP_PORT             7200        // port where the CE discovery for reports from cores and for requests from devices
#define CLIENT_UDP_PORT             7201        // this is where the CE device will wait for discovery response

#define SERVER_TCP_PORT_HDD_FIRST   7300        // first port used by HDD core (next at +1, +2, ...)

#define SERVER_TCP_PORT_FDD         7400        // port used by FDD core
#define SERVER_TCP_PORT_IKBD        7401        // port used by IKBD core

#define SERVER_STATUS_NOT_RUNNING   0       // when this server slot is not used yet and server is not running
#define SERVER_STATUS_FREE          1       // server is running but no client is connected there
#define SERVER_STATUS_OCCUPIED      2       // server is running and client is connected

typedef struct {
    uint8_t     status;         // one of the SERVER_STATUS_* values
    uint32_t    clientIp;       // IP of client that is connected to this server
    uint32_t    lastUpdate;     // time when this struct was last updated
    int         pid;            // pid of server's process
} TCEServerStatus;

#define MAX_SERVER_COUNT    8

#endif
