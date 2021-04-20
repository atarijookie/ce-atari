#include <osbind.h>

#include "vt52.h"
#include "stdlib.h"

int  getIntFromStr (const char *str, int len);
void showAppVersion(void);

void showMenu(void);

void makeFloppy(void);

void showLineStart(BYTE linearNotRandom, int trNo);
void showLineStartWrite(BYTE linearNotRandom, int trNo);
void updateFloppyPosition(BYTE linearNotRandom, BYTE foreverNotOnce);

void showInt(int value, int length);
BYTE readSector(int sector, int track, int side, BYTE checkData);       // 0 means good, 1 means failed READ operation, 2 means READ good but DATA bad
BYTE checkReadDataForTestImage(int sector, int track, int side);

void deleteOneProgressLine(void);
void calcCheckSum(BYTE *bfr, int sector, int track, int side);
void showHexByte(int val);
void showHexWord(WORD val);

void showDecimal(int num);
void showDiff(BYTE* bfr, int track, int side, int sector, char*range);
void dumpBuffer(BYTE *bfr, WORD count, WORD itemsPerRow);

DWORD getTicks(void);

void print_status_read(void);
void print_status_write(void);

void setImageGeometry(int tracks, int sides, int sectors);

BYTE bfr     [9*512];
BYTE writeBfr[9*512];

void removeAllWaitingKeys(void);
BYTE showDebugInfoFunc(BYTE resultChar, int ms, int howManyTracksSeeked, int lazyTime, int msSeek, int msRead);

//----------------------------------
// floppy API 'polymorphism'

void floppy_on  (BYTE *buf, WORD dev);
void floppy_off (BYTE *buf, WORD dev);
int  floppy_write(BYTE *buf, WORD dev, WORD sector, WORD track, WORD side, WORD count);             // write implentation abstraction of TOS/asm binding
int  floppy_read(BYTE *buf, WORD dev, WORD sector, WORD track, WORD side, WORD count);
void floppy_seekRate(WORD dev, WORD rate);

//----------------------------------
#define RATE_2      2
#define RATE_3      3
#define RATE_6      0
#define RATE_12     1

BYTE seekRate   = RATE_3;
int  seekRateMs = 3;

WORD fdcStatus;

//----------------------------------
// asm fdc floppy interface
extern DWORD argFuncId;
extern DWORD argDrive;
extern DWORD argTrack;
extern DWORD argSide;
extern DWORD argSector;
extern DWORD argCount;
extern DWORD argLeaveOn;
extern DWORD argBufferPtr;
extern DWORD argSuccess;

void runFdcAsm(void);

#define FDC_FUN_RESTORE     0
#define FDC_FUN_SEEK        1
#define FDC_FUN_READ_ONE    2
#define FDC_FUN_READ_MULTI  3
#define FDC_FUN_WRITE       4
#define FDC_FUN_READTRK     5
#define FDC_FUN_WRITETRK    6
//----------------------------------

struct {
    DWORD runs;

    DWORD good;
    DWORD lazy;

    DWORD errRead;
    DWORD errWrite;
    DWORD errData;
} counts;

struct {
    WORD min;
    WORD max;
    WORD avg;
} times;

struct {
    int  randCount;
    BYTE finish;
    BYTE newLine;
    BYTE space;

    int sector;
    int track;
    int side;
} fl;

struct {
    int sectors;
    int tracks;
    int sides;

    int totalSectors;
} imgGeometry;

void writeReadVerifyTest(BYTE linearNotRandom, BYTE foreverNotOnce);
void writeSingleSectorTest(void);
void readTest(BYTE linearNotRandom, BYTE imageTestNotAny, BYTE foreverNotOnce);

#define DATATYPE_RANDOM     0
#define DATATYPE_COUNTER    1
#define DATATYPE_TRSISE     2

struct {
    BYTE fddViaTos;
    BYTE dataType;
    BYTE stopAfterError;
    BYTE readNotWrite;
    BYTE linearNotRandom;
    BYTE imageTestNotAny;
    BYTE foreverNotOnce;
    BYTE sectorsAtOnce;
} testConf;

