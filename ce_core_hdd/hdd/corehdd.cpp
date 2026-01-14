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

#include "misc/global.h"
#include "misc/debug.h"
#include "hdd/corehdd.h"
#include "hdd/hddclient.h"
#include "translated/translateddisk.h"
#include "native/scsi.h"
#include "native/scsi_defs.h"
#include "../misc/utils.h"
#include "../misc/statusreport.h"
#include "../chipinterface/chipinterface.h"

#define DEV_CHECK_TIME_MS       3000
#define UPDATE_CHECK_TIME       1000
#define INET_IFACE_CHECK_TIME   1000
#define UPDATE_SCRIPTS_TIME     10000

extern DebugVars    dbgVars;

struct TLastFwInfoTime {
    uint32_t hans;
    uint32_t franz;
    uint32_t nextDisplay;

    uint32_t hansResetTime;
    uint32_t franzResetTime;

    int     progress;
};

TLastFwInfoTime lastFwInfoTime;
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
    ChipInterface* chipInterface = new ChipInterface(LOGFILE_HDD, NET_ATN_HANS_ID, SYNC_TAG_HDD);
    chipInterface->ciOpen();

    // create hdd clients, 1 hdd client per each tcp client
    for(int i=0; i<MAX_CLIENTS; i++) {
        ClientInfo* ci = chipInterface->clientGetByIndex(i);
        hddClients[i] = new HddClient(chipInterface, ci->fdClient);
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

        chipInterface->clientsDisconnectInactive();

        max_fd = -1;
        FD_ZERO(&readfds);

        // get listening socket from chip interface, add it to readfds
        int fdListen = chipInterface->getFdListen();
        FD_SET(fdListen, &readfds);
        max_fd = MAX(max_fd, fdListen);

        // get all connected client fds, add them to readfds
        max_fd = MAX(max_fd, chipInterface->setAllClientFds(&readfds));     // all valid client fds will be set to readfds, and highest fd into max_fd

        // add timeout to select(), so we can check for connection status, settings reload, etc.
        timeval timeout;
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        if(select(max_fd + 1, &readfds, NULL, NULL, &timeout) < 0) {
            if(errno == EINTR) {
                continue;   // a signal was delivered
            } else {
                logFdd(LOG_ERROR, "FloppyThread::run() select: %s", strerror(errno));
                continue;
            }
        }

        // if listening socket is set, handle it
        if(FD_ISSET(fdListen, &readfds)) {
            chipInterface->acceptSocketIfNeededAndPossible();
        }

        someClientActive = false;

        // check which fds are ready to be handled and handle them
        for(int i=0; i<MAX_CLIENTS; i++) {
            ClientInfo* ci = chipInterface->clientGetByIndex(i);

            if(ci->fdClient == FD_EMPTY) {           // no client here? skip it
                continue;
            }

            if(FD_ISSET(ci->fdClient, &readfds)) {    // this fd read for read?
                bool needsAction = chipInterface->actionNeeded(i, inBuff);

                if(needsAction) {   // client #i needs action?
                    someClientActive = true;
                    hddClients[i]->handleHdd(inBuff);

                    chipInterface->dropRestOfData(i, inBuff, INBUF_SIZE);
                    ci->lastMs = Utils::getCurrentMs();     // mark client as active
                }
            }
        }
    }

    // destroy hdd clients
    for(int i=0; i<MAX_CLIENTS; i++) {
        delete hddClients[i];
    }

    // destroy chip interface
    chipInterface->ciClose();
    delete chipInterface;
}

void CoreHdd::displayStatusToConsole(uint32_t now)
{
    char progChars[4] = {'|', '/', '-', '\\'};

    printf("\033[2K  [ %c ]  CE HDD core is running\033[A\n", progChars[lastFwInfoTime.progress]);
    lastFwInfoTime.progress = (lastFwInfoTime.progress + 1) % 4;
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
