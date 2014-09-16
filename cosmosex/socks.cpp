#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

#include "socks.h"

static char gServerIp[128];
static int  gServerPort;
static int  clientSockFd = -1;

static int  gServerPortForServer;
static int  serverListenSockFd = -1;
static int  serverConnectSockFd = -1;
static int  serverSocket_createConnection(void);
static int  sockRead(int fd, unsigned char *bfr, int len);
static int  sockWrite(int fd, unsigned char *bfr, int len);

#define TAG_START   0xab

#ifdef DEBUGSTRINGS
    #define DEBUGSTR    printf
#else
    #define DEBUGSTR    (void)     
#endif

void clientSocket_setParams(char *serverIp, int serverPort)
{
    memset(gServerIp, 0, 128);
    strncpy(gServerIp, serverIp, 127);      // store server IP

    gServerPort = serverPort;               // store server port
}

void serverSocket_setParams(int serverPort)
{
    gServerPortForServer = serverPort;
}

int clientSocket_write(unsigned char *bfr, int len)
{
    int res = clientSocket_createConnection();

    if(!res) {
        DEBUGSTR("clientSocket_write: Failed to create connection to server!\n");
        return -1;
    }

    res = sockWrite(clientSockFd, bfr, len);

    if(len > 0 && res == 0) {               // if we tried to send something, but nothing was sent, close the sock and try again later
        close(clientSockFd);
        clientSockFd = -1;
    }

    return res;
}

int serverSocket_write(unsigned char *bfr, int len)
{
    int res = serverSocket_createConnection();

    if(!res) {
        DEBUGSTR("serverSocket_write: Failed to create connection...\n");
        return -1;
    }

    res = sockWrite(serverConnectSockFd, bfr, len);
    return res;
}

int clientSocket_read(unsigned char *bfr, int len)
{
    int res = clientSocket_createConnection();

    if(!res) {
        DEBUGSTR("clientSocket_read: Failed to create connection to server!\n");
        return -1;
    }

    res = sockRead(clientSockFd, bfr, len);
    return res;
}

int serverSocket_read(unsigned char *bfr, int len)
{
    int res = serverSocket_createConnection();

    if(!res) {
        DEBUGSTR("serverSocket_read: Failed to create connection...\n");
        return -1;
    }

    res = sockRead(serverConnectSockFd, bfr, len);
    return res;
}

int sockWrite(int fd, unsigned char *bfr, int len)
{
    ssize_t n;
    n = write(fd, bfr, len);

    if(n != len) {
        DEBUGSTR("sockWrite: sending failed...\n");
    }

    return len;
}

int sockRead(int fd, unsigned char *bfr, int len)
{
    ssize_t n;
    n = read(fd, bfr, len);                                // read data

    if(n == 0 && fd == serverConnectSockFd) {               // for server socket, when read returns 0, the client has quit
        close(serverConnectSockFd);
        serverConnectSockFd = -1;
    }

    if(n != len) {
        DEBUGSTR("sockRead: reading failed...\n");
    }

    return len;
}

int clientSocket_createConnection(void)
{
    if(clientSockFd != -1) {                   // got connection? just quit
        return 1;
    }

    int s;

    struct sockaddr_in serv_addr; 
    DEBUGSTR("clientSocket_createConnection\n");

    s = socket(AF_INET, SOCK_STREAM, 0);
    if(s < 0) {
        DEBUGSTR("Error: Could not create socket...\n");
        return 0;
    } 

    int flag = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));  // turn off Nagle's algorithm

    memset(&serv_addr, 0, sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(gServerPort); 

    if(inet_pton(AF_INET, gServerIp, &serv_addr.sin_addr) <= 0) {
        DEBUGSTR("Error: inet_pton failed\n");
        close(s);
        return 0;
    } 

    if(connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        DEBUGSTR("Error: Socket connect failed\n");
        close(s);
        return 0;
    } 

    DEBUGSTR("clientSocket_createConnection success\n");
    clientSockFd = s;
    return 1;
}

int serverSocket_createConnection(void)
{
    struct sockaddr_in serv_addr; 

    DEBUGSTR("serverSocket_createConnection\n");
    if(serverListenSockFd == -1) {                                                  // if we don't have listen socket yet, create it
        serverListenSockFd = socket(AF_INET, SOCK_STREAM, 0);                       // create socket
        memset(&serv_addr,  0, sizeof(serv_addr));

        int flag = 1;
        setsockopt(serverListenSockFd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));  // turn off Nagle's algorithm

        serv_addr.sin_family        = AF_INET;
        serv_addr.sin_addr.s_addr   = htonl(INADDR_ANY);
        serv_addr.sin_port          = htons(gServerPortForServer);                  // IP port

        bind(serverListenSockFd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));  // bind socket

        listen(serverListenSockFd, 3);                                              // set length of listening queue (max waiting connections)
    }

    if(serverConnectSockFd == -1) {                                                 // no client? wait for it...
        DEBUGSTR("Wating for connection...\n");
        serverConnectSockFd = accept(serverListenSockFd, (struct sockaddr*)NULL, NULL); 
    } 

    return 1;
}