void showMenu(void)
{
    VT52_Clear_home();

    (void)Cconws("\33p[ Floppy test tool      ver ");
    showAppVersion();
    (void)Cconws(" ]\33q\r\n\r\n");

    testConf.linearNotRandom    ? (void)Cconws(" \33pLinear\33q ")       : (void)Cconws(" \33pL\33qinear ");
    testConf.readNotWrite       ? (void)Cconws("\33pRead \33q ")         : (void)Cconws("\33pR\33qead  ");
    testConf.imageTestNotAny    ? (void)Cconws("of \33ptest Image\33q ") : (void)Cconws("of test \33pI\33qmage ");
    testConf.foreverNotOnce     ? (void)Cconws("\33pforEver\33q\r\n")    : (void)Cconws("for\33pE\33qver\r\n");

    (!testConf.linearNotRandom) ? (void)Cconws(" \33praNdom\33q ")       : (void)Cconws(" ra\33pN\33qdom ");
    (!testConf.readNotWrite)    ? (void)Cconws("\33pWrite\33q ")         : (void)Cconws("\33pW\33qrite ");
    (!testConf.imageTestNotAny) ? (void)Cconws("of \33pAny  image\33q ") : (void)Cconws("of \33pA\33qny  image ");
    (!testConf.foreverNotOnce)  ? (void)Cconws("\33pOnce   \33q\r\n")    : (void)Cconws("\33pO\33qnce   \r\n");

    (void)Cconws("\r\n");
    (void)Cconws(" \33p[ + -  ]\33q - sectors at once: ");
    showInt(testConf.sectorsAtOnce, 1);

    (void)Cconws("\r\n");
    (void)Cconws(" \33p[ 236c ]\33q - seek rate:");
    if(seekRate == RATE_2) (void)Cconws("\33p");
    (void)Cconws(" 2 \33q/");
    if(seekRate == RATE_3) (void)Cconws("\33p");
    (void)Cconws(" 3 \33q/");
    if(seekRate == RATE_6) (void)Cconws("\33p");
    (void)Cconws(" 6 \33q/");
    if(seekRate == RATE_12) (void)Cconws("\33p");
    (void)Cconws(" 12 \33qms\r\n");

    (void)Cconws("\r\n \33p[Return]\33q - execute test\r\n");

    (void)Cconws("\r\n");
    (void)Cconws(" \33p[ F ]\33q - use ");
    if(testConf.fddViaTos) {
        (void)Cconws("\33pTOS\33q/custom functions\r\n");
    } else {
        (void)Cconws("TOS/\33pcustom\33q functions\r\n");
    }

    (void)Cconws(" \33p[ D ]\33q - data ");
    switch(testConf.dataType) {
        case DATATYPE_RANDOM:   (void)Cconws("\33prandom\33q/counter/TrSiSe\r\n"); break;
        case DATATYPE_COUNTER:  (void)Cconws("random/\33pcounter\33q/TrSiSe\r\n"); break;
        case DATATYPE_TRSISE:   (void)Cconws("random/counter/\33pTrSiSe\33q\r\n"); break;
    }

    (void)Cconws(" \33p[ Y ]\33q - toggle stop after error - ");
    if(testConf.stopAfterError) (void)Cconws("\33p");
    (void)Cconws("YES\33q/");
    if(!testConf.stopAfterError) (void)Cconws("\33p");
    (void)Cconws("NO\33q\r\n");

    (void)Cconws("\r\n");
    (void)Cconws(" \33p[ M ]\33q - make TEST floppy image\r\n");
    (void)Cconws(" \33p[ S ]\33q - write sector 1, track 3, side 0\r\n");
    (void)Cconws(" \33p[ Q ]\33q - quit this app\r\n");
}

int main(void)
{
    DWORD oldSp = Super(0);

    testConf.dataType = DATATYPE_RANDOM;
    testConf.fddViaTos = 1;
    testConf.stopAfterError = 0;
    testConf.readNotWrite = 1;
    testConf.linearNotRandom = 1;
    testConf.imageTestNotAny = 1;
    testConf.foreverNotOnce = 0;
    testConf.sectorsAtOnce = 1;

    setImageGeometry(80, 2, 9);

    removeAllWaitingKeys();                                             // read all the possibly waiting keys, so we can ignore them...

    while(1) {
        showMenu();
        char req = Cnecin();

        if(req  >= 'A' && req <= 'Z') {
            req = req + 32;                                             // upper to lower case
        }

        if(req == 'q') {                                               // quit?
            break;
        }

        //---------------------
        // set floppy seek rate
        switch(req) {
            case '2': seekRate = RATE_2;  seekRateMs = 2;                   break;
            case '3': seekRate = RATE_3;  seekRateMs = 3;                   break;
            case '6': seekRate = RATE_6;  seekRateMs = 6;                   break;
            case 'c': seekRate = RATE_12; seekRateMs = 12;                  break;

            case 'y': testConf.stopAfterError = !testConf.stopAfterError;   break;

            case 's': writeSingleSectorTest();                              break;
            case 'm': makeFloppy();                                         break;

            case 'd': {
                testConf.dataType++;
                if(testConf.dataType > DATATYPE_TRSISE) {
                    testConf.dataType = DATATYPE_RANDOM;
                }
            }
            break;

            case 'f': testConf.fddViaTos = !testConf.fddViaTos;             break;

            case 'w':
            case 'r': testConf.readNotWrite = (req == 'r');                 break;

            case 'n':
            case 'l': testConf.linearNotRandom = (req == 'l');              break;

            case 'a':
            case 'i': testConf.imageTestNotAny = (req == 'i');              break;

            case 'o':
            case 'e': testConf.foreverNotOnce = (req == 'e');               break;
        }

        if(req == '+' && testConf.sectorsAtOnce < 9) {
            testConf.sectorsAtOnce++;
        }

        if(req == '-' && testConf.sectorsAtOnce > 1) {
            testConf.sectorsAtOnce--;
        }

        if(req == 13) {         // on ENTER
            if(testConf.readNotWrite) {     // read
                readTest(testConf.linearNotRandom, testConf.imageTestNotAny, testConf.foreverNotOnce);
            } else {                        // write
                writeReadVerifyTest(testConf.linearNotRandom, testConf.foreverNotOnce);
            }
        }

        removeAllWaitingKeys(); // read all the possibly waiting keys, so we can ignore them...
    }

    Super(oldSp);
    return 0;
}

