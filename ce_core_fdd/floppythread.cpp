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

#include "global.h"
#include "debug.h"
#include "floppythread.h"
#include "utils.h"
#include "chipinterface_network/chipinterfacenetwork.h"

#include "floppy/imagesilo.h"
#include "floppy/floppyencoder.h"

#define DEV_CHECK_TIME_MS       3000
#define UPDATE_CHECK_TIME       1000
#define INET_IFACE_CHECK_TIME   1000
#define UPDATE_SCRIPTS_TIME     10000

extern TFlags       flags;
extern ChipInterfaceNetwork* chipInterface;

extern DebugVars    dbgVars;

extern SharedObjects shared;

struct TLastFwInfoTime {
    uint32_t franz;
    int     progress;
};

TLastFwInfoTime lastFwInfoTime;

FloppyThread::FloppyThread()
{
    sharedObjects_create();
}

FloppyThread::~FloppyThread()
{
    sharedObjects_destroy();
}

void FloppyThread::sharedObjects_create(void)
{
    shared.imageSilo = new ImageSilo();
}

void FloppyThread::sharedObjects_destroy(void)
{
    delete shared.imageSilo;
    shared.imageSilo = NULL;
}

void FloppyThread::run(void)
{
    loadSettings();

    int max_fd;
    fd_set readfds;

    while(sigintReceived == 0) {
        Utils::sleepMs(1);      // intentional sleep to not utilize cpu to max when looping too much before client disconnect

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
        timeout.tv_sec = 2;

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

        // check which fds are ready to be handled and handle them
        chipInterface->handleAllReadyClients(&readfds, this);
    }
}

bool FloppyThread::handleOneClient(int clientIndex, int fdClient, int floppySlotIndex)
{
    uint8_t inBuff[INBUF_SIZE];
    memset(inBuff, 0, INBUF_SIZE);

    bool needsAction = chipInterface->actionNeeded(clientIndex, inBuff);

    if(needsAction) {   // floppy drive needs action?
        handleFdd(clientIndex, fdClient, floppySlotIndex, inBuff);
    }

    return needsAction;
}

bool FloppyThread::handleFdd(int clientIndex, int fdClient, int floppySlotIndex, uint8_t* inBuff)
{
    bool isFddCommand = false;

    switch(inBuff[3]) {
    case ATN_FW_VERSION:                    // device has sent FW version
        lastFwInfoTime.franz = Utils::getCurrentMs();
        handleFwVersion_franz(clientIndex);
        break;

    case ATN_SECTOR_WRITTEN:                // device has sent written sector data
        isFddCommand = true;
        handleSectorWritten(clientIndex);
        break;

    case ATN_SEND_TRACK:                    // device requests data of a whole track
        isFddCommand = true;
        handleSendTrack(clientIndex);
        break;

    case ATN_SEND_WHOLE_IMAGE:
        isFddCommand = true;
        handleSendImage(clientIndex);
        break;

    default:
        logFdd(LOG_ERROR, "FloppyThread received weird ATN code %02x waitForATN()", inBuff[3]);
        break;
    }

    return isFddCommand;
}

void FloppyThread::displayStatusToConsole(uint32_t now)
{
    char progChars[4] = {'|', '/', '-', '\\'};
    printf("\033[2K  [ %c ]  CE core is running\033[A\n", progChars[lastFwInfoTime.progress]);
    lastFwInfoTime.progress = (lastFwInfoTime.progress + 1) % 4;
}

void FloppyThread::reloadSettings(int type)
{
    // then load the new settings
    loadSettings();
}

void FloppyThread::loadSettings(void)
{
    logFdd(LOG_DEBUG, "FloppyThread::loadSettings");
}

void FloppyThread::handleFwVersion_franz(int clientIndex)
{
    // chipInterface->setFDDconfig(setFloppyConfig, &floppyConfig, setDiskChanged, diskChanged);
    chipInterface->getFWversion(clientIndex);
}

