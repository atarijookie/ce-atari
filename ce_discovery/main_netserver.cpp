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

#include "global.h"
#include "debug.h"
#include "utils.h"
#include "main_netserver.h"

extern int logLevel;
void sigint_handler(int sig);

void onServerStatus(uint8_t* recvData, int len);
void onClientRequest(sockaddr_in *clientAddr, uint8_t *recvData, int len);
void startHddCoreAtIndex(int serverIndex);
void udpSend(uint32_t ip, uint16_t port, uint8_t* data, uint16_t len);

volatile sig_atomic_t sigintReceived = 0;

TCEServerStatus serverStatus[MAX_SERVER_COUNT];

uint8_t serverIp[4] = {127, 0, 0, 1};

int netServerOpenSocket(void)
{
    int sockfd = 0;
    struct sockaddr_in servaddr;

    // create socket fd
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        Debug::out(LOG_ERROR, "Failed to create main server socket");
        return -1;
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // fill in server information
    servaddr.sin_family         = AF_INET;          // IPv4
    servaddr.sin_addr.s_addr    = INADDR_ANY;
    servaddr.sin_port           = htons(SERVER_UDP_PORT);

    // bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        Debug::out(LOG_ERROR, "Failed to bind() main server socket");
        return -1;
    }

    return sockfd;
}

const char *serverStatusAsString(int status)
{
    switch(status)
    {
        case SERVER_STATUS_NOT_RUNNING: return "NOT RUNNNING";
        case SERVER_STATUS_FREE: return "RUNNING, FREE";
        case SERVER_STATUS_OCCUPIED: return "RUNNING, IN USE";
        default: return "UNKNOWN";
    }
}

void checkForDeadCores(void)
{
    // Debug::out(LOG_DEBUG, "Checking for dead cores.");

    for(int i=0; i<MAX_SERVER_COUNT; i++) {
        // don't check not running instances
        if(serverStatus[i].status == SERVER_STATUS_NOT_RUNNING)
        {
            continue;
        }

        // no status update for a while? probably stuck or not running
        uint32_t now = Utils::getCurrentMs();
        if((now - serverStatus[i].lastUpdate) > 15000)
        {
            if(serverStatus[i].pid)
            {
                Debug::out(LOG_WARNING, "No status report from server #%d, terminating it by pid: %d", i, serverStatus[i].pid);

                char cmd[64];
                snprintf(cmd, sizeof(cmd), "kill %d &", serverStatus[i].pid);
                system(cmd);

                Utils::sleepMs(500);
                startHddCoreAtIndex(i);
            } else {
                Debug::out(LOG_WARNING, "No status report from server #%d, but has pid 0 (wrong), not terminating, just clearing struct", i);
            }

            serverStatus[i].clientIp = 0;
            serverStatus[i].status = SERVER_STATUS_NOT_RUNNING;
            serverStatus[i].lastUpdate = 0;
            serverStatus[i].pid = 0;
        }
    }
}

void networkServerMain(void)
{
    // register signal handlers
    if(signal(SIGINT, sigint_handler) == SIG_ERR) {         // register SIGINT handler
        printf("Cannot register SIGINT handler!\n");
    }

    if(signal(SIGHUP, sigint_handler) == SIG_ERR) {         // register SIGHUP handler
        printf("Cannot register SIGHUP handler!\n");
    }

    int udpSocket = netServerOpenSocket();                  // create main UDP socket

    if(udpSocket < 0) {     // on error, just quit
        Debug::out(LOG_ERROR, "Could not open port %d, terminating", SERVER_UDP_PORT);
        printf("Could not open port %d, terminating", SERVER_UDP_PORT);
        return;
    }

    Debug::out(LOG_INFO, "Starting CosmosEx discovery server");
    printf("Starting CosmosEx discovery server");

    // init the server status structs
    for(int i=0; i<MAX_SERVER_COUNT; i++) {
        serverStatus[i].clientIp = 0;
        serverStatus[i].status = SERVER_STATUS_NOT_RUNNING;
        serverStatus[i].lastUpdate = Utils::getCurrentMs();
        serverStatus[i].pid = 0;
    }

    // start one server on index 0
    startHddCoreAtIndex(0);
    Debug::out(LOG_INFO, "Starting first server on index # 0");

    struct timeval timeout;
    fd_set readfds;
    uint8_t recvData[256];
    struct sockaddr_in clientAddr;

    // This is main network server loop, which does the following:
    // - waits for and responds to requests from CE lite (telling CE lite the IP and port of server)
    // - holds the list of running servers and their status
    // - checks if any running servers is free and spawns a new one if it isn't
    while(sigintReceived == 0) {
        checkForDeadCores();

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

            if(cnt >= 11 && memcmp(recvData, "CELS", 4) == 0) {          // CE Lite Server tells us his status?
                onServerStatus(recvData, cnt);
            } else if(cnt >= 4 && memcmp(recvData, "CELC", 4) == 0) {   // CE Lite Client wants something?
                onClientRequest(&clientAddr, recvData, cnt);
            } else {
                Debug::out(LOG_INFO, "ignoring unknown request %02X %02X %02X %02X, packet length: ", recvData[0], recvData[1], recvData[2], recvData[3], cnt);
            }
        }
    }

    close(udpSocket);

    Debug::out(LOG_ERROR, "Terminating CosmosEx network server");
}