void removeAllWaitingKeys(void)
{
    // read all the possibly waiting keys, so we can ignore them...
    BYTE res;
    while(1) {
        res = Cconis();                 // see if there's something waiting from keyboard

        if(res != 0) {                  // something waiting? read it
            Cnecin();
        } else {                        // nothing waiting, continue with the app
            break;
        }
    }
}

void generateWriteData(void)
{
    BYTE randValue = Random();
    int i, s;

    for(s=0; s<testConf.sectorsAtOnce; s++) {           // for all sectors in test
        BYTE *b = writeBfr + (s * 512);                 // pointer to buffer for specific sector

        for(i=0; i<512; i++) {                          // go through the write buffer, write data
            if(testConf.dataType == DATATYPE_RANDOM) {  // random data?
                b[i] = i ^ randValue;
            } else {                                    // counter data or TrSiSe data?
                b[i] = i;
            }
        }

        if(testConf.dataType == DATATYPE_TRSISE) {      // add TrSiSe data
            b[0] = fl.track;
            b[1] = fl.side;
            b[2] = fl.sector + s;
        }
    }
}

void writeSingleSectorTest(void)
{
    fl.track = 3;
    fl.side = 0;
    fl.sector = 1;

    generateWriteData();                    // generate write data

    floppy_seekRate(0, seekRate);           // set SEEK RATE
    floppy_write(writeBfr, 0, fl.sector, fl.track, fl.side, testConf.sectorsAtOnce);  // sector 1, track 3, side 0
}

