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
#include "ccorethread.h"
#include "update.h"
#include "utils.h"
#include "chipinterface_network/chipinterfacenetwork.h"

#include "floppy/imagesilo.h"
#include "floppy/imagestorage.h"
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
LoadTracker load;

CCoreThread::CCoreThread()
{
    Update::initialize();

    setEnabledFloppyImgs    = false;
    setNewFloppyImageLed    = false;
    setFloppyConfig         = false;
    setDiskChanged          = false;
    diskChanged             = false;

    newFloppyImageLedAfterEncode = -2;

    sharedObjects_create();

    // now register all the objects which use some settings in the proxy
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this, SETTINGSUSER_FLOPPYIMGS);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this, SETTINGSUSER_FLOPPYCONF);
    settingsReloadProxy.addSettingsUser((ISettingsUser *) this, SETTINGSUSER_FLOPPY_SLOT);

    // the floppy image silo might change settings (when images are changes), add settings reload proxy
    shared.imageSilo->setSettingsReloadProxy(&settingsReloadProxy);
    settingsReloadProxy.reloadSettings(SETTINGSUSER_FLOPPYIMGS);            // mark that floppy settings changed (when imageSilo loaded the settings)
}

CCoreThread::~CCoreThread()
{
    sharedObjects_destroy();
}

void CCoreThread::sharedObjects_create(void)
{
    shared.imageSilo = new ImageSilo();
    shared.imageStorage = new ImageStorage();
}

void CCoreThread::sharedObjects_destroy(void)
{
    delete shared.imageSilo;
    shared.imageSilo = NULL;

    delete shared.imageStorage;
    shared.imageStorage = NULL;
}

