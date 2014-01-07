#ifndef CCORETHREAD_H
#define CCORETHREAD_H

#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QTimer>
#include <QStringList>

#include "global.h"
#include "cconusb.h"

#include "native/scsi.h"
#include "native/datamedia.h"
#include "native/testmedia.h"
#include "acsidatatrans.h"

#include "config/configstream.h"
#include "translated/translateddisk.h"

#include "settingsreloadproxy.h"

#include "ISettingsUser.h"

class CCoreThread: public QThread, public ISettingsUser
{
    Q_OBJECT
public:
    CCoreThread();
    ~CCoreThread();

    void run(void);
    void stopRunning(void);
    bool isRunning(void);

    static void appendToDbg(QString line);
    static void displayDbg(void);

    void sendHalfWord(void);

    virtual void reloadSettings(void);      // from ISettingsUser

public slots:


private:
    bool shouldRun;
    bool running;

    bool sendSingleHalfWord;

    CConUsb         *conUsb;

    Scsi            *scsi;
    AcsiDataTrans   *dataTrans;

    TranslatedDisk  *translated;
    ConfigStream    *confStream;

    TestMedia       testMedia;

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
