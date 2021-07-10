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

#include "settings.h"
#include "global.h"
#include "debug.h"
#include "utils.h"
#include "main_netserver.h"
#include "netservermainpage.h"

#include "webserver/webserver.h"
#include "webserver/api/apimodule.h"

void sigint_handler(int sig);
void handlePthreadCreate(int res, const char *threadName, pthread_t *pThread);
void loadLastHwConfig(void);
int  runCore(int instanceNo, bool localNotNetwork);

void onServerStatus(uint8_t* recvData, int len);
void onClientRequest(sockaddr_in *clientAddr, uint8_t *recvData, int len);
void forkCEliteServer(int serverIndex);
void udpSend(uint32_t ip, uint16_t port, uint8_t* data, uint16_t len);

extern volatile sig_atomic_t sigintReceived;
extern THwConfig           hwConfig;                           // info about the current HW setup
extern TFlags              flags;                              // global flags from command line
extern RPiConfig           rpiConfig;                          // RPi model, revision, serial
extern InterProcessEvents  events;

TCEServerStatus serverStatus[MAX_SERVER_COUNT];
std::string mainPage;
void generateMainPage(void);

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

    Debug::out(LOG_INFO, "Starting CosmosEx network server");
    printf("Starting CosmosEx network server");

    // init the server status structs
    for(int i=0; i<MAX_SERVER_COUNT; i++) {
        serverStatus[i].clientIp = 0;
        serverStatus[i].status = SERVER_STATUS_NOT_RUNNING;
        serverStatus[i].lastUpdate = Utils::getCurrentMs();
    }

    // start one server on index 0
    forkCEliteServer(0);
    Debug::out(LOG_INFO, "Starting first server on index # 0");

    // generate first main page
    generateMainPage();

    // start webserver with the main page
    WebServer xServer;
    xServer.start(true, 0);

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

            if(n < 8) {                                     // packet not big enough or error?
                Debug::out(LOG_DEBUG, "netServer: rejecting short UDP packet (%d bytes)", n);
                continue;
            }

            if(memcmp(recvData, "CELS", 4) == 0) {          // CE Lite Server tells us his status?
                onServerStatus(recvData, n);
            } else if(memcmp(recvData, "CELC", 4) == 0) {   // CE Lite Client wants something?
                onClientRequest(&clientAddr, recvData, n);
            } else {
                Debug::out(LOG_INFO, "netServer: ignoring unknown request %02X %02X %02X %02X", recvData[0], recvData[1], recvData[2], recvData[3]);
            }
        }
    }

    close(udpSocket);
    xServer.stop();

    Debug::out(LOG_ERROR, "Terminating CosmosEx network server");
}

void onServerStatus(uint8_t* recvData, int len)
{
    int index = recvData[4];                        // 4: server index - from 0 to (MAX_SERVER_COUNT-1)

    if(index >= MAX_SERVER_COUNT) {                 // if server index is more that we store, ignore it
        Debug::out(LOG_DEBUG, "netServer: onServerStatus ignoring status with invalid server index %d", index);
        return;
    }

    TCEServerStatus *ss = &serverStatus[index];     // get pointer to status struct
    ss->status = recvData[5];                       // 5: status
    ss->lastUpdate = Utils::getCurrentMs();         // updated time: now
    // don't modify clientIp here, it's not the IP of client but rather IP of server itself

    Debug::out(LOG_DEBUG, "netServer: onServerStatus at index: %d, status: %d", index, ss->status);

    generateMainPage();
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
        forkCEliteServer(idxNotRunning);    // start server on this index
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

        uint8_t bfr[10];
        Utils::getIpAdds(bfr);
        uint8_t *serverIp = NULL;

        if(bfr[0] == 1) {                   // eth0 enabled? add its IP
            serverIp = &bfr[1];
        } else if(bfr[5] == 1) {            // wlan0 enabled? add its IP
            serverIp = &bfr[6];
        }

        if(serverIp != NULL) {                      // if found valid IP address
            memcpy(&response[4], serverIp, 4);      // 4..7: store server's IP

            Debug::out(LOG_INFO, "onClientRequest response: use server #%d, on %d.%d.%d.%d, port: %d", 
                idxUse, serverIp[0], serverIp[1], serverIp[2], serverIp[3], SERVER_TCP_PORT_FIRST + idxUse);
        } else {
            Debug::out(LOG_INFO, "onClientRequest response: no valid IP of server");
        }

        Utils::storeWord(&response[8], SERVER_TCP_PORT_FIRST + idxUse);   // 8,9: server's port
    }

    // send response to client with info about this index
    udpSend(clientIp, CLIENT_UDP_PORT, response, sizeof(response));
}

void forkCEliteServer(int serverIndex)
{
    serverStatus[serverIndex].status = SERVER_STATUS_OCCUPIED;

    // fork process
    int childPid = fork();              // fork child to own process

    if(childPid == 0) {                 // code executed only by child
        char logPath[50];
        sprintf(logPath, "/var/log/ce_server_%d.log", serverIndex);
        Debug::setLogFile(logPath);     // new network server will use different log file than the main thread

        runCore(serverIndex, false);    // run network (not local) device server with this serverIndex
        return;
    }

    // parent (main server process) continues here
    Utils::sleepMs(500);         // give forked server bit time to start, then reply to client
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

void generateMainPage(void)
{
    // generate new report into string, check if changed since last time, if changed then write to file
    std::string mainPageNew;
    NetServerMainPage::create(mainPageNew);

    if(mainPage != mainPageNew) {   // main page changed?
        mainPage = mainPageNew;

        // create dir if it doesn't exist
        mkdir(NETSERVER_WEBROOT, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        // try to write page to file
        FILE *f = fopen(NETSERVER_WEBROOT_INDEX, "wt");

        if(f) { // if could open file, write and close
            fwrite(mainPage.c_str(), 1, mainPage.length(), f);
            fclose(f);
        }
    }
}
