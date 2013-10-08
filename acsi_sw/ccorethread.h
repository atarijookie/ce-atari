#ifndef CCORETHREAD_H
#define CCORETHREAD_H

#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QTimer>
#include <QStringList>

#include "global.h"
#include "cconusb.h"

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

    CConUsb *conUsb;

    void createConnectionObject(void);
    void usbConnectionCheck(void);

    void handleFwVersion(void);

    void sendAndReceive(int cnt, BYTE *outBuf, BYTE *inBuf, bool storeInData=true);
    void justReceive(int cnt, BYTE *inBuf);
    int bcdToInt(int bcd);

    void logToFile(char *str);
    void logToFile(int len, BYTE *bfr);
    void logToFile(WORD wval);

    struct {
        bool got;
        BYTE bytes[2];
    } prevAtnWord;

    void getAtnWord(BYTE *bfr);
    void setAtnWord(BYTE *bfr);
};

#endif // CCORETHREAD_H
