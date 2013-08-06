#ifndef CCORETHREAD_H
#define CCORETHREAD_H

#include <QThread>
#include <QQueue>
#include <QMutex>
#include <QTimer>
#include <QStringList>

#include "global.h"
#include "cconusb.h"
#include "ifloppyimage.h"
#include "floppyimagefactory.h"
#include "mfmcachedimage.h"

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

    CConUsb             *conUsb;
    MfmCachedImage      encImage;
    IFloppyImage        *image;
    FloppyImageFactory  imageFactory;

    int                 lastSide, lastTrack;            // these are here to avoid sending the same track again

    void createConnectionObject(void);
    void usbConnectionCheck(void);

    void handleFwVersion(void);
    void handleSendNextSector(int &side, int &track, int &sector, BYTE *oBuf, BYTE *iBuf);
    void handleSendTrack(int &side, int &track);
    void handleSectorWasWritten(void);

    void sendAndReceive(int cnt, BYTE *outBuf, BYTE *inBuf, bool storeInData=true);
    void justReceive(int cnt, BYTE *inBuf);
    int bcdToInt(int bcd);

    void logToFile(char *str);
    void logToFile(int len, BYTE *bfr);


    struct {
        bool got;
        BYTE bytes[2];
    } prevAtnWord;

    void getAtnWord(BYTE *bfr);
    void setAtnWord(BYTE *bfr);
};

#endif // CCORETHREAD_H
