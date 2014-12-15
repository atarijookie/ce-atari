#ifndef CCORETHREAD_H
#define CCORETHREAD_H

#include <map>

#include "utils.h"
#include "global.h"
#include "conspi.h"
#include "acsidatatrans.h"
#include "settings.h"

#include "devfinder.h"
#include "devchangeshandler.h"

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

class CCoreThread: public ISettingsUser, public DevChangesHandler
{
public:
    CCoreThread(ConfigService* configService, FloppyService* floppyService, ScreencastService* screencastService);
    virtual ~CCoreThread();

	void resetHansAndFranz(void);
    void run(void);

    void sendHalfWord(void);
    virtual void reloadSettings(int type);    								// from ISettingsUser

	virtual void onDevAttached(std::string devName, bool isAtariDrive);		// from DevChangesHandler
	virtual void onDevDetached(std::string devName);						// from DevChangesHandler

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
    ConfigStream            *confStream;
    SettingsReloadProxy     settingsReloadProxy;

    void loadSettings(void);

    //-----------------------------------
    // hard disk stuff
    DevFinder		devFinder;
    Scsi            *scsi;
    TranslatedDisk  *translated;
    bool            setEnabledIDbits;
	AcsiIDinfo		acsiIdInfo;
    bool            mountRawNotTrans;

    void handleAcsiCommand(void);
    void handleConfigStream(BYTE *cmd);

    //-----------------------------------
    // mount and attach stuff
	std::multimap<std::string, std::string> mapDeviceToHostPaths;

	void attachDevAsTranslated(std::string devName);
    //-----------------------------------
    // handle FW version
    void handleFwVersion(int whichSpiCs);
    int bcdToInt(int bcd);

    void responseStart(int bufferLengthInBytes);
    void responseAddWord(BYTE *bfr, WORD value);
    void responseAddByte(BYTE *bfr, BYTE value);

    struct {
        int bfrLengthInBytes;
        int currentLength;
    } response;

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

    void handleSendTrack(void);
    void handleSectorWritten(void);

    //----------------------------------
    // network adapter stuff

    NetAdapter          netAdapter;

    //----------------------------------
    // other
    void readWebStartupMode(void);
	bool inetIfaceReady(const char* ifrname);
};

#endif // CCORETHREAD_H