void onServerStatus(uint8_t* recvData, int len)
{
    int clientsPort = (int) Utils::getWord(recvData + 4);     // port on index 4..5
    int index = clientsPort - SERVER_TCP_PORT_HDD_FIRST;

    if(index < 0 || index >= MAX_SERVER_COUNT) {            // if server index is more that we store, ignore it
        Debug::out(LOG_DEBUG, "onServerStatus ignoring status with invalid server port: %d,  index: %d", clientsPort, index);
        return;
    }

    TCEServerStatus *ss = &serverStatus[index];     // get pointer to status struct
    ss->status = recvData[6];                       // 5: status
    ss->lastUpdate = Utils::getCurrentMs();         // updated time: now
    ss->pid = (int) Utils::getDword(recvData + 7);  // process pid on index 7..10

    Debug::out(LOG_DEBUG, "onServerStatus at index: %d, status: %d (%s)", index, ss->status, serverStatusAsString(ss->status));
}

void onClientRequest(sockaddr_in *clientAddr, uint8_t *recvData, int len)
{
    uint32_t clientIp = clientAddr->sin_addr.s_addr;    // get client address

    int idxFree = -1;
    int idxClient = -1;
    int idxNotRunning = -1;

    for(int i=0; i<MAX_SERVER_COUNT; i++) {
        if(idxClient == -1 && serverStatus[i].clientIp == clientIp) {  // if we got this client IP already
            idxClient = i;
        }

        if(idxFree == -1 && serverStatus[i].status == SERVER_STATUS_FREE) { // didn't find free slot yet, but found one now?
            idxFree = i;
        }

        if(idxNotRunning == -1 &&  serverStatus[i].status == SERVER_STATUS_NOT_RUNNING) {   // didn't find not running slot but found one now?
            idxNotRunning = i;
        }
    }

    int idxUse = -1;                    // which index should we use?

    if(idxClient != -1) {               // got index where client is / was connected? use that
        idxUse = idxClient;
        Debug::out(LOG_INFO, "onClientRequest - reusing server # %d for client", idxUse);
    } else if(idxFree != -1) {          // got index where server is running but nobody is connected? use that
        idxUse = idxFree;
        Debug::out(LOG_INFO, "onClientRequest - using free server # %d", idxUse);
    } else if(idxNotRunning != -1) {        // got index where no server is running? start it and run it
        startHddCoreAtIndex(idxNotRunning);    // start server on this index
        idxUse = idxNotRunning;
        Debug::out(LOG_INFO, "onClientRequest - forking and using new server # %d", idxUse);
    } else {
        Debug::out(LOG_INFO, "onClientRequest - couldn't find free server slot to use");
    }

    uint8_t response[10];
    memset(response, 0, sizeof(response));  // clear all bytes
    memcpy(response, "CELR", 4);            // 0..3: CE Lite Response

    if(idxUse != -1) {                      // if idxUse is not -1, fill in the response (and response with all zeros means nothing available)
        serverStatus[idxUse].clientIp = clientIp;

        int hddCorePort = SERVER_TCP_PORT_HDD_FIRST + idxUse;
        Utils::storeWord(response + 4, hddCorePort);
        Utils::storeWord(response + 6, SERVER_TCP_PORT_FDD);
        Utils::storeWord(response + 8, SERVER_TCP_PORT_IKBD);

        Debug::out(LOG_INFO, "onClientRequest response: use server #%d, port: %d", idxUse, hddCorePort);

        // send response to client with info about this index
        udpSend(clientIp, CLIENT_UDP_PORT, response, sizeof(response));
    }
    else
    {
        Debug::out(LOG_INFO, "onClientRequest no slot available, not responding.");
    }
}

void startHddCoreAtIndex(int serverIndex)
{
    serverStatus[serverIndex].status = SERVER_STATUS_OCCUPIED;
    char cmd[256];
    int clientPort = SERVER_TCP_PORT_HDD_FIRST + serverIndex;
    snprintf(cmd, sizeof(cmd), "./ce_hdd.elf ll%d p%d r%d > /dev/null 2>&1 &", logLevel, clientPort, SERVER_UDP_PORT);
    system(cmd);

    Debug::out(LOG_INFO, "startHddCoreAtIndex - serverIndex: %d, clientPort: %d", serverIndex, clientPort);
}

void udpSend(uint32_t ip, uint16_t port, uint8_t* data, uint16_t len)
{
    sockaddr_in servaddr;

    int sockFd = socket(AF_INET, SOCK_DGRAM, 0);

    if(sockFd < 0) {        // on error
        Debug::out(LOG_ERROR, "updSend - failed to open socket for response");
        return;
    }

    bzero(&servaddr,sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = ip;
    servaddr.sin_port = htons(port);

    int rv = sendto(sockFd, data, len, 0, (sockaddr*) &servaddr, sizeof(servaddr));
    close(sockFd);

    if(rv < 0) {
        Debug::out(LOG_ERROR, "updSend - failed to sendto() response");
    }
}

void sigint_handler(int sig)
{
    Debug::out(LOG_DEBUG, "Some SIGNAL received, terminating.");
    sigintReceived = 1;
}