void writeReadVerifyTest(BYTE linearNotRandom, BYTE foreverNotOnce)
{
    // init stuff
    BYTE initval=0;             // not sure yet

    counts.runs     = 0;        // keep track of number of runs
    counts.good     = 0;        // number of runs passed
    counts.lazy     = 0;        // ?
    counts.errWrite = 0;        // number of write error
    counts.errData  = 0;        // number of data incorrectly read

    times.min   = 32000;        // min time
    times.max   = 0;            // max time
    times.avg   = 0;            // avg time

    VT52_Clear_home();
    print_status_write();

    fl.finish   = 0;                    // don't finish yet
    fl.newLine  = 0;
    fl.space    = 0;

    fl.side     = 0;
    fl.track    = 0;
    fl.sector   = 1;

    fl.randCount = imgGeometry.totalSectors - 1;

    VT52_Goto_pos(0, 24);
    int x = 11;                         // 11 what?

    showLineStartWrite(linearNotRandom, fl.track);

    //BYTE isAfterStartOrError = 1;

    floppy_seekRate(0, seekRate);           // set SEEK RATE
    floppy_on(bfr, 0);                      // seek to TRACK #0, leave motor on
    //int prevTrack = 0;                    // previous track is now 0

    while(1) {
        //---------------------------------
        // code for termination of test by keyboard
        BYTE res = Cconis();                // see if there's something waiting from keyboard

        if(res != 0) {                      // something waiting?
            char req = Cnecin();

            if(req  >= 'A' && req <= 'Z') {
                req = req + 32;             // upper to lower case
            }

            if(req == 'q' || req == 'c') {  // quit?
                floppy_off(bfr, 0);         // turn floppy off
                break;
            }
        }

        //---------------------------------
        // execute the read test

        memset(bfr, initval++, testConf.sectorsAtOnce * 512);   //just to make sure we are not looking at data from the last read attempt on a failure

        generateWriteData();

        // write
        BYTE wRes = floppy_write(writeBfr, 0, fl.sector, fl.track, fl.side, testConf.sectorsAtOnce);

        BYTE rRes = 0;
        if(!wRes) {     // if write was OK, do read
            rRes = floppy_read(bfr, 0, fl.sector, fl.track, fl.side, testConf.sectorsAtOnce);
        }

        int testOk = 1;                     // default: no error
        BYTE resultChar = '*';              // default result: no error

        if(wRes || rRes) {                  // if write or read failed, not ok
            testOk = 0;

            if(wRes) resultChar = 'W';      // if write failed
            if(rRes) resultChar = 'R';      // if read failed
        } else {                            // if write and read succeeded, time to verify data
            int i;
            for(i=0; i<testConf.sectorsAtOnce * 512; i++){           // verify
                if(bfr[i] != writeBfr[i])
                {
                    testOk = 0;
                    resultChar = 'D';       // if data mismatch
                    break;
                }
            }
        }

        //--------------------------------- // evaluate the result
        counts.runs++;                      // increment the count of runs

        VT52_Goto_pos(x, 24);

        Cconout(resultChar);                // show result char

        if(testOk){
            counts.good++;
        } else {
            counts.errData++;
        }

        if(!testOk && testConf.stopAfterError) {
            BYTE quit = showDebugInfoFunc(resultChar, 0, 0, 0, 0, 0);
            //isAfterStartOrError = 1;

            if(quit) {
                break;
            }
        }
        //prevTrack = fl.track;

        print_status_write();

        //---------------------------------
        // update the next floppy position for writing
        updateFloppyPosition(linearNotRandom, foreverNotOnce);

        if(fl.finish) {                         // if should quit after this
            deleteOneProgressLine();

            (void) Cconws("Finished.");

            print_status_write();

            floppy_off(bfr, 0);                 // turn floppy off
            Cnecin();
            break;
        }

        if(fl.newLine) {                        // should start new line?
            fl.newLine = 0;

            deleteOneProgressLine();
            x = 11;

            showLineStartWrite(linearNotRandom, fl.track);
        } else {                                // just go to next char
            x++;
        }

        if(fl.space) {
            fl.space = 0;
            x++;
        }
    }
}

DWORD afterSeek;

