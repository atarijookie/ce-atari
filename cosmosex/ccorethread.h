#ifndef CCORETHREAD_H
#define CCORETHREAD_H

#include <map>

#include "global.h"
#include "conspi.h"
#include "utils.h"
#include "devfinder.h"
#include "devchangeshandler.h"

#include "native/translatedbootmedia.h"
#include "native/scsi.h"
#include "acsidatatrans.h"

#include "config/configstream.h"
#include "translated/translateddisk.h"

#include "settingsreloadproxy.h"

#include "isettingsuser.h"

class CCoreThread: public ISettingsUser, public DevChangesHandler
{
public:
    CCoreThread();
    ~CCoreThread();

	void resetHansAndFranz(void);
    void run(void);
    void sendHalfWord(void);
    virtual void reloadSettings(void);      								// from ISettingsUser

	virtual void onDevAttached(std::string devName, bool isAtariDrive);		// from DevChangesHandler
	virtual void onDevDetached(std::string devName);						// from DevChangesHandler
	
private:
    bool shouldRun;
    bool running;

    bool sendSingleHalfWord;

	std::multimap<std::string, std::string> mapDeviceToHostPaths;
	
    CConSpi         *conSpi;

    Scsi            *scsi;
    AcsiDataTrans   *dataTrans;

    TranslatedDisk  *translated;
    ConfigStream    *confStream;

    DevFinder		devFinder;

    SettingsReloadProxy     settingsReloadProxy;

    BYTE            acsiIDevType[8];
    BYTE            enabledIDbits;
    bool            setEnabledIDbits;
	
	bool			gotDevTypeRaw;
	bool			gotDevTypeTranslated;

    void createConnectionObject(void);
    void usbConnectionCheck(void);

    void loadSettings(void);

    void handleFwVersion(void);
    void handleAcsiCommand(void);
    void handleConfigStream(BYTE *cmd);

    int bcdToInt(int bcd);
	
	void attachDevAsTranslated(std::string devName);
	void mountAndAttachSharedDrive(void);
};

#endif // CCORETHREAD_H
