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
    }

    lastTimeRecv = Utils::getCurrentMs();
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
    if (listen(fdListen, 1) < 0) {
        Debug::out(LOG_ERROR, "netServer - listen() failed");
        return;
    }

    Debug::out(LOG_INFO, "netServer - listening on tcp port: %d", flags.portClient);
}

void ChipInterfaceNetwork::acceptSocketIfNeededAndPossible(void)
{
    int idx = getEmptyClientIndex();

    // out of empty indexes, don't accept
    if(idx < 0 || idx >= MAX_CLIENTS) {
        return;
    }

    // don't have client socket, try accept()
    socklen_t addrSize = sizeof(addressListen);

    int newSock = accept(fdListen, (struct sockaddr *) &addressListen, &addrSize);

    if(newSock < 0) {       // nothing to accept, would block? quit
        return;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;    // 500 ms timeout on blocking reads
    setsockopt(newSock, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv));

    // got the new client socket now
    fdClients[idx] = newSock;
    lastTimeRecv = Utils::getCurrentMs();

    Debug::out(LOG_DEBUG, "acceptSocketIfNeededAndPossible() - client connected");
}

void ChipInterfaceNetwork::closeClientSocket(void)
{
    // Utils::closeFdIfOpen(fdClient);            // close socket

    Debug::out(LOG_DEBUG, "closeClientSocket() - client disconnected");
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

int ChipInterfaceNetwork::setAllClientFds(fd_set* readfds)
{
    int maxFd = -1;

    for(int i=0; i<MAX_CLIENTS; i++) {
        if(fdClients[i] != FD_EMPTY) {      // got this client? add his fd
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

        if(FD_ISSET(fdClients[i], readfds)) {      // this fd read for read?
            // process the incoming data from original keyboard and from ST
            ikbd->processReceivedCommands(skipKeyboardTranslation, fdClients[i]);
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