void readTest(BYTE linearNotRandom, BYTE imageTestNotAny, BYTE foreverNotOnce)
{
    BYTE initval=0;                                                                  // not sure yet

    counts.runs     = 0;                                                             // keep track of number of runs
    counts.good     = 0;                                                            // number of runs passed
    counts.lazy     = 0;                                                            // ?
    counts.errRead  = 0;                                                            // number of read error
    counts.errData  = 0;                                                            // number of data incorrectly read

    times.min   = 32000;                                                            // min time
    times.max   = 0;                                                                // max time
    times.avg   = 0;                                                                // avg time

    VT52_Clear_home();
    print_status_read();

    fl.finish   = 0;                // don't finish yet
    fl.newLine  = 0;
    fl.space    = 0;

    fl.side     = 0;
    fl.track    = 0;
    fl.sector   = 1;

    fl.randCount = imgGeometry.totalSectors - 1;

    VT52_Goto_pos(0, 24);
    int x = 11;                     // 11 what?

    showLineStart(linearNotRandom, fl.track);

    BYTE isAfterStartOrError = 1;

    floppy_seekRate(0, seekRate);           // set SEEK RATE
    floppy_on(bfr, 0);                      // seek to TRACK #0, leave motor on
    int prevTrack = 0;                      // previous track is now 0

    while(1) {
        //---------------------------------
        // code for termination of test by keyboard
        BYTE res = Cconis();                // see if there's something waiting from keyboard

        if(res != 0) {                      // something waiting?
            char req = Cnecin();

            if(req  >= 'A' && req <= 'Z') {
                req = req + 32;             // upper to lower case
            }

            if(req == 'q' || req == 'c') {  // quit?
                floppy_off(bfr, 0);         // turn floppy off
                break;
            }
        }

        //---------------------------------
        // execute the read test

        memset(bfr, initval++, testConf.sectorsAtOnce*512);    //just to make sure we are not looking at data from the last read attempt on a failure

        DWORD start, end;
        start       = getTicks();
        afterSeek   = start;                // if custom FDC routines used, this will hold time after seek. Otherwise init to start and will result in zero time.
        BYTE bRes   = floppy_read(bfr, 0, fl.sector, fl.track, fl.side, testConf.sectorsAtOnce);
        end         = getTicks();

        if(bRes) {                          // on any floppy error in floppy_read, change result to 1
            bRes = 1;
        }

        if(bRes == 0 && imageTestNotAny) {  // if test image, we can also check data validity
            testConf.dataType = DATATYPE_TRSISE;    // for test image - use TrSiSe data
            generateWriteData();                    // generate write data

            int i, byteCount = testConf.sectorsAtOnce * 512;
            for(i=0; i<byteCount; i++) {
                if(bfr[i] != writeBfr[i]) { // data mismatch? fail
                    bRes = 2;
                }
            }
        }

        DWORD ms = (end - start) * 5;
        DWORD msSeek = (afterSeek - start) * 5;
        DWORD msRead = (end - afterSeek) * 5;

        //---------------------------------
        // update min / max / avg time, but only if it's not after start (spin up) or error
        if(!isAfterStartOrError) {
            if(times.max < ms) times.max = ms;
            if(times.min > ms) times.min = ms;
            times.avg = (times.avg + ms) / 2;
        }

        //---------------------------------
        // calculate lazy seek time treshold
        int howManyTracksSeeked = (prevTrack > fl.track) ? (prevTrack - fl.track) : (fl.track - prevTrack); // calculate how many sectors we had to seek

        // calculate how many time will be considered as lazy seek - if it's more than SEEK TO TRACK time + time to read all the sectors (22 ms per sector) + 2 floppy spin times, it's too lazy
        DWORD lazyTime = (testConf.sectorsAtOnce * 22) + (howManyTracksSeeked * seekRateMs) + 400;

        //---------------------------------
        // evaluate the result
        counts.runs++;                      // increment the count of runs

        VT52_Goto_pos(x, 24);

        BYTE showDebugInfo = 0;
        BYTE resultChar = '*';

        if(bRes == 0) {                     // read and DATA good
            if(ms > lazyTime && !isAfterStartOrError) { // operation was taking too much time? Too lazy! (but only if it's not after error or start, that way lazy is expected)
                counts.lazy++;

                resultChar      = 'L';
                showDebugInfo   = 1;
            } else {                        // operation was fast enough
                counts.good++;

                resultChar      = '*';
                showDebugInfo   = 0;
            }

            isAfterStartOrError = 0;        // mark that error didn't happen, so next lazy read is really lazy read
        } else if(bRes == 1) {              // operation failed - Floprd failed
            counts.errRead++;

            resultChar      = '!';
            showDebugInfo   = 1;

            isAfterStartOrError = 1;        // mark that error happened, next read might be lazy and it will be OK (floppy needs some time to get back to normal)
        } else if(bRes == 2) {              // operation failed - data mismatch
            counts.errData++;

            resultChar      = 'D';
            showDebugInfo   = 1;

            isAfterStartOrError = 0;        // mark that error didn't happen, so next lazy read is really lazy read
        }

        Cconout(resultChar);                // show result of this operation

        if(showDebugInfo && testConf.stopAfterError) {
            BYTE quit = showDebugInfoFunc(resultChar, ms, howManyTracksSeeked, lazyTime, msSeek, msRead);
            isAfterStartOrError = 1;

            if(quit) {
                break;
            }
        }
        prevTrack = fl.track;

        print_status_read();

        //---------------------------------
        // update the next floppy position for reading
        updateFloppyPosition(linearNotRandom, foreverNotOnce);

        if(fl.finish) {                         // if should quit after this
            deleteOneProgressLine();

            (void) Cconws("Finished.\r\n");

            (void) Cconws("\r\nTime min: ");
            showInt(times.min, 4);
            (void) Cconws("\r\nTime max: ");
            showInt(times.max, 4);
            (void) Cconws("\r\nTime avg: ");
            showInt(times.avg, 4);
            (void) Cconws("\r\n");

            print_status_read();

            floppy_off(bfr, 0);                 // turn floppy off
            Cnecin();
            break;
        }

        if(fl.newLine) {                        // should start new line?
            fl.newLine = 0;

            deleteOneProgressLine();
            x = 11;

            showLineStart(linearNotRandom, fl.track);
        } else {                                // just go to next char
            x++;
        }

        if(fl.space) {
            fl.space = 0;
            x++;
        }
    }
}

