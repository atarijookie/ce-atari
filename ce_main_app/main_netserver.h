#ifndef __MAIN_NETSERVER_H__
#define __MAIN_NETSERVER_H__

#define MAIN_PORT       7200                // port number where this main CE network server listens

#define SERVER_STATUS_NOT_RUNNING   0       // when this server slot is not used yet and server is not running
#define SERVER_STATUS_FREE          1       // server is running but no client is connected there
#define SERVER_STATUS_OCCUPIED      2       // server is running and client is connected

typedef struct {
    uint16_t    port;           // port where the individual server listens
    uint8_t     status;         // one of the SERVER_STATUS_* values
    uint32_t    clientIp;       // IP of client that is connected to this server
    uint32_t    lastUpdate;     // time when this struct was last updated
} TCEServerStatus;

#endif
