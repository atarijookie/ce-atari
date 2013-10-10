#include <QDir>
#include <QDebug>

#include "global.h"
#include "ccorethread.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

BYTE        circBfr[20480];             // 0x5000 bytes
int         cb_cnt, cb_posa, cb_posg;

#define		bfr_add(X)				{ circBfr[cb_posa] = X;     cb_posa++;      cb_posa &= 0x4FFF;   cb_cnt++; }
#define		bfr_get(X)				{ X = circBfr[cb_posg];     cb_posg++;      cb_posg &= 0x4FFF;   cb_cnt--; }

BYTE        cmdBuffer[16];
int         cd_cnt, cd_posa, cd_posg;
#define		cmd_add(X)				{ cmdBuffer[cd_posa] = X;     cd_posa++;      cd_posa &= 0x4FFF;   cd_cnt++; }
#define		cmd_get(X)				{ X = cmdBuffer[cd_posg];     cd_posg++;      cd_posg &= 0x4FFF;   cd_cnt--; }

#define LOGFILE "C:/acsilog.txt"

QStringList dbg;

BYTE inBuff[1200];
BYTE outBuff[1200];

extern "C" void outDebugString(const char *format, ...);

CCoreThread::CCoreThread()
{
    shouldRun       = true;
    running         = false;

    sendSingleHalfWord = false;

    conUsb      = NULL;
    createConnectionObject();
    conUsb->tryToConnect();

    dataTrans   = new AcsiDataTrans();

    dataMedia   = new DataMedia();
    dataMedia->open((char *) "C:\\datamedia.img", true);

    scsi        = new Scsi();
    scsi->setAcsiDataTrans(dataTrans);
    scsi->setDataMedia(dataMedia);
}

CCoreThread::~CCoreThread()
{
    if(isRunning()) {
        stopRunning();
    }

    CCoreThread::displayDbg();

    delete conUsb;
    delete dataTrans;
    delete dataMedia;
    delete scsi;
}

void CCoreThread::displayDbg(void)
{
    for(int i=0; i<dbg.count(); i++) {
        qDebug() << dbg.at(i);
    }
}

void CCoreThread::run(void)
{
    DWORD lastTick = GetTickCount();
    running = true;

    outDebugString("Core thread starting...");

    while(shouldRun) {
        if(sendSingleHalfWord) {
            BYTE halfWord = 0, inHalf;
            conUsb->txRx(1, &halfWord, &inHalf);
            sendSingleHalfWord = false;
        }

        getAtnWord(inBuff);

        if(inBuff[0] != 0 || inBuff[1] != 0) {
            logToFile((char *) "Waiting for ATN: \nOUT:\n");
            logToFile(2, outBuff);
            logToFile((char *) "\nIN:\n");
            logToFile(2, inBuff);
            logToFile((char *) "\n");
        }

        switch(inBuff[1]) {
        case 0:                 // this is valid, just empty data, skip this
            break;

        case ATN_FW_VERSION:
            handleFwVersion();
            break;

        default:
            logToFile((char *) "That ^^^ shouldn't happen!\n");
            break;
        }

        if(GetTickCount() - lastTick < 10) {            // less than 10 ms ago?
            msleep(1);
            continue;
        }
        lastTick = GetTickCount();
    }

    running = false;
}

void CCoreThread::getAtnWord(BYTE *bfr)
{
    if(prevAtnWord.got) {                   // got some previous ATN word? use it
        bfr[0] = prevAtnWord.bytes[0];
        bfr[1] = prevAtnWord.bytes[1];
        prevAtnWord.got = false;

        return;
    }

    // no previous ATN word? read it!
    BYTE outBuff[2];
    memset(outBuff, 0, 2);
    conUsb->txRx(2, outBuff, bfr);
}

void CCoreThread::setAtnWord(BYTE *bfr)
{
    prevAtnWord.bytes[0] = bfr[0];
    prevAtnWord.bytes[1] = bfr[1];
    prevAtnWord.got = true;
}

void CCoreThread::setNextCmd(BYTE cmd)
{
    cmd_add(cmd);
}

void CCoreThread::sendAndReceive(int cnt, BYTE *outBuf, BYTE *inBuf, bool storeInData)
{
    if(!conUsb->isConnected()) {                    // not connected? quit
        return;
    }

    conUsb->txRx(cnt, outBuf, inBuf);               // send and receive

//    QString so,si;
//    int val;

//    for(int i=0; i<cnt; i++) {
//        val = (int) outBuf[i];
//        so = so + QString("%1 ").arg(val, 2, 16, QLatin1Char('0'));

//        val = (int) inBuf[i];
//        si = si + QString("%1 ").arg(val, 2, 16, QLatin1Char('0'));
//    }
//    qDebug() << "Sent: " << so;
//    qDebug() << "Got : " << si;

    if(!storeInData) {
        return;
    }

    for(int i=0; i<cnt; i++) {                      // add to circular buffer
        bfr_add(inBuf[i]);
    }
}

