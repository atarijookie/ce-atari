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

#include "settings.h"
#include "global.h"
#include "debug.h"
#include "utils.h"
#include "main_netserver.h"

void sigint_handler(int sig);
void handlePthreadCreate(int res, const char *threadName, pthread_t *pThread);
void loadLastHwConfig(void);
int  runCore(int instanceNo, bool localNotNetwork);

extern volatile sig_atomic_t sigintReceived;
extern THwConfig           hwConfig;                           // info about the current HW setup
extern TFlags              flags;                              // global flags from command line
extern RPiConfig           rpiConfig;                          // RPi model, revision, serial
extern InterProcessEvents  events;

#define MAX_SERVER_COUNT    16
TCEServerStatus serverStatus[MAX_SERVER_COUNT];

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
    servaddr.sin_port           = htons(MAIN_PORT);

    // bind the socket with the server address
    if (bind(sockfd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        Debug::out(LOG_ERROR, "Failed to bind() main server socket");
        return -1;
    }

    return sockfd;
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
        return;
    }

    Debug::out(LOG_ERROR, "Starting CosmosEx network server");
    printf("Starting CosmosEx network server");

    // init the server status structs
    for(int i=0; i<MAX_SERVER_COUNT; i++) {
        serverStatus[i].status = SERVER_STATUS_NOT_RUNNING;
        serverStatus[i].lastUpdate = Utils::getCurrentMs();
    }

    struct timeval timeout;
    fd_set readfds;
    uint8_t recvData[64];
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

            ssize_t n = recvfrom(udpSocket, recvData, sizeof(recvData), 0, (struct sockaddr *) &clientAddr, &slen);

            if(n < 4) {                                     // packet not big enough or error?
                continue;
            }

            uint32_t clientIp = clientAddr.sin_addr.s_addr; // get client address

            if(memcmp(recvData, "CELS", 4) == 0) {          // CE Lite Server tells us his status?
                int index = recvData[4];                    // 4: server index - from 0 to (MAX_SERVER_COUNT-1)

                if(index >= MAX_SERVER_COUNT || n < 8) {    // if server index is more that we store or packet too short, ignore it
                    continue;
                }

                TCEServerStatus *ss = &serverStatus[index]; // get pointer to status struct
                ss->port = Utils::getWord(&recvData[5]);    // 5,6: port
                ss->status = recvData[7];                   // 7: status
                ss->clientIp = clientIp;                    // store client IP
                ss->lastUpdate = Utils::getCurrentMs();     // updated time: now

            } else if(memcmp(recvData, "CELC", 4) == 0) {       // CE Lite Client wants something?
                int idxFree = -1;
                int idxClient = -1;
                int idxNotRunning = -1;

                for(int i=0; i<MAX_SERVER_COUNT; i++) {
                    if(serverStatus[i].clientIp == clientIp) {  // if we got this client IP already
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
                } else if(idxFree != -1) {          // got index where server is running but nobody is connected? use that
                    idxUse = idxFree;
                } else if(idxNotRunning == -1) {    // got index where no server is running? start it and run it
                    // TODO: start server on this index and fill serverStatus

                    idxUse = idxNotRunning;
                }

                // if idxUse is still -1, nothing is available

                // TODO: send response to client with info about this index

            }
        }
    }

    Debug::out(LOG_ERROR, "Terminating CosmosEx network server");
}
