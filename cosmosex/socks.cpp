#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 

#include "datatypes.h"
#include "socks.h"
#include "debug.h"

static char gServerIp[128];
static int  gServerPort;
static int  clientSockFd = -1;

static int  gServerPortForServer;
static int  serverListenSockFd = -1;
static int  serverConnectSockFd = -1;
static int  serverSocket_createConnection(void);
static int  sockRead(int fd, unsigned char *bfr, int len);
static int  sockWrite(int fd, unsigned char *bfr, int len);

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
        Debug::out(LOG_DEBUG, "clientSocket_write: Failed to create connection to server!\n");
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
        Debug::out(LOG_DEBUG, "serverSocket_write: Failed to create connection...\n");
        return -1;
    }

    res = sockWrite(serverConnectSockFd, bfr, len);
    return res;
}

int clientSocket_read(unsigned char *bfr, int len)
{
    int res = clientSocket_createConnection();

    if(!res) {
        Debug::out(LOG_DEBUG, "clientSocket_read: Failed to create connection to server!\n");
        return -1;
    }

    res = sockRead(clientSockFd, bfr, len);
    return res;
}

int serverSocket_read(unsigned char *bfr, int len)
{
    int res = serverSocket_createConnection();

    if(!res) {
        Debug::out(LOG_DEBUG, "serverSocket_read: Failed to create connection...\n");
        return -1;
    }

    res = sockRead(serverConnectSockFd, bfr, len);
    return res;
}

int sockWrite(int fd, unsigned char *bfr, int len)
{
    ssize_t n;
    n = write(fd, bfr, len);

//    Debug::out(LOG_DEBUG, "sockWrite: sending %d bytes\n", len);

    if(n != len) {
        Debug::out(LOG_DEBUG, "sockWrite: sending failed...\n");
    }

    return len;
}

int sockRead(int fd, unsigned char *bfr, int len)
{
    ssize_t n;
    n = read(fd, bfr, len);                                // read data

//    Debug::out(LOG_DEBUG, "sockRead: receiving %d bytes\n", len);

    if(n == 0 && fd == serverConnectSockFd) {               // for server socket, when read returns 0, the client has quit
        Debug::out(LOG_DEBUG, "sockRead: closing socked\n");
        
        close(serverConnectSockFd);
        serverConnectSockFd = -1;
        
        return 0;
    }

    if(n != len) {
        Debug::out(LOG_DEBUG, "sockRead: reading failed...\n");
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
    Debug::out(LOG_DEBUG, "clientSocket_createConnection\n");

    s = socket(AF_INET, SOCK_STREAM, 0);
    if(s < 0) {
        Debug::out(LOG_DEBUG, "Error: Could not create socket...\n");
        return 0;
    } 

    int flag = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));  // turn off Nagle's algorithm

    memset(&serv_addr, 0, sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(gServerPort); 

    if(inet_pton(AF_INET, gServerIp, &serv_addr.sin_addr) <= 0) {
        Debug::out(LOG_DEBUG, "Error: inet_pton failed\n");
        close(s);
        return 0;
    } 

    if(connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        Debug::out(LOG_DEBUG, "Error: Socket connect failed\n");
        close(s);
        return 0;
    } 

    Debug::out(LOG_DEBUG, "clientSocket_createConnection success\n");
    clientSockFd = s;
    return 1;
}

int serverSocket_createConnection(void)
{
    struct sockaddr_in serv_addr; 

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
        Debug::out(LOG_DEBUG, "Wating for connection...\n");
        serverConnectSockFd = accept(serverListenSockFd, (struct sockaddr*)NULL, NULL); 
    } 

    return 1;
}

BYTE    header[16];
BYTE    *bufferRead;
BYTE    *bufferWrite;
DWORD   sockByteCount;
BYTE    sockReadNotWrite;

bool gotCmd(void)
{
    if(serverSocket_createConnection() != 1) {      // couldn't get connection? fail
        return false;
    } 

    int count;
    int res = ioctl(serverConnectSockFd, FIONREAD, &count);

    if(res == -1) {                                 // if ioctl failed, fail
        Debug::out(LOG_DEBUG, "gotCmd - ioctl failed");
        return false;
    }

    if(count < 16) {                                // not enough data? fail
//      Debug::out(LOG_DEBUG, "gotCmd - not enough data, got %d", count);
        return false;
    }
    
    res = serverSocket_read(header, 16);            // try to get header

    if(res != 16) {
        Debug::out(LOG_DEBUG, "gotCmd - failed to get header, res is %d", res);
        return false;
    }

    sockReadNotWrite = header[14];

    sockByteCount = header[15];                     // read how many bytes we need to transfer
    sockByteCount = sockByteCount * 512;            // convert sectors to bytes

    Debug::out(LOG_DEBUG, "gotCmd - got header (16 bytes)[%02x %02x %02x %02x %02x %02x], sockByteCount: %d, sockReadNotWrite: %d", header[0], header[1], header[2], header[3], header[4], header[5], sockByteCount, sockReadNotWrite);

    if(sockReadNotWrite == 0) {                     // on write - read data from ST, on read - we first have to process the command
        memset(bufferWrite, 0, sockByteCount);
        
        res = serverSocket_read(bufferWrite, sockByteCount);

        if(res != (int) sockByteCount) {
            Debug::out(LOG_DEBUG, "gotCmd - res != sockByteCount -- %d != %d", res, sockByteCount);
            return false;
        }
        
        // now calculate and verify checksum
        WORD sum = dataChecksum(bufferWrite, sockByteCount);
        
        WORD cs = 0;
        res = serverSocket_read((BYTE *) &cs, 2);
        
        if(res != 2) {          // didn't receive data? 
            Debug::out(LOG_DEBUG, "gotCmd - failed to get checksum!");
        }
        
        if(cs != sum) {         // checksum mismatch? 
            Debug::out(LOG_DEBUG, "gotCmd - checksum fail -- %04x != %04x", cs, sum);
            BYTE *p = bufferWrite;
            Debug::out(LOG_DEBUG, "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9]);
        } else {                // checksum ok?
//            Debug::out(LOG_DEBUG, "gotCmd - checksum good");
        }
    } 
    
    return true;
}

WORD dataChecksum(BYTE *data, int byteCount)
{
    int i;
    WORD sum = 0;
    WORD *wp = (WORD *) data;
    WORD wordCount = byteCount/2;
        
    for(i=0; i<wordCount; i++) {                // calculate checksum
        sum += *wp;
        wp++;
    }
    
    return sum;
}

