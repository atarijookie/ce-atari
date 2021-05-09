#ifndef CCORETHREAD_H
#define CCORETHREAD_H

#include "global.h"
#include "settings.h"

#include "settingsreloadproxy.h"
#include "isettingsuser.h"

#include "config/configstream.h"
#include "floppy/floppysetup.h"
#include "network/netadapter.h"
#include "misc/misc.h"

#include "version.h"

class ConfigService;
class FloppyService;
class ScreencastService;
class AcsiDataTrans;
class RetryModule;

class CCoreThread: public ISettingsUser
{
public:
    CCoreThread(ConfigService* configService, FloppyService* floppyService, ScreencastService* screencastService);
    virtual ~CCoreThread();

    void resetHansAndFranz(void);
    void run(void);

    void sendHalfWord(void);
    virtual void reloadSettings(int type);                                  // from ISettingsUser

    void setFloppyImageLed(int ledNo);

private:
    bool shouldRun;
    bool running;

    AcsiDataTrans   *dataTrans;

    //-----------------------------------
    // settings and config stuff
    SettingsReloadProxy     settingsReloadProxy;

    void loadSettings(void);

    //-----------------------------------
    // hard disk stuff
    bool            setEnabledIDbits;
    AcsiIDinfo      acsiIdInfo;
    RetryModule     *retryMod;

    void handleAcsiCommand(BYTE *bufIn);
    void handleConfigStream(BYTE *cmd);

    //-----------------------------------
    // handle FW version
    void handleFwVersion_hans(void);
    void handleFwVersion_franz(void);

    void saveHwConfig(void);
    void getIdBits(BYTE &enabledIDbits, BYTE &sdCardAcsiId);

    //-----------------------------------
    // floppy stuff
    FloppySetup         floppySetup;
    bool                setEnabledFloppyImgs;
    int                 lastFloppyImageLed;

    bool                setFloppyConfig;
    FloppyConfig        floppyConfig;

    bool                setDiskChanged;
    bool                diskChanged;

    bool                setNewFloppyImageLed;
    int                 newFloppyImageLed;
    int                 newFloppyImageLedAfterEncode;

    void handleSendTrack(BYTE *inBuf);
    void handleSectorWritten(void);

    //----------------------------------
    // network adapter stuff

    NetAdapter          netAdapter;

    // host module - misc
    Misc                misc;
    //----------------------------------
    // recovery stuff
    void handleRecoveryCommands(int recoveryLevel);
    void deleteSettingAndSetNetworkToDhcp(void);
    void insertSpecialFloppyImage(int specialImageId);

    //----------------------------------
    // other
    void showHwVersion(void);

    void sharedObjects_create(ConfigService* configService, FloppyService *floppyService, ScreencastService* screencastService);
    void sharedObjects_destroy(void);

    void fillDisplayLines(void);
};

class LoadTracker {
public:
    struct {
        DWORD start;
        DWORD total;
    } cycle;

    struct {
        void markStart(void) {                          // call on start of block where the work is done (exclude idle sleep())
            start  = Utils::getCurrentMs();
        }

        void markEnd(void) {                            // call on end of block where the work is done (exclude idle sleep())
            total += Utils::getCurrentMs() - start;
        }

        DWORD start;
        DWORD total;
    } busy;

    int     loadPercents;                               // contains 0 .. 100, meaning percentage of load
    bool    suspicious;                                 // if the last load percentage was high or cycle time was long, this will be true

    LoadTracker(void) {
        clear();
    }

    void calculate(void) {  // call this on the end of 1 second interval to calculate load
        cycle.total     = Utils::getCurrentMs() - cycle.start;
        loadPercents    = (busy.total * 100) / cycle.total;

        suspicious      = false;

        if(cycle.total > 1100 || loadPercents > 90) {
            suspicious  = true;
        }
    }

    void clear(void) {      // call this on the start of new 1 second interval to clear everything
        loadPercents= 0;
        suspicious  = false;

        cycle.total = 0;
        cycle.start = Utils::getCurrentMs();
        busy.total  = 0;
    }
};

#endif // CCORETHREAD_H
