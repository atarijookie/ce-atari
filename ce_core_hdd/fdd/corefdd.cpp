#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <errno.h>
#include <net/if.h>

#include "../misc/global.h"
#include "../misc/debug.h"
#include "../misc/utils.h"
#include "../chipinterface/chipinterface.h"
#include "../discovery/discovery.h"

#include "corefdd.h"
#include "imagesilo.h"
#include "floppyencoder.h"

#define DEV_CHECK_TIME_MS       3000
#define UPDATE_CHECK_TIME       1000
#define INET_IFACE_CHECK_TIME   1000
#define UPDATE_SCRIPTS_TIME     10000

extern DebugVars    dbgVars;

extern volatile uint8_t slotSendToDevice[SLOT_COUNT];

int fddThreadCode(void)
{
    CoreFdd *coreFdd;
    pthread_t floppyEncThreadInfo;

    logFdd(LOG_INFO, "CosmosEx FDD core starting at port %d", SERVER_TCP_PORT_FDD);
    printf("\nCosmosEx FDD core starting at port %d\n", SERVER_TCP_PORT_FDD);
    printf("\nlog file: %s\n", Debug::getCoreLogFileName(false, LOGFILE_FDD));

    Utils::setTimezoneVariable_inThisContext();

    //-------------
    coreFdd = new CoreFdd();

    handlePthreadCreate("floppy encoder", &floppyEncThreadInfo, (void*) floppyEncodeThreadCode);

    coreFdd->run();                // run the fdd code
    delete coreFdd;

    floppyEncoder_stop();
    pthread_kill_join("floppy encoder", floppyEncThreadInfo);

    logFdd(LOG_INFO, "CosmosEx terminated.");
    printf("Terminated\n");
    return 0;
}

CoreFdd::CoreFdd()
{

}

CoreFdd::~CoreFdd()
{

}

void CoreFdd::run(void)
{
    loadSettings();

    // create network chip interface
    ciFdd = new ChipInterface(LOGFILE_FDD, NET_ATN_FRANZ_ID, SYNC_TAG_FDD);
    ciFdd->ciOpen();

    imageSilo = new ImageSilo(ciFdd);

    int max_fd;
    fd_set readfds;

    while(sigintReceived == 0) {
        Utils::sleepMs(1);      // intentional sleep to not utilize cpu to max when looping too much before client disconnect

        // handle floppy actions from command socket
        switch(events.fddAction) {
            case FDD_ACTION_INSERT: imageSilo->add(events.fddIndex, events.fddFlename, events.fdddHostPath); break;
            case FDD_ACTION_EJECT:  imageSilo->remove(events.fddIndex); break;
        }
        events.fddAction = FDD_ACTION_NONE;

        ciFdd->clientsDisconnectInactive();

        // check if any slot was just loaded and needs to be sent to device
        for(int i=0; i<SLOT_COUNT; i++) {
            if(slotSendToDevice[i]) {                   // if slot #i should be sent to device
                slotSendToDevice[i] = false;

                ClientInfo* client = ciFdd->clientsGetOne(i);

                if(client->fdClient == FD_EMPTY) {       // client not connected? skip
                    continue;
                }

                logFdd(LOG_DEBUG, "CoreFdd::run - sending whole image to client %d", i);
                handleSendImageToClient(client);
            }
        }

        max_fd = -1;
        FD_ZERO(&readfds);

        // get listening socket from chip interface, add it to readfds
        int fdListen = ciFdd->getFdListen();
        FD_SET(fdListen, &readfds);
        max_fd = MAX(max_fd, fdListen);

        // get all connected client fds, add them to readfds
        max_fd = MAX(max_fd, ciFdd->setAllClientFds(&readfds));     // all valid client fds will be set to readfds, and highest fd into max_fd

        // add timeout to select(), so we can check for connection status, settings reload, etc.
        timeval timeout;
        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;

        if(select(max_fd + 1, &readfds, NULL, NULL, &timeout) < 0) {
            if(errno == EINTR) {
                continue;   // a signal was delivered
            } else {
                logFdd(LOG_ERROR, "CoreFdd::run() select: %s", strerror(errno));
                continue;
            }
        }

        // if listening socket is set, handle it
        if(FD_ISSET(fdListen, &readfds)) {
            ciFdd->acceptSocketIfNeededAndPossible();
        }

        // check which fds are ready to be handled and handle them
        for(int i=0; i<MAX_CLIENTS; i++) {
            ClientInfo* ci = ciFdd->clientGetByIndex(i);

            if(ci->fdClient == FD_EMPTY) {           // no client here? skip it
                continue;
            }

            if(FD_ISSET(ci->fdClient, &readfds)) {    // this fd read for read?
                if(handleOneClient(i, ci->fdClient, ci->floppySlotIndex)) {
                    ci->lastMs = Utils::getCurrentMs();
                }
            }
        }
    }

    delete imageSilo;
    imageSilo = NULL;
}

