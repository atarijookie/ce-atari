// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/un.h>
#include<sys/socket.h>

#include <signal.h>
#include <pthread.h>

#include "utils.h"
#include "debug.h"

#include "global.h"
#include "ccorethread.h"
#include "native/scsi.h"
#include "native/scsi_defs.h"
#include "update.h"

extern THwConfig    hwConfig;
extern TFlags       flags;

int createSocket(void)
{
	int sock;

	// create a UNIX DGRAM socket
	if ((sock = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
	    Debug::out(LOG_ERROR, "cmdSockThreadCode: Failed to create command Socket!");
	    return -1;
	}

    std::string sockPath = Utils::dotEnvValue("CORE_SOCK_PATH");
    struct sockaddr_un addr;
    strcpy(addr.sun_path, sockPath.c_str());
    addr.sun_family = AF_UNIX;
    bind (sock, (struct sockaddr *) &addr, strlen(addr.sun_path) + sizeof (addr.sun_family));

    if (bind(sock, (struct sockaddr *) &addr, strlen(addr.sun_path) + sizeof (addr.sun_family)) == -1) {
	    Debug::out(LOG_ERROR, "cmdSockThreadCode: Failed to bind command Socket to %s !", sockPath.c_str());
	    return -1;
    }

    return sock;
}

void *cmdSockThreadCode(void *ptr)
{
    Debug::out(LOG_INFO, "Command Socket thread starting...");
    int sock = createSocket();

    if(sock < 0) {              // without socket this thread has no use
        return 0;
    }

    char bfr[1024];

    while(sigintReceived == 0) {
        struct timeval timeout;
        timeout.tv_sec = 1;                             // short timeout
        timeout.tv_usec = 0;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        int res = select(sock + 1, &readfds, NULL, NULL, &timeout);     // wait for data or timeout here

        if(res < 0 || !FD_ISSET(sock, &readfds)) {          // if select() failed or cannot read from fd, skip rest
            continue;
        }

        ssize_t recvCnt = recv(sock, bfr, sizeof(bfr), 0);  // receive now

        if(recvCnt < 1) {                                   // nothing received?
            continue;
        }

        Debug::out(LOG_DEBUG, "cmdSockThreadCode: received: %s", bfr);
    }

    Debug::out(LOG_INFO, "Command Socket thread terminated.");
    return 0;
}
