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

void sigint_handler(int sig);
void handlePthreadCreate(int res, const char *threadName, pthread_t *pThread);
void loadLastHwConfig(void);
int  runCore(int instanceNo, bool localNotNetwork);

extern volatile sig_atomic_t sigintReceived;
extern THwConfig           hwConfig;                           // info about the current HW setup
extern TFlags              flags;                              // global flags from command line
extern RPiConfig           rpiConfig;                          // RPi model, revision, serial
extern InterProcessEvents  events;

#define MAIN_PORT       7200

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

            if(n < 1) {                     // empty packet or error? 
                continue;
            }

            recvData[n] = 0;                // zero terminate string

        }
    }

    Debug::out(LOG_ERROR, "Terminating CosmosEx network server");
}
