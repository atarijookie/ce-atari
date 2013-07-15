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
    IFloppyImage        *image;
    FloppyImageFactory  imageFactory;

    BYTE                nextCmd;

    WORD                CRC;

    void createConnectionObject(void);
    void usbConnectionCheck(void);

    void appendA1MarkToStream(BYTE *bfr, int &cnt);
    void appendTime(BYTE time, BYTE *bfr, int &cnt);
    void appendChange(BYTE chg, BYTE *bfr, int &cnt);
    void appendByteToStream(BYTE val, BYTE *bfr, int &cnt);
    bool createMfmStream(int side, int track, int sector, BYTE *buffer, int &count);

    void fdc_add_to_crc(WORD &crc, BYTE data);

    void handleFwVersion(void);
    void handleSendNextSector(int &side, int &track, int &sector, BYTE *oBuf, BYTE *iBuf);
    void handleSendTrack(int &side, int &track, BYTE *oBuf, BYTE *iBuf);
    void handleSectorWasWritten(void);

    void sendAndReceive(int cnt, BYTE *outBuf, BYTE *inBuf);
    int bcdToInt(int bcd);
};

#endif // CCORETHREAD_H