BYTE showDebugInfoFunc(BYTE resultChar, int ms, int howManyTracksSeeked, int lazyTime, int msSeek, int msRead)
{
    VT52_Goto_pos(0, 5);

    (void) Cconws("Tr, Si, Se: ");
    showInt(fl.track, 2);
    (void) Cconws(",");
    showInt(fl.side, 1);
    (void) Cconws(",");
    showInt(fl.sector, 1);

    (void) Cconws("  \r\nOp result :    ");
    Cconout(resultChar);
    (void) Cconws("  \r\nFDC status:   ");
    showHexByte(fdcStatus);
    (void) Cconws("  \r\nOp time   : ");
    showInt(ms, 4);

    if(msSeek > 0) {    // if got some seek time, show it
        (void) Cconws("  \r\nSeek time : ");
        showInt(msSeek, 4);

        (void) Cconws("  \r\nRead time : ");
        showInt(msRead, 4);
    }

    (void) Cconws("  \r\nSeek count:   ");
    showInt(howManyTracksSeeked, 2);
    (void) Cconws("  \r\nLazy thres: ");
    showInt(lazyTime, 4);
    (void) Cconws("  \r\n");

    char req = Cnecin();    // wait for key

    if(req  >= 'A' && req <= 'Z') {
        req = req + 32;     // upper to lower case
    }

    VT52_Goto_pos(0, 5);
    int i;
    for(i=0; i<6; i++) {    // clear debug output
        (void) Cconws("                      \r\n");
    }

    if(req == 'r') {        // on R - dump read buffer
        dumpBuffer(bfr, 512, 32);
        Cnecin();
    }

    if(req == 'w') {        // on W - dump write buffer
        dumpBuffer(writeBfr, 512, 32);
        Cnecin();
    }

    if(req == 'q' || req == 'c') {
        return 1;
    }

    return 0;
}

void deleteOneProgressLine(void)
{
    VT52_Goto_pos(0,4);
    VT52_Del_line();
    VT52_Goto_pos(0, 24);
}

void showLineStart(BYTE linearNotRandom, int trNo)
{
    if(linearNotRandom) {
        (void) Cconws("TRACK ");
        showInt(trNo, 2);
        (void) Cconws(": ");
    } else {
        (void) Cconws("Rand read: ");
    }
}

void showLineStartWrite(BYTE linearNotRandom, int trNo)
{
    if(linearNotRandom) {
        (void) Cconws("TRACK ");
        showInt(trNo, 2);
        (void) Cconws(": ");
    } else {
        (void) Cconws("Rand write: ");
    }
}

void updateFloppyPosition(BYTE linearNotRandom, BYTE foreverNotOnce)
{
    // if trying to write N sectors, the last one you should try is N sectors from the end of track
    int maxSectorForSectorsAtOnce = imgGeometry.sectors + 1 - testConf.sectorsAtOnce;

    //-----------------
    // for random order
    if(!linearNotRandom) {
        fl.sector  = (Random() % maxSectorForSectorsAtOnce) + 1;
        fl.side    = (Random() % imgGeometry.sides);
        fl.track   = (Random() % imgGeometry.tracks);

        if((fl.randCount % 20) == 0) {
            fl.newLine = 1;         // make new line after this
        }

        if(fl.randCount > 0) {      // more than 0 runs to go?
            fl.randCount--;
        } else {                    // we did all the runs
            fl.randCount = imgGeometry.totalSectors - 1;

            if(!foreverNotOnce) {   // if once, quit
                fl.finish = 1;      // quit after this
            }
        }

        return;
    }

    //-----------------
    // for sequential order
    if(fl.sector < maxSectorForSectorsAtOnce) {   // not last sector? next sector
        fl.sector++;
    } else {                            // last sector?
        fl.sector = 1;

        if(fl.side < 1) {               // side 0?
            fl.side     = 1;
            fl.space    = 1;            // add space between sides
        } else {                        // side 1?
            fl.side     = 0;
            fl.newLine  = 1;            // make new line after this, because next track

            if(fl.track < (imgGeometry.tracks - 1)) {   // Not the last track? Next track!
                fl.track++;
            } else {                                    // It's the last track? restart or quit
                fl.track = 0;

                if(!foreverNotOnce) {   // if once, quit
                    fl.finish = 1;      // quit after this
                }
            }
        }
    }
}

void showInt(int value, int length)
{
    char tmp[10];
    memset(tmp, 0, 10);

    int i;
    for(i=0; i<length; i++) {               // go through the int lenght and get the digits
        int val, mod;

        val = value / 10;
        mod = value % 10;

        tmp[length - 1 - i] = mod + 48;     // store the current digit

        value = val;
    }

    (void) Cconws(tmp);                     // write it out
}

void print_status_read(void)
{
    VT52_Goto_pos(0,0);

    (void) Cconws("\33p[ CosmosEx FDD test.     Reads: ");
    showInt(counts.runs, 4);
    (void) Cconws("   ]\33q\r\n");

    (void) Cconws("\33p[         Good: ");
    showInt(counts.good, 4);
    (void) Cconws("      Lazy: ");
    showInt(counts.lazy, 4);
    (void) Cconws("   ]\33q\r\n");

    (void) Cconws("\33p[   Err Floprd: ");
    showInt(counts.errRead, 4);
    (void) Cconws("  Err DATA: ");
    showInt(counts.errData, 4);
    (void) Cconws("   ]\33q\r\n");
}

