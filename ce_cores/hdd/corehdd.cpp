// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <net/if.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <unistd.h>

#include "misc/global.h"
#include "misc/debug.h"
#include "corehdd.h"
#include "hddclient.h"
#include "translated/translateddisk.h"
#include "native/scsi.h"
#include "native/scsi_defs.h"
#include "acsicommand/screencastacsicommand.h"
#include "../misc/utils.h"
#include "../misc/statusreport.h"
#include "../chipinterface/chipinterface.h"
#include "../discovery/discovery.h"

#define DEV_CHECK_TIME_MS       3000
#define UPDATE_CHECK_TIME       1000
#define INET_IFACE_CHECK_TIME   1000
#define UPDATE_SCRIPTS_TIME     10000

extern DebugVars    dbgVars;

LoadTracker load;

CoreHdd::CoreHdd()
{

}

CoreHdd::~CoreHdd()
{

}

void CoreHdd::run(void)
{
    // inBuff might contain whole written floppy sector + header size
    uint8_t inBuff[INBUF_SIZE], outBuf[INBUF_SIZE];

    memset(outBuf, 0, INBUF_SIZE);
    memset(inBuff, 0, INBUF_SIZE);

    // create network chip interface
    ChipInterface* ciHdd = new ChipInterface(LOGFILE_HDD, NET_ATN_HANS_ID, SYNC_TAG_HDD, SERVER_TCP_PORT_HDD);
    ciHdd->ciOpen();

    int sharedMemFd;
    sem_t* sharedMemSemaphore;
    uint8_t* sharedMemPointer;
    ScreencastAcsiCommand::sharedMemoryOpen(sharedMemFd, &sharedMemSemaphore, &sharedMemPointer);

    // create hdd clients, 1 hdd client per each tcp client
    for(int i=0; i<MAX_CLIENTS; i++) {
        ClientInfo* ci = ciHdd->clientGetByIndex(i);
        hddClients[i] = new HddClient(ciHdd, ci, sharedMemFd, sharedMemSemaphore, sharedMemPointer);
    }

    //------------------------------
    int max_fd;
    fd_set readfds;
    bool someClientActive = false;

    while(sigintReceived == 0) {
        handleEvents();

        if(!someClientActive) {
            Utils::sleepMs(1);      // intentional sleep to not utilize cpu to max when looping too much before client disconnect
        }

        ciHdd->clientsDisconnectInactive();

        max_fd = -1;
        FD_ZERO(&readfds);

        // get listening socket from chip interface, add it to readfds
        int fdListen = ciHdd->getFdListen();
        FD_SET(fdListen, &readfds);
        max_fd = MAX(max_fd, fdListen);

        // get all connected client fds, add them to readfds
        max_fd = MAX(max_fd, ciHdd->setAllClientFds(&readfds));     // all valid client fds will be set to readfds, and highest fd into max_fd

        // add timeout to select(), so we can check for connection status, settings reload, etc.
        timeval timeout;
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        if(select(max_fd + 1, &readfds, NULL, NULL, &timeout) < 0) {
            if(errno == EINTR) {
                continue;   // a signal was delivered
            } else {
                logHdd(LOG_ERROR, "run() select: %s", strerror(errno));
                continue;
            }
        }

        // if listening socket is set, handle it
        if(FD_ISSET(fdListen, &readfds)) {
            int idx = ciHdd->acceptSocketIfNeededAndPossible();

            if(idx != FD_EMPTY) {
                hddClients[idx]->onMacUpdated();
            }
        }

        someClientActive = false;

        // check which fds are ready to be handled and handle them
        for(int i=0; i<MAX_CLIENTS; i++) {
            ClientInfo* ci = ciHdd->clientGetByIndex(i);

            if(ci->fdClient == FD_EMPTY) {           // no client here? skip it
                continue;
            }

            if(FD_ISSET(ci->fdClient, &readfds)) {    // this fd read for read?
                bool needsAction = ciHdd->actionNeeded(ci, inBuff);

                if(needsAction) {   // client #i needs action?
                    // logHdd(LOG_DEBUG, "client %d, fdClient %d needs action", i, ci->fdClient);

                    someClientActive = true;
                    hddClients[i]->handleHdd(inBuff);

                    ciHdd->dropRestOfData(ci, inBuff, INBUF_SIZE);
                    ci->lastMs = Utils::getCurrentMs();     // mark client as active
                }
            }
        }
    }

    // destroy hdd clients
    for(int i=0; i<MAX_CLIENTS; i++) {
        delete hddClients[i];
    }

    ScreencastAcsiCommand::sharedMemoryClose(sharedMemFd, &sharedMemSemaphore, &sharedMemPointer);

    // destroy chip interface
    ciHdd->ciClose();
    delete ciHdd;
}

void CoreHdd::displayStatusToConsole(uint32_t now)
{
    static int progress = 0;

    char progChars[4] = {'|', '/', '-', '\\'};
    printf("\033[2K  [ %c ]  CE core is running\033[A\n", progChars[progress]);
    progress = (progress + 1) % 4;
}

void CoreHdd::handleEvents(void)
{
    if(events.hddReloadRaw) {
        events.hddReloadRaw = false;

        for(int i=0; i<MAX_CLIENTS; i++) {
            hddClients[i]->reloadDrivesRaw();
        }
    }

    if(events.hddReloadTranslated) {
        events.hddReloadTranslated = false;

        for(int i=0; i<MAX_CLIENTS; i++) {
            hddClients[i]->reloadDrivesTranslated();
        }
    }
}