bool CoreFdd::handleOneClient(int clientIndex, int fdClient, int floppySlotIndex)
{
    uint8_t inBuff[FDD_BUF_SIZE];
    memset(inBuff, 0, FDD_BUF_SIZE);

    bool needsAction = ciFdd->actionNeeded(clientIndex, inBuff);

    if(needsAction) {   // floppy drive needs action?
        handleFdd(clientIndex, fdClient, floppySlotIndex, inBuff);
    }

    return needsAction;
}

bool CoreFdd::handleFdd(int clientIndex, int fdClient, int floppySlotIndex, uint8_t* inBuff)
{
    bool isFddCommand = false;

    switch(inBuff[3]) {
    case ATN_FW_VERSION:                    // device has sent FW version
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
        handleSendImageToIndex(clientIndex);
        break;

    default:
        logFdd(LOG_ERROR, "CoreFdd received weird ATN code %02x waitForATN()", inBuff[3]);
        break;
    }

    return isFddCommand;
}

void CoreFdd::reloadSettings(int type)
{
    // then load the new settings
    loadSettings();
}

void CoreFdd::loadSettings(void)
{
    logFdd(LOG_DEBUG, "CoreFdd::loadSettings");
}

void CoreFdd::loadLastImageIntoSlot(int clientIndex)
{
    ClientInfo* client = ciFdd->clientsGetOne(clientIndex);
    if(!client) {
        return;
    }

    Settings s;
    s.setPrefix(client->mac, 6);                        // mac as prefix to settings
    const char *pPathAndFile = s.getString("FLOPPY_IMAGE", "");  // try to read the value
    std::string pathAndFile = pPathAndFile;

    if(!pathAndFile.empty()) {      // this specific device has file? store it also to slot #
        logFdd(LOG_DEBUG, "CoreFdd::loadLastImageIntoSlot - have stored image for this specific device, saving to slot %d -> %s", client->floppySlotIndex, pathAndFile.c_str());
        imageSilo->saveImageFilepathToSlot(client->floppySlotIndex, pathAndFile.c_str());
    } else {                        // no image file for this specific device? try to get it from the slot #
        pathAndFile = imageSilo->getImageFilePathFromSlotNo(client->floppySlotIndex);

        if(!pathAndFile.empty()) {  // slot # had an image stored? store it for this specific device
            logFdd(LOG_DEBUG, "CoreFdd::loadLastImageIntoSlot - no stored image for this specific device, but slot %d had image, so storing for specific device -> %s", client->floppySlotIndex, pathAndFile.c_str());
            s.setString("FLOPPY_IMAGE", pathAndFile.c_str());
        } else {
            logFdd(LOG_DEBUG, "CoreFdd::loadLastImageIntoSlot - no stored image for this specific device and no stored image for slot %d, so no image to load", client->floppySlotIndex);
        }
    }

    imageSilo->loadImageToSlot(client->floppySlotIndex, pathAndFile.c_str());
}

void CoreFdd::handleFwVersion_franz(int clientIndex)
{
    // ciFdd->setFDDconfig(setFloppyConfig, &floppyConfig, setDiskChanged, diskChanged);
    bool macChanged = ciFdd->getFWversionFdd(clientIndex);

    if(macChanged) {
        loadLastImageIntoSlot(clientIndex);
    }
}

