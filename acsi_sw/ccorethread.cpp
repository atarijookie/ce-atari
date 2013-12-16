#include <QDir>
#include <QDebug>

#include "global.h"
#include "ccorethread.h"
#include "native/scsi_defs.h"
#include "settings.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define LOGFILE     "H:/acsilog.txt"
#define MEDIAFILE   "C:/datamedia.img"

QStringList dbg;

extern "C" void outDebugString(const char *format, ...);

CCoreThread::CCoreThread()
{
    setEnabledIDbits = false;

    shouldRun       = true;
    running         = false;

    sendSingleHalfWord = false;

    conUsb      = NULL;
    createConnectionObject();
    conUsb->tryToConnect();

    dataTrans   = new AcsiDataTrans();
    dataTrans->setCommunicationObject(conUsb);

    scsi        = new Scsi();
    scsi->setAcsiDataTrans(dataTrans);
    scsi->attachToHostPath(MEDIAFILE, SOURCETYPE_IMAGE_TRANSLATEDBOOT, SCSI_ACCESSTYPE_FULL);

    translated = new TranslatedDisk();
    translated->setAcsiDataTrans(dataTrans);

    translated->attachToHostPath("H:\\dom", TRANSLATEDTYPE_NORMAL);


    confStream = new ConfigStream();
    confStream->setAcsiDataTrans(dataTrans);
}

CCoreThread::~CCoreThread()
{
    if(isRunning()) {
        stopRunning();
    }

    CCoreThread::displayDbg();

    delete conUsb;
    delete dataTrans;
    delete scsi;

    delete translated;
    delete confStream;
}

void CCoreThread::displayDbg(void)
{
    for(int i=0; i<dbg.count(); i++) {
        qDebug() << dbg.at(i);
    }
}

void CCoreThread::run(void)
{
    BYTE inBuff[2];

    DWORD lastTick = GetTickCount();
    running = true;

    outDebugString("Core thread starting...");

    if(!conUsb->isConnected()) {
        outDebugString("USB not connected, quit.");
        return;
    }

    loadSettings();

    while(shouldRun) {
        if(GetTickCount() - lastTick < 10) {            // less than 10 ms ago?
            msleep(1);
            continue;
        }
        lastTick = GetTickCount();

        if(sendSingleHalfWord) {
            BYTE halfWord = 0, inHalf;
            conUsb->txRx(1, &halfWord, &inHalf, false);
            sendSingleHalfWord = false;
        }

        conUsb->getAtnWord(inBuff);

        if(inBuff[0] != 0 || inBuff[1] != 0) {
            logToFile((char *) "Waiting for ATN: \nIN:\n");
            logToFile(2, inBuff);
            logToFile((char *) "\n");
        }

        switch(inBuff[1]) {
        case 0:                 // this is valid, just empty data, skip this
            break;

        case ATN_FW_VERSION:
            handleFwVersion();
            break;

        case ATN_ACSI_COMMAND:
            handleAcsiCommand();
            break;

        default:
            logToFile((char *) "That ^^^ shouldn't happen!\n");
            break;
        }
    }

    running = false;
}

void CCoreThread::handleAcsiCommand(void)
{
    #define CMD_SIZE    14

    BYTE bufOut[CMD_SIZE], bufIn[CMD_SIZE];
    memset(bufOut, 0, CMD_SIZE);

    conUsb->txRx(14, bufOut, bufIn);        // get 14 cmd bytes
    outDebugString("\nhandleAcsiCommand: %02x %02x %02x %02x %02x %02x", bufIn[0], bufIn[1], bufIn[2], bufIn[3], bufIn[4], bufIn[5]);

    BYTE justCmd = bufIn[0] & 0x1f;
    BYTE wasHandled = FALSE;

    BYTE acsiId = bufIn[0] >> 5;                        // get just ACSI ID

    if(acsiIDdevType[acsiId] == DEVTYPE_OFF) {          // if this ACSI ID is off, reply with error and quit
        dataTrans->setStatus(SCSI_ST_CHECK_CONDITION);
        dataTrans->sendDataAndStatus();
        return;
    }

    // ok, so the ID is right, let's see what we can do
    if(justCmd == 0) {                              // if the command is 0 (TEST UNIT READY)
        if(bufIn[1] == 'C' && bufIn[2] == 'E') {    // and this is CosmosEx specific command

            switch(bufIn[3]) {
            case HOSTMOD_CONFIG:                    // config console command?
                wasHandled = TRUE;
                confStream->processCommand(bufIn);
                break;

            case HOSTMOD_TRANSLATED_DISK:
                wasHandled = TRUE;
                translated->processCommand(bufIn);
                break;
            }
        }
    } else if(justCmd == 0x1f) {                    // if the command is ICD mark
        BYTE justCmd2 = bufIn[1] & 0x1f;

        if(justCmd2 == 0 && bufIn[2] == 'C' && bufIn[3] == 'E') {    // the command is 0 (TEST UNIT READY), and this is CosmosEx specific command

            switch(bufIn[4]) {
            case HOSTMOD_TRANSLATED_DISK:
                wasHandled = TRUE;
                translated->processCommand(bufIn + 1);
                break;
            }
        }
    }

    if(wasHandled != TRUE) {                        // if the command was not previously handled, it's probably just some SCSI command
        scsi->processCommand(bufIn);                // process the command
    }
}

void CCoreThread::reloadSettings(void)
{
    loadSettings();
}

void CCoreThread::loadSettings(void)
{
    Settings s;
    enabledIDbits = 0;                                    // no bits / IDs enabled yet

    char key[32];
    for(int id=0; id<8; id++) {							// read the list of device types from settings
        sprintf(key, "ACSI_DEVTYPE_%d", id);			// create settings KEY, e.g. ACSI_DEVTYPE_0
        int devType = s.getInt(key, DEVTYPE_OFF);

        if(devType < 0) {
            devType = DEVTYPE_OFF;
        }

        acsiIDdevType[id] = devType;

        if(devType != DEVTYPE_OFF) {                    // if ON
            enabledIDbits |= (1 << id);                 // set the bit to 1
        }
    }

    setEnabledIDbits = true;
}

void CCoreThread::handleFwVersion(void)
{
    BYTE fwVer[10], oBuf[10];

    memset(oBuf, 0, 10);                    // first clear the output buffer

    if(setEnabledIDbits) {                  // if we should send ACSI ID configuration
        oBuf[3] = CMD_ACSI_CONFIG;          // CMD: send acsi config
        oBuf[4] = enabledIDbits;            // store ACSI enabled IDs
        setEnabledIDbits = false;           // and don't sent this anymore (until needed)
    }

    conUsb->txRx(10, oBuf, fwVer);

    logToFile((char *) "handleFwVersion: \nOUT:\n");
    logToFile(10, oBuf);
    logToFile((char *) "\nIN:\n");
    logToFile(10, fwVer);
    logToFile((char *) "\n");

    int year = bcdToInt(fwVer[1]) + 2000;
    if(fwVer[0] == 0xf0) {
        outDebugString("FW: Franz, %d-%02d-%02d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));
    } else {
        outDebugString("FW: Hans,  %d-%02d-%02d", year, bcdToInt(fwVer[2]), bcdToInt(fwVer[3]));
    }
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
