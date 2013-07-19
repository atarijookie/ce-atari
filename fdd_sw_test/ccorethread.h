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

public slots:


private:
    bool shouldRun;
    bool running;

    CConUsb             *conUsb;
    MfmCachedImage      encImage;
    IFloppyImage        *image;
    FloppyImageFactory  imageFactory;

    int                 lastSide, lastTrack;            // these are here to avoid sending the same track again

    void createConnectionObject(void);
    void usbConnectionCheck(void);

    void handleFwVersion(void);
    void handleSendNextSector(int &side, int &track, int &sector, BYTE *oBuf, BYTE *iBuf);
    void handleSendTrack(int &side, int &track, BYTE *iBuf);
    void handleSectorWasWritten(void);

    void sendAndReceive(int cnt, BYTE *outBuf, BYTE *inBuf);
    int bcdToInt(int bcd);
};

#endif // CCORETHREAD_H
