#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <pty.h>
#include <sys/file.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "../misc/global.h"
#include "../misc/debug.h"
#include "../misc/utils.h"
#include "discovery.h"
#include "../chipinterface/chipinterface.h"

void onClientRequest(sockaddr_in *clientAddr, uint8_t *recvData, int len);
void udpSend(uint32_t ip, uint16_t port, uint8_t* data, uint16_t len);

uint8_t serverIp[4] = {127, 0, 0, 1};

#define IP_TO_MAC_COUNT  (3 * MAX_CLIENTS)
TClientIpToMac ipToMac[IP_TO_MAC_COUNT];

int discoveryServerOpenSocket(void)
{
    int sockfd = 0;
    struct sockaddr_in servaddr;

    // create socket fd
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        logDiscovery(LOG_ERROR, "Failed to create main server socket");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // fill in server information
    servaddr.sin_family         = AF_INET;          // IPv4
    servaddr.sin_addr.s_addr    = INADDR_ANY;
    servaddr.sin_port           = htons(SERVER_UDP_PORT);

    // bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        logDiscovery(LOG_ERROR, "Failed to bind() main server socket");
        return -1;
    }

    return sockfd;
}

void discoveryMain(void)
{
    int udpSocket = discoveryServerOpenSocket();        // create main UDP socket

    if(udpSocket < 0) {     // on error, just quit
        logDiscovery(LOG_ERROR, "Could not open port %d, terminating", SERVER_UDP_PORT);
        printf("Could not open port %d, terminating", SERVER_UDP_PORT);
        return;
    }

    logDiscovery(LOG_INFO, "Starting CosmosEx discovery server");
    printf("Starting CosmosEx discovery server");

    struct timeval timeout;
    fd_set readfds;
    uint8_t recvData[256];
    struct sockaddr_in clientAddr;

    // This is main network server loop, which does the following:
    // - waits for and responds to requests from CE lite (telling CE lite the IP and port of server)
    // - holds the list of running servers and their status
    // - checks if any running servers is free and spawns a new one if it isn't
    while(sigintReceived == 0) {
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        FD_ZERO(&readfds);
        FD_SET(udpSocket, &readfds);

        if(select(udpSocket + 1, &readfds, NULL, NULL, &timeout) < 0) { // on select() error
            continue;
        }

        if(FD_ISSET(udpSocket, &readfds)) { // can read from UDP socket?
            socklen_t slen = sizeof(clientAddr);
            memset(&clientAddr, 0, sizeof(clientAddr));

            ssize_t cnt = recvfrom(udpSocket, recvData, sizeof(recvData), 0, (struct sockaddr *) &clientAddr, &slen);

            if(cnt >= 4 && memcmp(recvData, "CELC", 4) == 0) {   // CE Lite Client wants something?
                onClientRequest(&clientAddr, recvData, cnt);
            } else {
                logDiscovery(LOG_INFO, "ignoring unknown request %02X %02X %02X %02X, packet length: ", recvData[0], recvData[1], recvData[2], recvData[3], cnt);
            }
        }
    }

    close(udpSocket);

    logDiscovery(LOG_ERROR, "Terminating CosmosEx network server");
}

void getMacForIp(uint32_t clientIp, uint8_t* mac)
{
    memset(mac, 0, 6);      // start with zeros

    for(int i=0; i<IP_TO_MAC_COUNT; i++) {
        if(ipToMac[i].clientIp == clientIp) {   // found ip? copy mac
            memcpy(mac, ipToMac[i].mac, 6);
            break;
        }
    }
}

void onClientRequest(sockaddr_in *clientAddr, uint8_t *recvData, int len)
{
    uint32_t clientIp = clientAddr->sin_addr.s_addr;    // get client address

    // find slot for this ip-to-mac mapping
    int idx = 0;
    int lastTimeMin = ipToMac[0].lastTime;
    for(int i=0; i<IP_TO_MAC_COUNT; i++) {
        if(ipToMac[i].clientIp == clientIp || ipToMac[i].clientIp == 0) {   // existing slot or empty slot? use it
            idx = i;
            break;
        }

        if(ipToMac[i].lastTime != 0 && ipToMac[i].lastTime < lastTimeMin) {     // this slot has older time that the minimum so far?
            idx = i;
        }
    }

    // copy mac into the slot
    memcpy(ipToMac[idx].mac, recvData + 4, 6);
    ipToMac[idx].lastTime = time(NULL);

    // send response back
    uint8_t response[10];
    memset(response, 0, sizeof(response));  // clear all bytes
    memcpy(response, "CELR", 4);            // 0..3: CE Lite Response

    Utils::storeWord(response + 4, SERVER_TCP_PORT_HDD);
    Utils::storeWord(response + 6, SERVER_TCP_PORT_FDD);
    Utils::storeWord(response + 8, SERVER_TCP_PORT_IKBD);

    udpSend(clientIp, CLIENT_UDP_PORT, response, sizeof(response));     // send response to client with info about ports

    logDiscovery(LOG_INFO, "onClientRequest response - ports %d, %d, %d", SERVER_TCP_PORT_HDD, SERVER_TCP_PORT_FDD, SERVER_TCP_PORT_IKBD);
}

void udpSend(uint32_t ip, uint16_t port, uint8_t* data, uint16_t len)
{
    sockaddr_in servaddr;

    int sockFd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockFd < 0) {        // on error
        logDiscovery(LOG_ERROR, "updSend - failed to open socket for response");
        return;
    }

    bzero(&servaddr,sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = ip;
    servaddr.sin_port = htons(port);

    int rv = sendto(sockFd, data, len, 0, (sockaddr*) &servaddr, sizeof(servaddr));
    close(sockFd);

    if(rv < 0) {
        logDiscovery(LOG_ERROR, "updSend - failed to sendto() response");
    }
}