void CoreFdd::handleSendTrack(int clientIndex)
{
    #define BFR_SIZE    32
    uint8_t inBuf[BFR_SIZE];
    int readCnt = ciFdd->readRestOfData(clientIndex, inBuf, BFR_SIZE);

    if(readCnt < 2) {
        logFdd(LOG_ERROR, "handleSendTrack() -- not enough data received: %d", readCnt);
        return;
    }

    int side = inBuf[0];                      // now read the current floppy position
    int track = inBuf[1];

    ClientInfo* client = ciFdd->clientsGetOne(clientIndex);

    int tr, si, spt;
    imageSilo->getParams(client->floppySlotIndex, tr, si, spt);      // read the floppy image params

    uint8_t *encodedTrack;
    int countInTrack;

    if(side < 0 || side > 1 || track < 0 || track >= tr) {      // side / track out of range? use empty track
        logFdd(LOG_ERROR, "handleSendTrack() -- side / track out of range, returning empty track. Franz wants: [track %d, side %d], but current image has %d tracks, %d sides, %d sectors/track", track, side, tr, si, spt);

        encodedTrack = imageSilo->getEmptyTrack();
        countInTrack = MFM_STREAM_SIZE;
    } else {                                                    // side + track within range? use encoded track
        logFdd(LOG_DEBUG, "handleSendTrack() -- Franz wants: [track %d, side %d]", track, side);

        encodedTrack = imageSilo->getEncodedTrack(client->floppySlotIndex, track, side, countInTrack);
        countInTrack = MIN(countInTrack, MFM_STREAM_SIZE);
    }

    ciFdd->fdd_sendTrackToChip(client->fdClient, countInTrack, encodedTrack);
}

void CoreFdd::handleSendImageToIndex(int clientIndex)
{
    #define BFR_SIZE    32
    uint8_t inBuf[BFR_SIZE];

    ciFdd->readRestOfData(clientIndex, inBuf, BFR_SIZE);

    ClientInfo* client = ciFdd->clientsGetOne(clientIndex);
    handleSendImageToClient(client);
}

void CoreFdd::handleSendImageToClient(ClientInfo* client)
{
    std::string fileName = imageSilo->getFileName(client->floppySlotIndex);   // get the filename

    int imgTracks, imgSides, imgSectorsPerTrack;
    imageSilo->getParams(client->floppySlotIndex, imgTracks, imgSides, imgSectorsPerTrack);      // read the floppy image params

    uint8_t *encodedTrack;
    int countInTrack;

    // sending started, send imagTracks, imgSides, spt
    ciFdd->fdd_sendImageParamsToChip(client->fdClient, false, imgTracks, imgSides, imgSectorsPerTrack, fileName);

    // send all the tracks from all the sides
    for(int side=0; side<imgSides; side++) {
        for(int track=0; track<imgTracks; track++) {
            // logFdd(LOG_DEBUG, "handleSendImage -- sending track %d, side %d, countInTrack: %d", track, side, countInTrack);
            encodedTrack = imageSilo->getEncodedTrack(client->floppySlotIndex, track, side, countInTrack);
            countInTrack = MIN(countInTrack, MFM_STREAM_SIZE);
            ciFdd->fdd_sendTrackToChip(client->fdClient, countInTrack, encodedTrack);
        }
    }

    // sending finished, send imagTracks, imgSides, spt, this will also do the disk change
    ciFdd->fdd_sendImageParamsToChip(client->fdClient, true, imgTracks, imgSides, imgSectorsPerTrack, fileName);
}

void CoreFdd::handleSectorWritten(int clientIndex)
{
    int side, track, sector, byteCount;
    uint8_t *writtenSector = ciFdd->fdd_sectorWritten(clientIndex, side, track, sector, byteCount); // get side + track + sector number, byte count, and pointer to buffer where the written data is

    // Debug::outBfr(LOGFILE_FDD, writtenSector, byteCount);

    ClientInfo* client = ciFdd->clientsGetOne(clientIndex);

    logFdd(LOG_DEBUG, "handleSectorWritten -- track %d, side %d, sector %d", track, side, sector);
    floppyEncoder_decodeMfmWrittenSector(client->floppySlotIndex, track, side, sector, writtenSector, byteCount); // let floppy encoder handle decoding, reencoding, saving
}