void print_status_write(void)
{
    VT52_Goto_pos(0,0);

    (void) Cconws("\33p[ CosmosEx FDD test.    Writes: ");
    showInt(counts.runs, 4);
    (void) Cconws("   ]\33q\r\n");

    (void) Cconws("\33p[         Good: ");
    showInt(counts.good, 4);
    (void) Cconws("      Lazy: ");
    showInt(counts.lazy, 4);
    (void) Cconws("   ]\33q\r\n");

    (void) Cconws("\33p[   Err Flopwr: ");
    showInt(counts.errRead, 4);
    (void) Cconws("  Err DATA: ");
    showInt(counts.errData, 4);
    (void) Cconws("   ]\33q\r\n");
}

void makeFloppy(void)
{
    int track, side, sector;

    VT52_Clear_home();
    (void)Cconws("Writing TEST floppy...\r\n");

    for(track=0; track<80; track++) {
        (void)Cconws("\r\nTrack ");
        showInt(track, 2);
        (void)Cconws(": ");

        for(side=0; side<2; side++) {
            testConf.dataType = DATATYPE_TRSISE;

            fl.track = track;
            fl.side = side;

            for(sector=1; sector<=9; sector++) {
                fl.sector = sector;

                if(Cconis()) {
                    Cnecin();
                    (void) Cconws("\r\nCanceled...\r\n");
                    return;
                }

                generateWriteData();                    // generate write data

                floppy_write(writeBfr, 0, fl.sector, fl.track, fl.side, 1);
                (void)Cconws(".");
            }
        }
    }
    (void)Cconws("\r\nDone.\r\n");
    Cnecin();
}

void showAppVersion(void)
{
    char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char const *buildDate = __DATE__;

    int year = 0, month = 0, day = 0;
    int i;
    for(i=0; i<12; i++) {
        if(strncmp(months[i], buildDate, 3) == 0) {
            month = i + 1;
            break;
        }
    }

    day     = getIntFromStr(buildDate + 4, 2);
    year    = getIntFromStr(buildDate + 7, 4);

    if(day > 0 && month > 0 && year > 0) {
        showInt(year, 4);
        (void) Cconout('-');
        showInt(month, 2);
        (void) Cconout('-');
        showInt(day, 2);
    } else {
        (void) Cconws("YYYY-MM-DD");
    }
}

int getIntFromStr(const char *str, int len)
{
    int i;
    int val = 0;

    for(i=0; i<len; i++) {
        int digit;

        if(str[i] >= '0' && str[i] <= '9') {
            digit = str[i] - '0';
        } else {
            digit = 0;
        }

        val *= 10;
        val += digit;
    }

    return val;
}

void showHexByte(int val)
{
    int hi, lo;
    char tmp[3];
    char table[16] = {"0123456789ABCDEF"};

    hi = (val >> 4) & 0x0f;;
    lo = (val     ) & 0x0f;

    tmp[0] = table[hi];
    tmp[1] = table[lo];
    tmp[2] = 0;

    (void) Cconws(tmp);
}

void showHexWord(WORD val)
{
    BYTE a,b;
    a = val >>  8;
    b = val;

    showHexByte(a);
    showHexByte(b);
}

void setImageGeometry(int tracks, int sides, int sectors)
{
    imgGeometry.sectors = sectors;
    imgGeometry.tracks  = tracks;
    imgGeometry.sides   = sides;

    imgGeometry.totalSectors = sectors * tracks * sides;
}

// given set implementation (TOS/asm), will:
//  1). seek to the given sector,track,side; and return if error
//  2). given no error, write bfr (512 bytes) to set sector, return result code
int floppy_write(BYTE *wrbuf, WORD dev, WORD sector, WORD track, WORD side, WORD count)
{
    // floppy write via TOS?
    if(testConf.fddViaTos){
        return Flopwr(wrbuf, 0, dev, sector, track, side, count);
    }

    // floppy write via custom routines
    // 1). seek
    argFuncId       = FDC_FUN_SEEK; // seek to the right track
    argDrive        = dev;
    argTrack        = track;
    argSide         = side;
    argLeaveOn      = 1;
    argBufferPtr    = (DWORD) wrbuf;
    runFdcAsm();                            // do the requested action

    if(argSuccess != 0) {                   // failed?
        return argSuccess;                  // return that error
    }

//#define WRITE_ALL_IN_ONE_CALL

#ifdef WRITE_ALL_IN_ONE_CALL
    argFuncId = FDC_FUN_WRITE;          // now write the sector
    argCount = count;                   // write COUNT sectors at once
    argSector = sector;                 // sector number
    argLeaveOn = 1;                     // leave drive ON after cmd

    runFdcAsm();                        // do the requested action
#else
    // 2). write
    argFuncId = FDC_FUN_WRITE;          // now write the sector
    argCount = 1;                       // write one sector at a time, count times
    argLeaveOn = 1;                     // leave drive ON after cmd

    int i;
    for(i=0; i<count; i++) {            // work around for write multiple sectors not working by doing multiple single sector writes
        argBufferPtr = (DWORD) (wrbuf + (i * 512)); // pointer to the right buffer
        argSector = sector + i;                     // sector number

        runFdcAsm();                    // do the requested action

        if(argSuccess != 0) {           // if op failed, quit loop
            break;
        }
    }
#endif

    return argSuccess;                  // return success / failure
}

