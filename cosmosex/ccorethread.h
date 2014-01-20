#ifndef CCORETHREAD_H
#define CCORETHREAD_H

#include "global.h"
#include "conspi.h"
#include "utils.h"
#include "devfinder.h"

#include "native/scsi.h"
#include "native/datamedia.h"
#include "native/testmedia.h"
#include "acsidatatrans.h"

#include "config/configstream.h"
#include "translated/translateddisk.h"

#include "settingsreloadproxy.h"

#include "isettingsuser.h"

class CCoreThread: public ISettingsUser
{
public:
    CCoreThread();
    ~CCoreThread();

    void run(void);
    void sendHalfWord(void);
    virtual void reloadSettings(void);      // from ISettingsUser

private:
    bool shouldRun;
    bool running;

    bool sendSingleHalfWord;

    CConSpi         *conSpi;

    Scsi            *scsi;
    AcsiDataTrans   *dataTrans;

    TranslatedDisk  *translated;
    ConfigStream    *confStream;

    TestMedia       testMedia;

    DevFinder		devFinder;

    SettingsReloadProxy     settingsReloadProxy;

    BYTE            acsiIDdevType[8];
    BYTE            enabledIDbits;
    bool            setEnabledIDbits;

    void createConnectionObject(void);
    void usbConnectionCheck(void);

    void loadSettings(void);

    void handleFwVersion(void);
    void handleAcsiCommand(void);
    void handleConfigStream(BYTE *cmd);

    int bcdToInt(int bcd);

    void logToFile(char *str);
    void logToFile(int len, BYTE *bfr);
    void logToFile(WORD wval);
};

#endif // CCORETHREAD_H