void CCoreThread::run(void)
{
    loadSettings();

    load.clear();                               // clear load counter

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
                Debug::out(LOG_ERROR, "CCoreThread::run() select: %s", strerror(errno));
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

bool CCoreThread::handleOneClient(int fdClient, int floppySlotindex)
{
    uint8_t inBuff[INBUF_SIZE];
    memset(inBuff, 0, INBUF_SIZE);

    bool needsAction = chipInterface->actionNeeded(fdClient, inBuff);

    if(needsAction) {   // floppy drive needs action?
        handleFdd(fdClient, inBuff);
    }

    return needsAction;
}

bool CCoreThread::handleFdd(int fdClient, uint8_t* inBuff)
{
    bool isFddCommand = false;

    switch(inBuff[3]) {
    case ATN_FW_VERSION:                    // device has sent FW version
        lastFwInfoTime.franz = Utils::getCurrentMs();
        handleFwVersion_franz(fdClient);
        break;

    case ATN_SECTOR_WRITTEN:                // device has sent written sector data
        isFddCommand = true;
        handleSectorWritten(fdClient);
        break;

    case ATN_SEND_TRACK:                    // device requests data of a whole track
        isFddCommand = true;
        handleSendTrack(fdClient, inBuff + 8);
        break;

    default:
        Debug::out(LOG_ERROR, "CCoreThread received weird ATN code %02x waitForATN()", inBuff[3]);
        break;
    }

    return isFddCommand;
}

void CCoreThread::displayStatusToConsole(uint32_t now)
{
    char progChars[4] = {'|', '/', '-', '\\'};

    //-------------
    // calculate load, show message if needed
    load.calculate();                                           // calculate load

    if(load.suspicious) {                                       // load is suspiciously high?
        Debug::out(LOG_DEBUG, ">>> Suspicious core cycle load -- load: %3d %%", load.loadPercents);
        printf(">>> Suspicious core cycle load -- load: %3d %%\n", load.loadPercents);
    }
    //-------------

    printf("\033[2K  [ %c ]  CE core is running\033[A\n", progChars[lastFwInfoTime.progress]);
    lastFwInfoTime.progress = (lastFwInfoTime.progress + 1) % 4;

    load.clear();                       // clear load counter
}

void CCoreThread::reloadSettings(int type)
{
    // if just floppy image configuration changed, set that we should send new floppy images config and quit
    if(type == SETTINGSUSER_FLOPPYIMGS) {
        setEnabledFloppyImgs = true;
        return;
    }

    if(type == SETTINGSUSER_FLOPPYCONF) {
        Settings s;
        s.loadFloppyConfig(&floppyConfig);

        setFloppyConfig = true;
        return;
    }

    if(type == SETTINGSUSER_FLOPPY_SLOT) {
        setFloppyImageLed(shared.imageSilo->getCurrentSlot());
        return;
    }

    // then load the new settings
    loadSettings();
}

void CCoreThread::loadSettings(void)
{
    Debug::out(LOG_DEBUG, "CCoreThread::loadSettings");

    Settings s;
    s.loadFloppyConfig(&floppyConfig);
    setFloppyConfig     = true;
}

void CCoreThread::handleFwVersion_franz(int fdClient)
{
    uint8_t fwVer[14];
    memset(fwVer,  0, 14);

    chipInterface->setFDDconfig(setFloppyConfig, &floppyConfig, setDiskChanged, diskChanged);
    chipInterface->getFWversion(fdClient, fwVer);

    if(setFloppyConfig) {                                       // did set floppy config? don't set again
        setFloppyConfig = false;
    }
    
    static bool franzHandledOnce = false;

    if(franzHandledOnce) {                                      // don't send the commands until we did receive at least one firmware message
        if(setDiskChanged) {
            if(diskChanged) {                                   // if the disk changed, change it to not-changed and let it send a command again in a second
                diskChanged = false;
            } else {                                            // if we're in the not-changed state, don't send it again
                setDiskChanged = false;
            }
        }
    }

    franzHandledOnce = true;
    Debug::out(LOG_DEBUG, "FW: Franz, %d-%02d-%02d", Update::versions.franz.getYear(), Update::versions.franz.getMonth(), Update::versions.franz.getDay());
}

void CCoreThread::setFloppyImageLed(int ledNo)
{
    if(ledNo >=0 && ledNo < 3) {                    // if the LED # is within expected range
        uint8_t enabledImgs = shared.imageSilo->getSlotBitmap();

        if(enabledImgs & (1 << ledNo)) {            // if the required LED # is enabled, set it
            Debug::out(LOG_DEBUG, "Setting new floppy image LED to %d", ledNo);
            newFloppyImageLed       = ledNo;
            setNewFloppyImageLed    = true;
        }
    }

    if(ledNo == 0xff) {                             // if this is a request to turn the LEDs off, do it
        Debug::out(LOG_DEBUG, "Setting new floppy image LED to 0xff (no LED)");
        newFloppyImageLed       = ledNo;
        setNewFloppyImageLed    = true;
    }
}

void CCoreThread::handleSendTrack(int fdClient, uint8_t *inBuf)
{
    static int prevTrack = 0;

    int side    = inBuf[0];                      // now read the current floppy position
    int track   = inBuf[1];

    int tr, si, spt;
    shared.imageSilo->getParams(tr, si, spt);      // read the floppy image params

    uint8_t *encodedTrack;
    int countInTrack;

    if(side < 0 || side > 1 || track < 0 || track >= tr) {      // side / track out of range? use empty track
        Debug::out(LOG_ERROR, "handleSendTrack() -- side / track out of range, returning empty track. Franz wants: [track %d, side %d], but current image has %d tracks, %d sides, %d sectors/track", track, side, tr, si, spt);

        encodedTrack = shared.imageSilo->getEmptyTrack();
    } else {                                                    // side + track within range? use encoded track
        Debug::out(LOG_DEBUG, "handleSendTrack() -- Franz wants: [track %d, side %d]", track, side);

        encodedTrack = shared.imageSilo->getEncodedTrack(track, side, countInTrack);
    }

    int remaining = MFM_STREAM_SIZE - (4*2) - 2;    // this much bytes remain to send after the received ATN
    chipInterface->fdd_sendTrackToChip(fdClient, remaining, encodedTrack);

    // now we should do some buzzing because of floppy seek
    if(prevTrack != track) {                        // track changed?
        prevTrack = track;
    }
}

void CCoreThread::handleSectorWritten(int fdClient)
{
    int side, track, sector, byteCount;
    uint8_t *writtenSector = chipInterface->fdd_sectorWritten(fdClient, side, track, sector, byteCount); // get side + track + sector number, byte count, and pointer to buffer where the written data is

    if(!floppyConfig.writeProtected) {  // not write protected? write
        Debug::out(LOG_DEBUG, "handleSectorWritten -- track %d, side %d, sector %d", track, side, sector);
        floppyEncoder_decodeMfmWrittenSector(track, side, sector, writtenSector, byteCount); // let floppy encoder handle decoding, reencoding, saving
    } else {                            // is write protected? don't write
        Debug::out(LOG_DEBUG, "handleSectorWritten -- floppy is write protected, not writing");
    }
}

void CCoreThread::insertSpecialFloppyImage(int specialImageId)
{
    std::string imgFilename;
    std::string imgSrcPath;
    std::string imgFullPath;
    std::string dummy;

    if(specialImageId == SPECIAL_FDD_IMAGE_CE_CONF) {           // CE CONF image
        imgFilename = CE_CONF_FDD_IMAGE_JUST_FILENAME;
        imgSrcPath  = CE_CONF_FDD_IMAGE_PATH_AND_FILENAME;      // image in /ce/app dir
        imgFullPath = CE_CONF_FDD_IMAGE_PATH_AND_FILENAME_TMP;  // image in /tmp dir

        Utils::copyFile(imgSrcPath, imgFullPath);               // copy from /ce/app to /tmp to allow writing, but don't preserve changes

        Debug::out(LOG_INFO, "Will insert special FDD image: CE_CONF image");
    } else if(specialImageId == SPECIAL_FDD_IMAGE_FDD_TEST) {   // FDD TEST image
        imgFilename = FDD_TEST_IMAGE_JUST_FILENAME;
        imgSrcPath  = FDD_TEST_IMAGE_PATH_AND_FILENAME;         // image in /ce/app dir
        imgFullPath = FDD_TEST_IMAGE_PATH_AND_FILENAME_TMP;     // image in /tmp dir

        Utils::copyFile(imgSrcPath, imgFullPath);               // copy from /ce/app to /tmp to allow writing, but don't preserve changes

        Debug::out(LOG_INFO, "Will insert special FDD image: FDD TEST image");
    } else {
        Debug::out(LOG_INFO, "Unknown special image: %d, doing nothing.", specialImageId);
        return;
    }

    // encode MSA config image to MFM stream - in slot #0
    shared.imageSilo->add(0, imgFilename, imgFullPath, dummy, false);

    // set the current to slot #0
    shared.imageSilo->setCurrentSlot(0);

    // when encoding stops, set this FDD image LED
    newFloppyImageLedAfterEncode = 0;
}