int floppy_read(BYTE *buf, WORD dev, WORD sector, WORD track, WORD side, WORD count)
{
    // floppy read via TOS?
    if(testConf.fddViaTos) {
        return Floprd(buf, 0, dev, sector, track, side, count);
    }

    //-------------
    // floppy read via custom functions?
    // first seek
    argFuncId       = FDC_FUN_SEEK; // seek to the right track
    argDrive        = dev;
    argTrack        = track;
    argSide         = side;
    argSector       = sector;
    argCount        = count;
    argLeaveOn      = 1;
    argBufferPtr    = (DWORD) buf;

    runFdcAsm();                    // do the requested action

    afterSeek = getTicks();         // time stamp after seek finished
    
    if(argSuccess != 0) {           // failed?
        return argSuccess;          // return that error
    }

    //-------------
    // then read
//#define READ_ALL_IN_ONE_CALL

#ifdef READ_ALL_IN_ONE_CALL
    argFuncId = FDC_FUN_READ_MULTI;    // now read the sector
    argCount = count;                  // read one sector at a time, count times

    runFdcAsm();                       // do the requested action
#else
    argFuncId = FDC_FUN_READ_ONE;      // now read the sector
    argCount = 1;                      // read one sector at a time, count times

    int i;
    for(i=0; i<count; i++) {
        argBufferPtr = (DWORD) (buf + (i * 512));   // pointer to the right buffer
        argSector = sector + i;                     // sector number

        runFdcAsm();                    // do the requested action

        if(argSuccess != 0) {           // if op failed, quit loop
            break;
        }
    }
#endif

    return argSuccess;                  // return success / failure
}

void floppy_seekRate(WORD dev, WORD rate)
{
    if(testConf.fddViaTos) {     // for TOS functions
        (void) Floprate(dev, rate);
    } else {            // for asm functions

    }
}

void floppy_on(BYTE *buf, WORD dev)
{
    if(!testConf.fddViaTos) {                        // for asm functions only
        argFuncId       = FDC_FUN_RESTORE;  // go to TRACK #0
        argDrive        = dev;
        argTrack        = 0;
        argSide         = 0;
        argSector       = 1;
        argCount        = 1;
        argLeaveOn      = 1;                // leave MOTOR ON
        argBufferPtr    = (DWORD) buf;

        runFdcAsm();                        // do the requested action
    }
}

void floppy_off(BYTE *buf, WORD dev)
{
    if(!testConf.fddViaTos) {                        // for asm functions only
        argFuncId       = FDC_FUN_RESTORE;  // go to TRACK #0
        argDrive        = dev;
        argTrack        = 0;
        argSide         = 0;
        argSector       = 1;
        argCount        = 1;
        argLeaveOn      = 0;                // turn MOTOR OFF
        argBufferPtr    = (DWORD) buf;

        runFdcAsm();                        // do the requested action
    }
}

void dumpBuffer(BYTE *bfr, WORD count, WORD itemsPerRow)
{
    int i, j;
    int rows = (count / itemsPerRow) + (((count % itemsPerRow) == 0) ? 0 : 1);

    (void)Cconws("\r\n");

    for(i=0; i<rows; i++) {
        int ofs = i * itemsPerRow;

        for(j=0; j<itemsPerRow; j++) {
            if((ofs + j) < count) {
                showHexByte(bfr[ofs + j]);
            } else {
                (void)Cconws("  ");
            }
        }

        //(void)Cconws(" | ");

        for(j=0; j<itemsPerRow; j++) {
            char v = bfr[ofs + j];
            v = (v >= 32 && v <= 126) ? v : '.';

            if((ofs + j) < count) {
                Cconout(v);
            } else {
                Cconout(' ');
            }
        }

        (void)Cconws("\r\n");
    }
}
