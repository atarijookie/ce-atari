#include <QDir>
#include <QDebug>

#include "global.h"
#include "ccorethread.h"
#include "floppyimagefactory.h"

BYTE        circBfr[20480];             // 0x5000 bytes
int         cb_cnt, cb_posa, cb_posg;

#define		bfr_add(X)				{ circBfr[cb_posa] = X;     cb_posa++;      cb_posa &= 0x4FFF;   cb_cnt++; }
#define		bfr_get(X)				{ X = circBfr[cb_posg];     cb_posg++;      cb_posg &= 0x4FFF;   cb_cnt--; }

BYTE        cmdBuffer[16];
int         cd_cnt, cd_posa, cd_posg;
#define		cmd_add(X)				{ cmdBuffer[cd_posa] = X;     cd_posa++;      cd_posa &= 0x4FFF;   cd_cnt++; }
#define		cmd_get(X)				{ X = cmdBuffer[cd_posg];     cd_posg++;      cd_posg &= 0x4FFF;   cd_cnt--; }

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// todo:
// - set floppy parameters to device and send only the required amount of data to floppy

QStringList dbg;

CCoreThread::CCoreThread()
{
    shouldRun       = true;
    running         = false;

    conUsb      = NULL;
    createConnectionObject();
    conUsb->tryToConnect();

    lastSide = -1;
    lastTrack = -1;
}

CCoreThread::~CCoreThread()
{
    if(isRunning()) {
        stopRunning();
    }

    CCoreThread::displayDbg();

    delete conUsb;
}

void CCoreThread::displayDbg(void)
{
    for(int i=0; i<dbg.count(); i++) {
        qDebug() << dbg.at(i);
    }
}

void CCoreThread::run(void)
{
    running = true;
    DWORD lastTick = GetTickCount();

    BYTE outBuff[20480], inBuff[20480];
    int side=0, track=0;

    image = imageFactory.getImage((char *) "A_054.ST");

    if(!image) {
        outDebugString("Image file type not supported!");
        return;
    }

    if(!image->isOpen()) {
        outDebugString("Image is not open!");
        return;
    }

    outDebugString("Encoding image...");
    encImage.encodeAndCacheImage(image);
    outDebugString("...done");

//    for(int i=0; i<7500; i++) {
//        outBuff[i*2 + 0] = i >> 8;
//        outBuff[i*2 + 1] = i  & 0xff;
//    }
////    sendAndReceive(64, outBuff, inBuff);
//    sendAndReceive(15000, outBuff, inBuff);
//    while(1);


//    BYTE buffer[512];
//    outDebugString("Check start");
//    for(int t=0; t<80; t++) {
//        for(int si=0; si<2; si++) {
//            for(int se=1; se<=9; se++) {
//                image->readSector(t, si, se, buffer);

//                if(buffer[0] != t || buffer[1] != si || buffer[2] != se) {
//                    outDebugString("Data mismatch: %d-%d-%d vs %d-%d-%d", t, si, se, buffer[0], buffer[1], buffer[2]);
//                }
//            }
//        }
//    }
//    outDebugString("Check end");

    while(shouldRun) {
        while(cb_cnt >= 6) {                               // while we have enough data
            int val;
            bfr_get(val);

            switch(val) {
                case ATN_FW_VERSION:
                    handleFwVersion();
                    break;
//                case ATN_SEND_NEXT_SECTOR:
//                    handleSendNextSector(side, track, sector, outBuff, inBuff);
//                    break;
                case ATN_SECTOR_WRITTEN:
                    handleSectorWasWritten();
                    break;
                case ATN_SEND_TRACK:
                    handleSendTrack(side, track, inBuff);
                    break;
            }
        }

        if(!conUsb->isConnected()) {                    // if not connected?
            conUsb->tryToConnect();                     // try to connect and wait

            msleep(100);
            continue;
        }

        if(GetTickCount() - lastTick < 10) {            // less than 10 ms ago?
            msleep(1);
            continue;
        }
        lastTick = GetTickCount();

        memset(outBuff, 0, 6);                          // send 8 zeros to receive any potential command
        sendAndReceive(6, outBuff, inBuff);
    }

    running = false;
}

void CCoreThread::setNextCmd(BYTE cmd)
{
    cmd_add(cmd);
}

void CCoreThread::sendAndReceive(int cnt, BYTE *outBuf, BYTE *inBuf)
{
//    outDebugString("sendAndReceive -- %d", cnt);
//    DWORD start = GetTickCount();

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


    for(int i=0; i<cnt; i++) {                      // add to circular buffer
        bfr_add(inBuf[i]);
    }
//    DWORD end = GetTickCount();
//    outDebugString("sendAndReceive: %d ms", end - start);
}