void CCoreThread::justReceive(int cnt, BYTE *inBuf)
{
    if(!conUsb->isConnected()) {                    // not connected? quit
        return;
    }

    conUsb->read(cnt, inBuf);                       // receive

    for(int i=0; i<cnt; i++) {                      // add to circular buffer
        bfr_add(inBuf[i]);
    }
}

void CCoreThread::handleFwVersion(void)
{
    BYTE fwVer[10-2], oBuf[10-2];
    int storeCnt;

    memset(oBuf, 0, 10-2);
    storeCnt = MIN(cd_cnt, 10-2);             // get the minimum from sizeof(buf), remaining, cd_cnt (what we may send, what we should send, what we got)

    for(int i=0; i<storeCnt; i++) {         // now copy all the possible bytes (commands) to buffer
        cmd_get(oBuf[i]);
    }

    conUsb->txRx(10-2, oBuf, fwVer);

    logToFile((char *) "handleFwVersion: \nOUT:\n");
    logToFile(10-2, oBuf);
    logToFile((char *) "\nIN:\n");
    logToFile(10-2, fwVer);
    logToFile((char *) "\n");

    int year = bcdToInt(fwVer[1]) + 2000;
    if(fwVer[0] == 0xf0) {
        outDebugString("FW: Franz, %d-%02d-%02d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));
    } else {
        outDebugString("FW: Hans,  %d-%02d-%02d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));
    }

    setAtnWord(&fwVer[8]);          // add the last WORD as possible to check for the new ATN
}

int CCoreThread::bcdToInt(int bcd)
{
    int a,b;

    a = bcd >> 4;       // upper nibble
    b = bcd &  0x0f;    // lower nibble

    return ((a * 10) + b);
}

void CCoreThread::sendHalfWord(void)
{
    sendSingleHalfWord = true;
}

void CCoreThread::stopRunning(void)
{
    shouldRun = false;
}

bool CCoreThread::isRunning(void)
{
    return running;
}

void CCoreThread::createConnectionObject(void)
{
    if(!conUsb) {                       // create object
        conUsb = new CConUsb();
    }

    if(!conUsb->init()) {               // load dll, if failed, delete object
        outDebugString("Failed to open USB connection - .dll not loaded.");
    } else {
        outDebugString("USB connection ready to connect to device.");
    }
}

void CCoreThread::usbConnectionCheck(void)
{
    if(!conUsb) {                           // no usb connection object?
        return;
    }

    if(!conUsb->isConnected()) {            // if not connected, try to connect
        conUsb->tryToConnect();
    } else {                                // if connected, check if still connected
        if(!conUsb->connectionWorking()) {  // connection not working?
            createConnectionObject();
        }
    }
}

void CCoreThread::appendToDbg(QString line)
{
    dbg.append(line);
}

void outDebugString(const char *format, ...)
{
    va_list args;
    va_start(args, format);

#define KJUT
#ifdef KJUT
    char tmp[1024];
    vsprintf(tmp, format, args);
    qDebug() << tmp;
//    CCoreThread::appendToDbg(QString(tmp));
#else
    vprintf(format, args);
#endif

    va_end(args);
}

void CCoreThread::logToFile(char *str)
{
    FILE *f = fopen(LOGFILE, "at");

    if(!f) {
        qDebug() << "dafuq!";
        return;
    }

    fprintf(f, str);
    fclose(f);
}

void CCoreThread::logToFile(WORD wval)
{
    FILE *f = fopen(LOGFILE, "at");

    if(!f) {
        qDebug() << "dafuq!";
        return;
    }

//    fprintf(f, "%04x\n", wval);
    fprintf(f, "%d\n", wval);
    fclose(f);
}

void CCoreThread::logToFile(int len, BYTE *bfr)
{
    FILE *f = fopen(LOGFILE, "at");

    if(!f) {
        qDebug() << "dafuq!";
        return;
    }

    fprintf(f, "buffer -- %d bytes\n", len);

    while(len > 0) {
        for(int col=0; col<16; col++) {
            if(len == 0) {
                break;
            }

            fprintf(f, "%02x ", *bfr);

            bfr++;
            len--;
        }
        fprintf(f, "\n");
    }

    fclose(f);
}
