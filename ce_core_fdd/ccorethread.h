#ifndef CCORETHREAD_H
#define CCORETHREAD_H

#include "global.h"
#include "settings.h"

#include "settingsreloadproxy.h"
#include "isettingsuser.h"

#include "version.h"
#include "utils.h"

class ConfigService;
class FloppyService;
class ScreencastService;
class RetryModule;
class ExtensionHandler;

#define INBUF_SIZE  16384

class CCoreThread: public ISettingsUser
{
public:
    CCoreThread();
    virtual ~CCoreThread();

    void run(void);

    virtual void reloadSettings(int type);                                  // from ISettingsUser

    void setFloppyImageLed(int ledNo);
    bool handleOneClient(int fdClient, int floppySlotindex);

private:
    bool shouldRun;
    bool running;

    ExtensionHandler *extensionHandler;

    //-----------------------------------
    // settings and config stuff
    SettingsReloadProxy     settingsReloadProxy;

    void loadSettings(void);

    //-----------------------------------
    // floppy stuff
    bool                setEnabledFloppyImgs;
    int                 lastFloppyImageLed;

    bool                setFloppyConfig;
    FloppyConfig        floppyConfig;

    bool                setDiskChanged;
    bool                diskChanged;

    bool                setNewFloppyImageLed;
    int                 newFloppyImageLed;
    int                 newFloppyImageLedAfterEncode;

    bool handleFdd(int fdClient, uint8_t* inBuff);
    void handleFwVersion_franz(int fdClient);
    void handleSendTrack(int fdClient, uint8_t *inBuf);
    void handleSectorWritten(int fdClient);

    //----------------------------------
    // recovery stuff
    void insertSpecialFloppyImage(int specialImageId);

    //----------------------------------
    // other
    void sharedObjects_create(void);
    void sharedObjects_destroy(void);

    void displayStatusToConsole(uint32_t now);
};

class LoadTracker {
public:
    struct {
        uint32_t start;
        uint32_t total;
    } cycle;

    struct {
        void markStart(void) {                          // call on start of block where the work is done (exclude idle sleep())
            start  = Utils::getCurrentMs();
        }

        void markEnd(void) {                            // call on end of block where the work is done (exclude idle sleep())
            total += Utils::getCurrentMs() - start;
        }

        uint32_t start;
        uint32_t total;
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
        loadPercents = 0;
        suspicious  = false;

        cycle.total = 0;
        cycle.start = Utils::getCurrentMs();
        busy.total  = 0;
    }
};

#endif // CCORETHREAD_H
