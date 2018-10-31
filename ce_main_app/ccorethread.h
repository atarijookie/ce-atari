#ifndef CCORETHREAD_H
#define CCORETHREAD_H

#include "global.h"
#include "settings.h"
#include "datatrans.h"

#include "settingsreloadproxy.h"
#include "isettingsuser.h"

#include "config/configstream.h"
#include "floppy/floppysetup.h"
#include "network/netadapter.h"

#include "version.h"

class ConfigService;
class FloppyService;
class ScreencastService;
class DataTrans;

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

    //-----------------------------------
    // data transportation object
    DataTrans *dataTrans;

    //-----------------------------------
    // settings and config stuff
    SettingsReloadProxy     settingsReloadProxy;

    void loadSettings(void);

    //-----------------------------------
    // hard disk stuff
    bool            setEnabledIDbits;
    AcsiIDinfo      acsiIdInfo;

    void handleAcsiCommand(void);
    void handleConfigStream(BYTE *cmd);

    //-----------------------------------
    // handle FW version
    void handleFwVersion_hans(void);
    void handleFwVersion_franz(void);

    void responseStart(int bufferLengthInBytes);
    void responseAddWord(BYTE *bfr, WORD value);
    void responseAddByte(BYTE *bfr, BYTE value);

    struct {
        int bfrLengthInBytes;
        int currentLength;
    } response;

    void convertXilinxInfo(BYTE xilinxInfo);
    void saveHwConfig(void);
    void getIdBits(BYTE &enabledIDbits, BYTE &sdCardAcsiId);

    struct {
        struct {
            WORD acsi;
            WORD fdd;
        } current;

        struct {
            WORD acsi;
            WORD fdd;
        } next;

        bool skipNextSet;
    } hansConfigWords;

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

    void handleSendTrack(void);
    void handleSectorWritten(void);

    //----------------------------------
    // network adapter stuff

    NetAdapter          netAdapter;

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
