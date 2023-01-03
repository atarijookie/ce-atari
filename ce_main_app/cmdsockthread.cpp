// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>

#include <signal.h>
#include <pthread.h>

#include "utils.h"
#include "debug.h"

#include "global.h"
#include "periodicthread.h"
#include "floppy/imagesilo.h"
#include "ccorethread.h"
#include "native/scsi.h"
#include "native/scsi_defs.h"
#include "update.h"
#include "json.h"

using json = nlohmann::json;

extern THwConfig        hwConfig;
extern TFlags           flags;
extern SharedObjects    shared;

void handleFloppyAction(std::string& action, json& data);

int createSocket(void)
{
	// create a UNIX DGRAM socket
	int sock = socket(AF_UNIX, SOCK_DGRAM, 0);

	if (sock < 0) {
	    Debug::out(LOG_ERROR, "cmdSockThreadCode: Failed to create command Socket!");
	    return -1;
	}

    std::string sockPath = Utils::dotEnvValue("CORE_SOCK_PATH");
    unlink(sockPath.c_str());       // delete sock file if exists

    struct sockaddr_un addr;
    strcpy(addr.sun_path, sockPath.c_str());
    addr.sun_family = AF_UNIX;

    int res = bind(sock, (struct sockaddr *) &addr, strlen(addr.sun_path) + sizeof (addr.sun_family));
    if (res < 0) {
	    Debug::out(LOG_ERROR, "cmdSockThreadCode: Failed to bind command socket to %s - errno: %d", sockPath.c_str(), errno);
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

        if(bfr[0] != 0) {           // if buffer doesn't seem to be empty, clear it now
            memset(bfr, 0, sizeof(bfr));
        }

        ssize_t recvCnt = recv(sock, bfr, sizeof(bfr), 0);  // receive now

        if(recvCnt < 1) {                                   // nothing received?
            continue;
        }

        Debug::out(LOG_DEBUG, "cmdSockThreadCode: received: %s", bfr);

        json data;
        try {
            data = json::parse(bfr);   // try to parse the message
        }
        catch(...)                          // on any exception - log it, don't crash
        {
            std::exception_ptr p = std::current_exception();
            Debug::out(LOG_ERROR, "json::parse raised an exception: %s", (p ? p.__cxa_exception_type()->name() : "null"));
        }

        if(data.contains("module") && data.contains("action")) {    // mandatory fields found?
            std::string module = data["module"].get<std::string>();
            std::string action = data["action"].get<std::string>();

            Debug::out(LOG_DEBUG, "cmdSockThreadCode: module: %s, action: %s", module.c_str(), action.c_str());

            if(module == "floppy") {                // for floppy module?
                handleFloppyAction(action, data);
            } else {                                // for uknown module?
                Debug::out(LOG_WARNING, "cmdSockThreadCode: uknown module '%s', ignoring message!", module.c_str());
            }
        } else {        // some mandatory field is missing?
            Debug::out(LOG_WARNING, "cmdSockThreadCode: module or action is missing in the received data, ignoring message!");
        }
    }

    Debug::out(LOG_INFO, "Command Socket thread terminated.");
    return 0;
}

void handleFloppyAction(std::string& action, json& data)
{
    int slot = -1;
    pthread_mutex_lock(&shared.mtxImages);      // lock floppy images shared objects

    if(data.contains("slot")) {                 // if can get slot, get slot number
        slot = data["slot"].get<int>();
    }

    if(action == "insert") {                    // for 'insert' action
        if(data.contains("image")) {            // image is present in message
            std::string empty;
            std::string pathAndFile = data["image"].get<std::string>();     // get image full path

            std::string path, file;
            Utils::splitFilenameFromPath(pathAndFile, path, file);          // get just filename from full path

            shared.imageSilo->add(slot, file, pathAndFile, empty, true);    // insert into slot
        } else {
            Debug::out(LOG_WARNING, "handleFloppyAction: missing 'image' in message, ignoring message!");
        }
    } else if(action == "eject") {              // for 'eject' action
        shared.imageSilo->remove(slot);
    } else {
        Debug::out(LOG_WARNING, "handleFloppyAction: unknown action '%s', ignoring message!", action.c_str());
    }

    pthread_mutex_unlock(&shared.mtxImages);    // unlock floppy images shared objects
}
