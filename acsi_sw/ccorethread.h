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
#include "acsidatatrans.h"

class CCoreThread: public QThread
{
    Q_OBJECT
public:
    CCoreThread();
    ~CCoreThread();

    void run(void);
    void stopRunning(void);
    bool isRunning(void);

    void setNextCmd(BYTE cmd);

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

    void createConnectionObject(void);
    void usbConnectionCheck(void);

    void handleFwVersion(void);
    void handleAcsiCommand(void);

    void sendAndReceive(int cnt, BYTE *outBuf, BYTE *inBuf, bool storeInData=true);
    void justReceive(int cnt, BYTE *inBuf);
    int bcdToInt(int bcd);

    void logToFile(char *str);
    void logToFile(int len, BYTE *bfr);
    void logToFile(WORD wval);
};

#endif // CCORETHREAD_H