void FloppyThread::handleSendTrack(int clientIndex)
{
    #define BFR_SIZE    32
    uint8_t inBuf[BFR_SIZE];
    int readCnt = chipInterface->readRestOfData(clientIndex, inBuf, BFR_SIZE);

    if(readCnt < 2) {
        logFdd(LOG_ERROR, "handleSendTrack() -- not enough data received: %d", readCnt);
        return;
    }

    int side = inBuf[0];                      // now read the current floppy position
    int track = inBuf[1];

    ClientInfo* client = chipInterface->clientsGetOne(clientIndex);

    int tr, si, spt;
    shared.imageSilo->getParams(client->floppySlotIndex, tr, si, spt);      // read the floppy image params

    uint8_t *encodedTrack;
    int countInTrack;

    if(side < 0 || side > 1 || track < 0 || track >= tr) {      // side / track out of range? use empty track
        logFdd(LOG_ERROR, "handleSendTrack() -- side / track out of range, returning empty track. Franz wants: [track %d, side %d], but current image has %d tracks, %d sides, %d sectors/track", track, side, tr, si, spt);

        encodedTrack = shared.imageSilo->getEmptyTrack();
        countInTrack = MFM_STREAM_SIZE;
    } else {                                                    // side + track within range? use encoded track
        logFdd(LOG_DEBUG, "handleSendTrack() -- Franz wants: [track %d, side %d]", track, side);

        encodedTrack = shared.imageSilo->getEncodedTrack(client->floppySlotIndex, track, side, countInTrack);
        countInTrack = MIN(countInTrack, MFM_STREAM_SIZE);
    }

    chipInterface->fdd_sendTrackToChip(client->fdClient, countInTrack, encodedTrack);
}

void FloppyThread::handleSendImage(int clientIndex)
{
    #define BFR_SIZE    32
    uint8_t inBuf[BFR_SIZE];

    chipInterface->readRestOfData(clientIndex, inBuf, BFR_SIZE);

    ClientInfo* client = chipInterface->clientsGetOne(clientIndex);

    std::string fileName = shared.imageSilo->getFileName(client->floppySlotIndex);   // get the filename

    int imgTracks, imgSides, imgSectorsPerTrack;
    shared.imageSilo->getParams(client->floppySlotIndex, imgTracks, imgSides, imgSectorsPerTrack);      // read the floppy image params

    uint8_t *encodedTrack;
    int countInTrack;

    // sending started, send imagTracks, imgSides, spt
    chipInterface->fdd_sendImageParamsToChip(client->fdClient, false, imgTracks, imgSides, imgSectorsPerTrack, fileName);

    // send all the tracks from all the sides
    for(int side=0; side<imgSides; side++) {
        for(int track=0; track<imgTracks; track++) {
            // logFdd(LOG_DEBUG, "handleSendImage -- sending track %d, side %d, countInTrack: %d", track, side, countInTrack);
            encodedTrack = shared.imageSilo->getEncodedTrack(client->floppySlotIndex, track, side, countInTrack);
            countInTrack = MIN(countInTrack, MFM_STREAM_SIZE);
            chipInterface->fdd_sendTrackToChip(client->fdClient, countInTrack, encodedTrack);
        }
    }

    // sending finished, send imagTracks, imgSides, spt, this will also do the disk change
    chipInterface->fdd_sendImageParamsToChip(client->fdClient, true, imgTracks, imgSides, imgSectorsPerTrack, fileName);
}

void FloppyThread::handleSectorWritten(int clientIndex)
{
    int side, track, sector, byteCount;
    uint8_t *writtenSector = chipInterface->fdd_sectorWritten(clientIndex, side, track, sector, byteCount); // get side + track + sector number, byte count, and pointer to buffer where the written data is

    ClientInfo* client = chipInterface->clientsGetOne(clientIndex);

    logFdd(LOG_DEBUG, "handleSectorWritten -- track %d, side %d, sector %d", track, side, sector);
    floppyEncoder_decodeMfmWrittenSector(client->floppySlotIndex, track, side, sector, writtenSector, byteCount); // let floppy encoder handle decoding, reencoding, saving
}
