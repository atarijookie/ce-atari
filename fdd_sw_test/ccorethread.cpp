#include <QDir>
#include <QDebug>

#include "global.h"
#include "ccorethread.h"
#include "floppyimagefactory.h"
#include "mfmdecoder.h"

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

    sendSingleHalfWord = false;

    conUsb      = NULL;
    createConnectionObject();
    conUsb->tryToConnect();

    lastSide        = -1;
    lastTrack       = -1;

    prevAtnWord.got = false;
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

    image = imageFactory.getImage((char *) "fcreated.st");

    if(!image) {
        outDebugString("Image file type not supported!");
        return;
    }

    if(!image->isOpen()) {
        outDebugString("Image is not open!");
        return;
    }

    outDebugString("Encoding image...");
    encImage.encodeAndCacheImage(image, true);
    outDebugString("...done");

    /////////////
//    BYTE *encodedTrack;
//    int countInTrack;
//    encodedTrack = encImage.getEncodedTrack(0, 0, countInTrack);

//    BYTE data[15000];
//    int cnt;
//    memset(data, 0, 15000);

//    MfmDecoder md;
//    md.decodeStream(encodedTrack, countInTrack, data, cnt);

//    FILE *g = fopen("C:\\decoded.bin", "wb");
//    fwrite(data, 1, 15000, g);
//    fclose(g);
    /////////////

//    memset(circBfr, 0, 15000);
//    handleSendTrack(side,track,inBuff);
//    return;


//    for(int i=0; i<7500; i++) {
//        outBuff[i*2 + 0] = i >> 8;
//        outBuff[i*2 + 1] = i  & 0xff;
//    }
//////    sendAndReceive(64, outBuff, inBuff);
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

//    for(int i=0; i<7500; i++) {
//        outBuff[i*2 + 1] = i & 0xff;
//        outBuff[i*2 + 0] = i >> 8;
//    }

//    while(1) {
//        conUsb->txRx(15000, outBuff, inBuff);
//    }
/*
    qDebug() << "Waiting for data...";

    WORD wprev = 0;
    while(1) {
        getAtnWord(inBuff);

        if(inBuff[0] != 0 || inBuff[1] != 0) {
            memset(outBuff, 0, 15000);
            conUsb->txRx(15000, outBuff, inBuff);

            for(int i=0; i<7500; i++) {
                WORD wval, wdiff;

                wval = (inBuff[i*2 + 0] << 8) | inBuff[i*2 + 1];
                wdiff = wval - wprev;
                wprev = wval;

                logToFile(wdiff);
            }

            qDebug() << "Done!";
            return;
        }
    }
*/
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
            //                case ATN_SEND_NEXT_SECTOR:
            //                    handleSendNextSector(side, track, sector, outBuff, inBuff);
            //                    break;
        case ATN_SECTOR_WRITTEN:
            handleSectorWasWritten();
            break;
        case ATN_SEND_TRACK:
            handleSendTrack(side, track);
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

    if(!storeInData) {
        return;
    }

    for(int i=0; i<cnt; i++) {                      // add to circular buffer
        bfr_add(inBuf[i]);
    }

//    DWORD end = GetTickCount();
//    outDebugString("sendAndReceive: %d ms", end - start);
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

void CCoreThread::handleSendTrack(int &side, int &track)
{
    BYTE oBuf[4], iBuf[15000];
    DWORD start = GetTickCount();

    memset(oBuf, 0, 4);
    conUsb->txRx(4, oBuf, iBuf);

    logToFile((char *) "handleSendTrack: \nOUT:\n");
    logToFile(4, oBuf);
    logToFile((char *) "\nIN:\n");
    logToFile(4, iBuf);
    logToFile((char *) "\n");

    side    = iBuf[0];               // now read the current floppy position
    track   = iBuf[1];

    outDebugString("ATN_SEND_TRACK -- track %d, side %d", track, side);

    int tr, si, spt;
    image->getParams(tr, si, spt);  // read the floppy image params

    if(side < 0 || side > 1 || track < 0 || track >= tr) {
        outDebugString("Side / Track out of range!");
        return;
    }

    //---------
//    // avoid sending the same track again
//    if(lastSide == side && lastTrack == track) {                // if this track is what we've sent last time, don't send it
//        return;
//    }
//    lastSide    = side;
//    lastTrack   = track;
    //---------

    BYTE *encodedTrack;
    int countInTrack;

    encodedTrack = encImage.getEncodedTrack(track, side, countInTrack);

    int remaining   = 15000 - 6 -2;                 // this much bytes remain to send after the received ATN

    DWORD mid = GetTickCount();

    conUsb->txRx(remaining, encodedTrack, iBuf);

    logToFile((char *) "handleSendTrack -- rest: \nOUT:\n");
    logToFile(remaining, encodedTrack);
    logToFile((char *) "\nIN:\n");
    logToFile(remaining, iBuf);
    logToFile((char *) "\n");

    DWORD end = GetTickCount();
//    outDebugString("handleSendTrack -- encode: %d, tx-rx: %d", mid-start, end-mid);

    setAtnWord(&iBuf[remaining - 2]);               // add the last WORD as possible to check for the new ATN
}

void CCoreThread::sendHalfWord(void)
{
    sendSingleHalfWord = true;
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

void CCoreThread::logToFile(char *str)
{
    FILE *f = fopen("f:/fddlog.txt", "at");

    if(!f) {
        qDebug() << "dafuq!";
        return;
    }

    fprintf(f, str);
    fclose(f);
}

void CCoreThread::logToFile(WORD wval)
{
    FILE *f = fopen("f:/fddlog.txt", "at");

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
    FILE *f = fopen("f:\\fddlog.txt", "at");

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
