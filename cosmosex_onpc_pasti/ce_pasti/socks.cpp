#include "stdafx.h"

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include "socks.h"

static char gServerIp[128];
static int  gServerPort;
static int  clientSockFd = -1;

static int  sockRead(SOCKET &fd, unsigned char *bfr, int len);
static int  sockWrite(SOCKET &fd, unsigned char *bfr, int len);

#define TAG_START   0xab

void log(char *str);

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

WSADATA wsaData;
SOCKET sck = INVALID_SOCKET;

int clientSocket_createConnection(void)
{
    if(sck != INVALID_SOCKET) {														// got connection? just quit
        return 1;
    }

    log("clientSocket_createConnection\n");

	int res = WSAStartup(0x101, &wsaData);											// init winsock
	if(res) {
	    log("clientSocket_createConnection - WSAStartup failed\n");
		return 0;
	}

	log("clientSocket_createConnection - open socket\n");

	sck = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);								// create socket
	if(sck == INVALID_SOCKET) {
	    log("clientSocket_createConnection - socket() failed\n");
		return 0;
	}

	log("clientSocket_createConnection - gethostbyaddr\n");

	unsigned long addr	= inet_addr("127.0.0.1");
    hostent *hp			= gethostbyaddr((char*)&addr, sizeof(addr), AF_INET);

	sockaddr_in server;
	server.sin_addr.s_addr	= *((unsigned long*)hp->h_addr);
	server.sin_family		= AF_INET;
	server.sin_port			= htons(1111);

    log("clientSocket_createConnection - before connect\n");

	if(connect(sck, (struct sockaddr*) &server, sizeof(server))) {					// connect to host
	    log("clientSocket_createConnection - connect() failed\n");
	    closesocket(sck);
		sck = INVALID_SOCKET;
		return 0;	
	}

    log("clientSocket_createConnection - connected!\n");

    return 1;
}

int clientSocket_write(unsigned char *bfr, int len)
{
    int res = clientSocket_createConnection();

    if(!res) {
        log("clientSocket_write: Failed to create connection to server!\n");
        return -1;
    }

    res = sockWrite(sck, bfr, len);

    if(len > 0 && res == 0) {               // if we tried to send something, but nothing was sent, close the sock and try again later
        closesocket(sck);
   		sck = INVALID_SOCKET;
    }

    return res;
}

int clientSocket_read(unsigned char *bfr, int len)
{
    int res = clientSocket_createConnection();

    if(!res) {
        log("clientSocket_read: Failed to create connection to server!\n");
        return -1;
    }

    res = sockRead(sck, bfr, len);
    return res;
}

int sockWrite(SOCKET &s, unsigned char *bfr, int len)
{
	char tmp[128];
	sprintf(tmp, "sockWrite - total len: %d\n", len);
	log(tmp);

	int n = send(s, (char *) bfr, len, 0);

    if(n != len) {
        log("sockWrite: sending failed...\n");
    }

    return len;
}

int sockRead(SOCKET &s, unsigned char *bfr, int len)
{
	int cnt = len;
	BYTE *b = bfr;

	char tmp[128];
	sprintf(tmp, "sockRead - total len: %d\n", len);
	log(tmp);

	memset(bfr, 0, len);

	while(cnt > 0) {
		int n = recv(s, (char *) b, cnt, 0);

		if(n == SOCKET_ERROR) {
			closesocket(s);
			s = INVALID_SOCKET;

	        log("sockRead: socket closed because SOCKET_ERROR, will try to reconnect\n");
			return 0;
		}

		cnt -= n;		// now we need to read less
		b	+= n;
	}

    return len;
}




