#ifndef CCORETHREAD_H
#define CCORETHREAD_H

#include "utils.h"
#include "global.h"
#include "conspi.h"
#include "acsidatatrans.h"
#include "settings.h"
#include "retrymodule.h"

#include "native/translatedbootmedia.h"
#include "native/scsi.h"
#include "translated/translateddisk.h"

#include "settingsreloadproxy.h"
#include "isettingsuser.h"

#include "config/configstream.h"

#include "floppy/floppyimagefactory.h"
#include "floppy/mfmdecoder.h"
#include "floppy/mfmcachedimage.h"
#include "floppy/floppysetup.h"

#include "service/configservice.h"
#include "service/floppyservice.h"
#include "service/screencastservice.h"

#include "network/netadapter.h"

#include "version.h"

class CCoreThread: public ISettingsUser
{
public:
    CCoreThread(ConfigService* configService, FloppyService* floppyService, ScreencastService* screencastService);
    virtual ~CCoreThread();

	void resetHansAndFranz(void);
    void run(void);

    void sendHalfWord(void);
    virtual void reloadSettings(int type);    								// from ISettingsUser

    void setFloppyImageLed(int ledNo);

private:
    bool shouldRun;
    bool running;

    //-----------------------------------
    // data connection to STM32 slaves over SPI
    CConSpi         *conSpi;
    AcsiDataTrans   *dataTrans;

    //-----------------------------------
    // settings and config stuff
    SettingsReloadProxy     settingsReloadProxy;

    void loadSettings(void);

    //-----------------------------------
    // hard disk stuff
    bool            setEnabledIDbits;
	AcsiIDinfo		acsiIdInfo;
    RetryModule     *retryMod;
    
    void handleAcsiCommand(void);
    void handleConfigStream(BYTE *cmd);

    //-----------------------------------
    // handle FW version
    void handleFwVersion_hans(void);
    void handleFwVersion_franz(void);
    int bcdToInt(int bcd);

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
    ImageSilo           floppyImageSilo;
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

};

#endif // CCORETHREAD_H
