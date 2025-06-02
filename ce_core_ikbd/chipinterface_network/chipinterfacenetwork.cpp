#include <string.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <sys/select.h>
#include <sys/inotify.h>
#include <sys/socket.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>  // For TCP_NODELAY

#include "../utils.h"
#include "../debug.h"
#include "../global.h"
#include "chipinterfacenetwork.h"
#include "../ikbd/ikbd.h"

extern TFlags    flags;                 // global flags from command line

#define SERVER_STATUS_NOT_RUNNING   0       // when this server slot is not used yet and server is not running
#define SERVER_STATUS_FREE          1       // server is running but no client is connected there
#define SERVER_STATUS_OCCUPIED      2       // server is running and client is connected

ChipInterfaceNetwork::ChipInterfaceNetwork()
{
    fdListen = FD_EMPTY;

    for(int i=0; i<MAX_CLIENTS; i++) {
        fdClients[i] = FD_EMPTY;
        clientLastMs[i] = 0;
    }
}

ChipInterfaceNetwork::~ChipInterfaceNetwork()
{

}

int ChipInterfaceNetwork::getEmptyClientIndex(void)
{
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(fdClients[i] == FD_EMPTY) {
            return i;
        }
    }

    return -1;
}

void ChipInterfaceNetwork::createListeningSocket(void)
{
    if(fdListen >= 0) { // if the listening socket seems to be already created, just quit
        return;
    }

    // open socket
    if ((fdListen = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        Debug::out(LOG_ERROR, "netServer - failed to open socket");
        return;
    }

    // Forcefully attach socket to the port
    int opt = 1;

    if (setsockopt(fdListen, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        Debug::out(LOG_ERROR, "netServer - setsockopt() failed");
        return;
    }

    // change the socket into non-blocking
    fcntl(fdListen, F_SETFL, O_NONBLOCK);

    addressListen.sin_family = AF_INET;
    addressListen.sin_addr.s_addr = INADDR_ANY;
    addressListen.sin_port = htons(flags.portClient);

    // bind to address
    if (bind(fdListen, (struct sockaddr *) &addressListen, sizeof(addressListen)) < 0) {
        Debug::out(LOG_ERROR, "netServer - bind() failed");
        return;
    }

    // mark the socket as a passive socket
    if (listen(fdListen, 5) < 0) {      // listen with backlog of this many connections
        Debug::out(LOG_ERROR, "netServer - listen() failed");
        return;
    }

    Debug::out(LOG_INFO, "netServer - listening on tcp port: %d", flags.portClient);
}

int ChipInterfaceNetwork::getFdListen(void)
{
    return fdListen;
}

void ChipInterfaceNetwork::acceptSocketIfNeededAndPossible(void)
{
    int idx = getEmptyClientIndex();

    // out of empty indexes, don't accept
    if(idx < 0 || idx >= MAX_CLIENTS) {
        return;
    }

    // don't have client socket, try accept()
    struct sockaddr_in addressClient;
    socklen_t addrSize = sizeof(addressClient);

    int newSock = accept(fdListen, (struct sockaddr *) &addressClient, &addrSize);

    if(newSock < 0) {       // nothing to accept, would block? quit
        return;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;    // 500 ms timeout on blocking reads
    setsockopt(newSock, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv));

    // turn off Nagle's algorithm
    int flag = 1;
    setsockopt(newSock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    // got the new client socket now
    fdClients[idx] = newSock;
    clientLastMs[idx] = Utils::getCurrentMs();

    char clientIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addressClient.sin_addr, clientIp, sizeof(clientIp));

    Debug::out(LOG_INFO, "acceptSocketIfNeededAndPossible() - client #%d connected from %s", idx, clientIp);
}

bool ChipInterfaceNetwork::ciOpen(void)
{
    createListeningSocket();

    return true;
}

void ChipInterfaceNetwork::ciClose(void)
{
    // close sockets
    Utils::closeFdIfOpen(fdListen);

    for(int i=0; i<MAX_CLIENTS; i++) {
        Utils::closeFdIfOpen(fdClients[i]);
    }
}

int ChipInterfaceNetwork::disconnectInactiveClients(void)
{
    int maxFd = -1;
    uint32_t now = Utils::getCurrentMs();

    for(int i=0; i<MAX_CLIENTS; i++) {
        if(fdClients[i] == FD_EMPTY) {      // no client at this index? skip
            continue;
        }

        // there hasn't been any data from this client for some time? close connection
        uint32_t diff = now - clientLastMs[i];

        if(diff > 15000) {
            Utils::closeFdIfOpen(fdClients[i]);
            clientLastMs[i] = 0;
            Debug::out(LOG_INFO, "disconnected inactive client #%i", i);
        }
    }

    return maxFd;
}

int ChipInterfaceNetwork::setAllClientFds(fd_set* readfds)
{
    int maxFd = -1;

    for(int i=0; i<MAX_CLIENTS; i++) {
        if(fdClients[i] != FD_EMPTY) {      // got this client?
            FD_SET(fdClients[i], readfds);
            maxFd = MAX(fdClients[i], maxFd);
        }
    }

    return maxFd;
}

void ChipInterfaceNetwork::handleAllReadyClients(bool skipKeyboardTranslation, fd_set* readfds, Ikbd* ikbd)
{
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(fdClients[i] == FD_EMPTY) {      // no client here? skip it
            continue;
        }

        if(FD_ISSET(fdClients[i], readfds)) {           // this fd read for read?
            // process the incoming data from original keyboard and from ST
            int bytesRead = ikbd->processReceivedCommands(skipKeyboardTranslation, fdClients[i]);

            if(bytesRead > 0) {     // something was read, mark client as active
                clientLastMs[i] = Utils::getCurrentMs();
            }
        }
    }
}

void ChipInterfaceNetwork::ikbdUartWriteToAll(uint8_t* bfr, int len)
{
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(fdClients[i] == FD_EMPTY) {      // client not connected here? skip it
            continue;
        }

        write(fdClients[i], bfr, len);    // send it
    }
}
