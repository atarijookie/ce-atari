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

class CCoreThread: public QThread
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

public slots:


private:
    bool shouldRun;
    bool running;

    bool sendSingleHalfWord;

    CConUsb         *conUsb;

    Scsi            *scsi;
    AcsiDataTrans   *dataTrans;
    DataMedia       *dataMedia;

    TestMedia       testMedia;

    void createConnectionObject(void);
    void usbConnectionCheck(void);

    void handleFwVersion(void);
    void handleAcsiCommand(void);
    void handleConfigStream(BYTE *cmd);

    int bcdToInt(int bcd);

    void logToFile(char *str);
    void logToFile(int len, BYTE *bfr);
    void logToFile(WORD wval);
};

#endif // CCORETHREAD_H