void CCoreThread::handleFwVersion(void)
{
    BYTE fwVer[4];
    int val;

    for(int i=0; i<4; i++) {
        bfr_get(val);
        fwVer[i] = val;
    }

    if(fwVer[0] == 0xf0) {
        outDebugString("FW: Franz ");
    } else {
        outDebugString("FW: Hans ");
    }

    int year = bcdToInt(fwVer[1]) + 2000;
    outDebugString("%d-%02d-%02d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));

    // receive 6 WORDs == 12 BYTEs, while at least 3 WORDs (6 BYTEs) are received
    int remaining = 12 - (6 + cb_cnt);

    BYTE oBuf[6], iBuf[6];
    memset(oBuf, 0, 6);

    int storeCnt;
    storeCnt = MIN(remaining, 6);           // get the minimum from sizeof(buf), remaining, cd_cnt (what we may send, what we should send, what we got)
    storeCnt = MIN(storeCnt, cd_cnt);

    for(int i=0; i<storeCnt; i++) {         // now copy all the possible bytes (commands) to buffer
        cmd_get(oBuf[i]);
    }

    sendAndReceive(remaining, oBuf, iBuf);
}

int CCoreThread::bcdToInt(int bcd)
{
    int a,b;

    a = bcd >> 4;       // upper nibble
    b = bcd &  0x0f;    // lower nibble

    return ((a * 10) + b);
}

//void CCoreThread::handleSendNextSector(int &side, int &track, int &sector, BYTE *oBuf, BYTE *iBuf)
//{
//    BYTE buf[6];
//    int val;

//    for(int i=0; i<6; i++) {        // get arguments from buffer
//        bfr_get(val);
//        buf[i] = val;
//    }

//    int count;
//    side    = buf[0];               // now read the current floppy position
//    track   = buf[1];
//    sector  = buf[2];

//    outDebugString("ATN_SEND_NEXT_SECTOR -- track %d, side %d, sector %d", track, side, sector);

//    int tr, si, spt;
//    image->getParams(tr, si, spt);  // read the floppy image params

//    if(sector > spt) {              // if we're at the end of the track
//        return;
//    }

//    oBuf[0] = CMD_CURRENT_SECTOR;                           // first send the current sector #
//    oBuf[1] = sector;

//    createMfmStream(side, track, sector, oBuf + 2, count);  // then create the right MFM stream

//    count += 2;                                             // now add the 2 bytes used by CMD_CURRENT_SECTOR

//    if((count & 0x0001) != 0) {                     // if got even number of bytes
//        oBuf[count] = 0;                            // store 0 at last position and increment count
//        count++;
//    }

//    sendAndReceive(count, oBuf, iBuf);                // send and receive data
//}

void CCoreThread::handleSendTrack(int &side, int &track, BYTE *iBuf)
{
    BYTE buf[2];
    int val;

    DWORD start = GetTickCount();

    for(int i=0; i<2; i++) {        // get arguments from buffer
        bfr_get(val);
        buf[i] = val;
    }

    side    = buf[0];               // now read the current floppy position
    track   = buf[1];

    outDebugString("ATN_SEND_TRACK -- track %d, side %d", track, side);

    int tr, si, spt;
    image->getParams(tr, si, spt);  // read the floppy image params

    if(side < 0 || side > 1 || track < 0 || track >= tr) {
        outDebugString("Side / Track out of range!");
        return;
    }

    //---------
    // avoid sending the same track again
    if(lastSide == side && lastTrack == track) {                // if this track is what we've sent last time, don't send it
        return;
    }
    lastSide    = side;
    lastTrack   = track;
    //---------

    BYTE *encodedTrack;
    int countInTrack;

    encodedTrack = encImage.getEncodedTrack(track, side, countInTrack);

    // we want to send total 15000 bytes, while at least 3 WORDs (6 BYTEs) are received
    int remaining   = 15000 - (4 + cb_cnt);                 // this much bytes remain to send after the received ATN

    DWORD mid = GetTickCount();

    sendAndReceive(remaining, encodedTrack, iBuf);          // send and receive data

    DWORD end = GetTickCount();
    outDebugString("handleSendTrack -- encode: %d, tx-rx: %d", mid-start, end-mid);

//    FILE *f = fopen("f:/data.bin", "wb");

//    if(!f) {
//        qDebug() << "dafuq!";
//        return;
//    }

//    fwrite(encodedTrack, 1, remaining, f);
//    fclose(f);
}

void CCoreThread::handleSectorWasWritten(void)
{


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
