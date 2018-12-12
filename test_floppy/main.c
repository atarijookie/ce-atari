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
BYTE writeSector(int sector, int track, int side);

void deleteOneProgressLine(void);
void calcCheckSum(BYTE *bfr, int sector, int track, int side);
WORD getWholeCheckSum(BYTE linearNotRandom);
void showHexByte(int val);
void showHexWord(WORD val);

void showDecimal(int num);
void showDiff(BYTE* bfr, int track, int side, int sector, char*range);

DWORD getTicks(void);

void print_status_read(void);
void print_status_write(void);

void setImageGeometry(int tracks, int sides, int sectors);
void guessImageGeometry(void);

BYTE writeBfr[512];

void removeAllWaitingKeys(void);
BYTE showDebugInfoFunc(BYTE resultChar, int ms, int howManyTracksSeeked, int lazyTime);

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
extern DWORD argLeaveOn;
extern DWORD argBufferPtr;
extern DWORD argSuccess;

void runFdcAsm(void);

#define FDC_FUN_RESTORE     0
#define FDC_FUN_SEEK        1
#define FDC_FUN_STEP        2
#define FDC_FUN_STEPIN      3
#define FDC_FUN_STEPOUT     4
#define FDC_FUN_READ        5
#define FDC_FUN_WRITE       6
#define FDC_FUN_READID      7
#define FDC_FUN_READTRK     8
#define FDC_FUN_WRITETRK    9
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

//             TRACK SIDE SECTOR
BYTE checksums[90][2][15];

struct {
    int sectors;
    int tracks;
    int sides;

    int totalSectors;
} imgGeometry;

void writeReadVerifyTest(BYTE linearNotRandom, BYTE imageTestNotAny, BYTE foreverNotOnce);
void writeSingleSectorTest(void);
void readTest(BYTE linearNotRandom, BYTE imageTestNotAny, BYTE foreverNotOnce);

struct {
    BYTE fddViaTos;
    BYTE stopAfterError;
    BYTE readNotWrite;
    BYTE linearNotRandom;
    BYTE imageTestNotAny;
    BYTE foreverNotOnce;
} testConf;

void showMenu(void)
{
    VT52_Clear_home();

    (void)Cconws("\33p[ Floppy test tool      ver ");
    showAppVersion();
    (void)Cconws(" ]\33q\r\n");

    (void)Cconws("\r\n\33p");
    testConf.linearNotRandom ? (void)Cconws("Linear ")     : (void)Cconws("Random ");
    testConf.readNotWrite    ? (void)Cconws("Read  of ")   : (void)Cconws("Write of ");
    testConf.imageTestNotAny ? (void)Cconws("Test image ") : (void)Cconws("Any image  ");
    testConf.foreverNotOnce  ? (void)Cconws("forEver ")    : (void)Cconws("Once    ");
    (void)Cconws("\33q\r\n");

    (void)Cconws("\33pL\33qinear \33pR\33qead     test \33pI\33qmage for\33pE\33qver\r\n");
    (void)Cconws("ra\33pN\33qdom \33pW\33qrite    \33pA\33qny  image \33pO\33qnce   \r\n");
    
    (void)Cconws("\r\n");
    (void)Cconws(" \33p[ T ]\33q - ");
    if(testConf.fddViaTos) (void)Cconws("\33p");
    (void)Cconws("use TOS function Floprd\33q\r\n");
    (void)Cconws(" \33p[ F ]\33q - ");
    if(!testConf.fddViaTos) (void)Cconws("\33p");
    (void)Cconws("use custom FDC function\33q\r\n");

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

    (void)Cconws("\r\n");
    (void)Cconws(" \33p[ Y ]\33q - toggle stop after error - ");
    if(testConf.stopAfterError) (void)Cconws("\33p");
    (void)Cconws("YES\33q/");
    if(!testConf.stopAfterError) (void)Cconws("\33p");
    (void)Cconws("NO\33q\r\n");

    (void)Cconws("\r\n");
    (void)Cconws(" \33p[ M ]\33q - make TEST floppy image\r\n");
    (void)Cconws(" \33p[ S ]\33q - write side 1, track 2, sector 3\r\n");
    (void)Cconws(" \33p[ Q ]\33q - quit this app\r\n");
}

int main(void)
{
    DWORD oldSp = Super(0);

    testConf.fddViaTos = 1;
    testConf.stopAfterError = 0;
    testConf.readNotWrite = 1;
    testConf.linearNotRandom = 1;
    testConf.imageTestNotAny = 1;
    testConf.foreverNotOnce = 0;

    int i;
    for(i=0; i<512; i++) {
        writeBfr[i] = (BYTE) i;
    }

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

            case 'f':
            case 't': testConf.fddViaTos = (req == 't');                    break;

            case 'w':
            case 'r': testConf.readNotWrite = (req == 'r');                 break;

            case 'n':
            case 'l': testConf.linearNotRandom = (req == 'l');              break;

            case 'a':
            case 'i': testConf.imageTestNotAny = (req == 'i');              break;

            case 'o':
            case 'e': testConf.foreverNotOnce = (req == 'e');               break;
        }

        if(req == 13) {         // on ENTER
            if(testConf.readNotWrite) {     // read
                readTest(testConf.linearNotRandom, testConf.imageTestNotAny, testConf.foreverNotOnce);
            } else {                        // write
                writeReadVerifyTest(testConf.linearNotRandom, testConf.imageTestNotAny, testConf.foreverNotOnce);
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

void writeSingleSectorTest(void)
{
    // set the buffer such that each byte is known thing
    int i;
    for(i=0; i<512; i++) {
        writeBfr[i] = (BYTE) i;
    }

    WORD tics = getTicks();
    writeBfr[510] = (BYTE) (tics >> 8);     // last two bytes are 'random' to keep the content changing
    writeBfr[511] = (BYTE) (tics     );

    floppy_seekRate(0, seekRate);           // set SEEK RATE
    floppy_write(writeBfr, 0, 3, 2, 1, 1);  // side 1, track 2, sector 3
}

void writeReadVerifyTest(BYTE linearNotRandom, BYTE imageTestNotAny, BYTE foreverNotOnce)
{
    // init stuff
    BYTE bfr[512];                                                                  // buffer for one sector
    BYTE initval=0;                                                                 // not sure yet

    counts.runs     = 0;                                                            // keep track of number of runs
    counts.good     = 0;                                                            // number of runs passed
    counts.lazy     = 0;                                                            // ?
    counts.errWrite = 0;                                                            // number of write error
    counts.errData  = 0;                                                            // number of data incorrectly read

    times.min   = 32000;                                                            // min time
    times.max   = 0;                                                                // max time
    times.avg   = 0;                                                                // avg time

    int sect,tr,si;
    for(si=0; si<imgGeometry.sides; si++) {
        for(tr=0; tr<imgGeometry.tracks; tr++) {
            for(sect=0; sect<imgGeometry.sectors; sect++) {
                checksums[tr][si][sect] = 0;                                       // set all reference checksum data to 0
            }
        }
    }

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

        memset(bfr, initval++, 512);    //just to make sure we are not looking at data from the last read attempt on a failure

        WORD tics = getTicks();
        writeBfr[510] = (BYTE) (tics >> 8);     // last two bytes are 'random' to keep the content changing
        writeBfr[511] = (BYTE) (tics     );

        // write
        BYTE wRes = floppy_write(writeBfr, 0, fl.sector, fl.track, fl.side, 1);

        // read
        BYTE rRes = floppy_read(bfr, 0, fl.sector, fl.track, fl.side, 1);

        int verifyOk = 1;

        if(wRes || rRes) {                  // if write or read failed, not ok
            verifyOk = 0;
        } else {                            // if write and read succeeded, time to verify data
            int i;
            for(i=0; i<512; i++){           // verify
                if(bfr[i] != writeBfr[i])
                {
                    verifyOk = 0;
                    break;
                }
            }
        }

        //---------------------------------        // evaluate the result
        counts.runs++;                      // increment the count of runs

        VT52_Goto_pos(x, 24);

        BYTE showDebugInfo = 0;
        BYTE resultChar;

        if(verifyOk){
            Cconout('*');
            resultChar = '*';
            counts.good++;
        } else {
            Cconout('!');
            resultChar = '!';
            showDebugInfo   = 1;
            counts.errData++;
        }

        if(showDebugInfo && testConf.stopAfterError) {
            BYTE quit = showDebugInfoFunc(resultChar, 0, 0, 0);
            isAfterStartOrError = 1;

            if(quit) {
                break;
            }
        }
        prevTrack = fl.track;

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

void readTest(BYTE linearNotRandom, BYTE imageTestNotAny, BYTE foreverNotOnce)
{
    BYTE bfr[512];                                                                   // buffer for one sector
    BYTE initval=0;                                                                  // not sure yet

    counts.runs     = 0;                                                             // keep track of number of runs
    counts.good     = 0;                                                            // number of runs passed
    counts.lazy     = 0;                                                            // ?
    counts.errRead  = 0;                                                            // number of read error
    counts.errData  = 0;                                                            // number of data incorrectly read

    times.min   = 32000;                                                            // min time
    times.max   = 0;                                                                // max time
    times.avg   = 0;                                                                // avg time

    int sect,tr,si;
    for(si=0; si<imgGeometry.sides; si++) {
        for(tr=0; tr<imgGeometry.tracks; tr++) {
            for(sect=0; sect<imgGeometry.sectors; sect++) {
                checksums[tr][si][sect] = 0;                                       // set all reference checksum data to 0
            }
        }
    }

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

        memset(bfr, initval++, 512);    //just to make sure we are not looking at data from the last read attempt on a failure

        DWORD start, end;
        start       = getTicks();
        BYTE bRes   = readSector(fl.sector, fl.track, fl.side, imageTestNotAny);
        end         = getTicks();

        DWORD ms = (end - start) * 5;

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
        DWORD lazyTime = (howManyTracksSeeked * seekRateMs) + 400;  // calculate how many time will be considered as lazy seek - if it's more than SEEK TO TRACK time + 2 floppy spin times, it's too lazy

        //---------------------------------
        // evaluate the result
        counts.runs++;                      // increment the count of runs

        VT52_Goto_pos(x, 24);

        BYTE showDebugInfo = 0;
        BYTE resultChar;

        if(bRes == 0) {                     // read and DATA good
            if(ms > lazyTime && !isAfterStartOrError) { // operation was taking too much time? Too lazy! (but only if it's not after error or start, that way lazy is expected)
                counts.lazy++;
                Cconout('L');

                resultChar      = 'L';
                showDebugInfo   = 1;
            } else {                        // operation was fast enough
                counts.good++;
                Cconout('*');

                resultChar      = '*';
                showDebugInfo   = 0;
            }

            isAfterStartOrError = 0;        // mark that error didn't happen, so next lazy read is really lazy read
        } else if(bRes == 1) {              // operation failed - Floprd failed
            counts.errRead++;
            Cconout('!');

            resultChar      = '!';
            showDebugInfo   = 1;

            isAfterStartOrError = 1;        // mark that error happened, next read might be lazy and it will be OK (floppy needs some time to get back to normal)
        } else if(bRes == 2) {              // operation failed - data mismatch
            counts.errData++;
            Cconout('D');

            resultChar      = 'D';
            showDebugInfo   = 1;

            isAfterStartOrError = 0;        // mark that error didn't happen, so next lazy read is really lazy read
        }

        if(showDebugInfo && testConf.stopAfterError) {
            BYTE quit = showDebugInfoFunc(resultChar, ms, howManyTracksSeeked, lazyTime);
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

            (void) Cconws("Finished.");

            (void) Cconws("\r\n");
            WORD cs = getWholeCheckSum(linearNotRandom);
            (void)Cconws("\r\nImage checksum: ");
            showHexWord(cs);
            (void)Cconws("\r\n");

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

BYTE showDebugInfoFunc(BYTE resultChar, int ms, int howManyTracksSeeked, int lazyTime)
{
    VT52_Goto_pos(0, 5);

    (void) Cconws("Sector    : ");
    showInt(fl.track, 2);
    (void) Cconws(",");
    showInt(fl.sector, 2);
    (void) Cconws(",");
    showInt(fl.side, 1);

    (void) Cconws("  \r\nOp result :    ");
    Cconout(resultChar);
    (void) Cconws("  \r\nFDC status:   ");
    showHexByte(fdcStatus);
    (void) Cconws("  \r\nOp time   : ");
    showInt(ms, 4);
    (void) Cconws("  \r\nSeek count:   ");
    showInt(howManyTracksSeeked, 2);
    (void) Cconws("  \r\nLazy thres: ");
    showInt(lazyTime, 4);
    (void) Cconws("  \r\n");

    char req = Cnecin();

    if(req  >= 'A' && req <= 'Z') {
        req = req + 32;     // upper to lower case
    }

    VT52_Goto_pos(0, 5);
    int i;
    for(i=0; i<6; i++) {
        (void) Cconws("                      \r\n");
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
    //-----------------
    // for random order
    if(!linearNotRandom) {
        fl.sector  = (Random() % imgGeometry.sectors) + 1;
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
    if(fl.sector < imgGeometry.sectors) {   // not last sector? next sector
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

BYTE readSector(int sector, int track, int side, BYTE checkData)    // 0 means good, 1 means failed READ operation, 2 means READ good but DATA bad
{
    BYTE bfr[512];
    int res, i;

    res = floppy_read(bfr, 0, sector, track, side, 1);

    if(res != 0) {                  // failed?
        return 1;                   // 1 means READ operation failed
    }

    calcCheckSum(bfr, sector, track, side);

    if(!checkData) {                // shouldn't check data? quit with success
        return 0;
    }
                                    // track / side / sector bytes in data don't match? Return BAD DATA.
    if(bfr[0] != track || bfr[1] != side || bfr[2] != sector) {
        return 2;
    }

    for(i=3; i<512; i++) {
        if(bfr[i] != ((BYTE) i)) {  // data mismatch? return DATA BAD
            return 2;
        }
    }

    return 0;                       // if came here, everything is OK
}

void calcCheckSum(BYTE *bfr, int sector, int track, int side)
{
    if(track < 0 || track > 85) {
        return;
    }

    if(sector < 1 || sector > 15) {
        return;
    }

    if(side < 0 || side > 1) {
        return;
    }

    BYTE cs = 0;
    int i;

    for(i=0; i<512; i++) {
        cs += bfr[i];                           // add this value to checksum
    }

    checksums[track][side][sector - 1] = cs;    // store the whole checksum
}

WORD getWholeCheckSum(BYTE linearNotRandom)
{
    WORD cs = 0;
    int rest = 0;

    if(!linearNotRandom) {
        (void)Cconws("\r\nAfter random test will read");
        (void)Cconws("\r\nall the missed sectors:\r\n");
    }

    int sect,tr,si;
    for(si=0; si<imgGeometry.sides; si++) {
        for(tr=0; tr<imgGeometry.tracks; tr++) {
            for(sect=0; sect<imgGeometry.sectors; sect++) {
                if(!linearNotRandom) {              // if random test, read the missing sectors
                    if(checksums[tr][si][sect] == 0) {  // no checksum for this sector? read and calculate it now
                        readSector(sect + 1, tr, si, 0);
                        Cconout('.');

                        rest++;
                        if(rest >= 40) {                // if would go out of screen, add new line
                            (void) Cconws("\r\n");
                            rest = 0;
                        }
                    }
                }

                cs += (WORD) checksums[tr][si][sect];   // add to the whole checksum
            }
        }
    }

    return cs;
}

BYTE writeSector(int sector, int track, int side){
    // customize write data
    writeBfr[0] = track;
    writeBfr[1] = side;
    writeBfr[2] = sector;

    // issue write command
    int result;
    result = Flopwr(writeBfr, 0, 0, sector, track, side, 1);
    return result;
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

    (void) Cconws("\33p[ CosmosEx FDD test.     Writes: ");
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
    int sector, track, side;

    VT52_Clear_home();
    (void)Cconws("Writing TEST floppy...\r\n");

    for(track=0; track<80; track++) {
        (void)Cconws("\r\nTrack ");
        showInt(track, 2);
        (void)Cconws(": ");

        for(side=0; side<2; side++) {
            for(sector=1; sector<=10; sector++) {
                writeSector(sector, track, side);
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

void guessImageGeometry(void)
{
    BYTE res;

    int spt, tracks;

    VT52_Clear_home();
    (void) Cconws("Estimating floppy geometry");

    imgGeometry.sectors = 9;
    for(spt=9; spt<15; spt++) {
        res = readSector(spt, 1, 0, 0);

        if(res != 0) {                          // if failed to read this sector, than it has this many sectors per track
            imgGeometry.sectors = spt - 1;
            break;
        }

        Cconout('.');
    }

    imgGeometry.tracks = 80;
    for(tracks=78; tracks<85; tracks++) {
        res = readSector(1, tracks, 0, 0);

        if(res != 0) {                          // if failed to read this track, than it has this many tracks
            imgGeometry.tracks = tracks;
            break;
        }
        Cconout('.');
    }

    imgGeometry.sides = 2;

    // show the geometry to user
    (void) Cconws("\r\nEstimated geometry: ");
    showInt(imgGeometry.tracks, 2);
    (void) Cconws(",");
    int decimals = (imgGeometry.sectors < 10) ? 1 : 2;
    showInt(imgGeometry.sectors, decimals);
    (void) Cconws(",");
    showInt(imgGeometry.sides, 1);
    (void) Cconws("\r\n");

    imgGeometry.totalSectors = imgGeometry.sectors * imgGeometry.tracks * imgGeometry.sides;        // calculate how many sectors there are on this floppy

    Cnecin();
}

// given set implementation (TOS/asm), will:
//  1). seek to the given sector,track,side; and return if error
//  2). given no error, write bfr (512 bytes) to set sector, return result code
int floppy_write(BYTE *wrbuf, WORD dev, WORD sector, WORD track, WORD side, WORD count)
{
    int result;

    if(testConf.fddViaTos){
        // we do it via tos shit
        result = Flopwr(wrbuf, 0, dev, sector, track, side, count);
    }
    else{
        // 1). seek
        argFuncId       = FDC_FUN_SEEK; // seek to the right track
        argDrive        = dev;
        argTrack        = track;
        argSide         = side;
        argSector       = sector;
        argLeaveOn      = 1;
        argBufferPtr    = (DWORD) wrbuf;
        runFdcAsm();                            // do the requested action
        if(argSuccess != 0) {                   // failed?
            result = argSuccess;                // return that error
        }
        else{
            // 2). write
            argFuncId       = FDC_FUN_WRITE;    // now write the sector
            runFdcAsm();                        // do the requested action
            result = argSuccess;                // return success / failure
        }
    }
    return result;
}

int floppy_read(BYTE *buf, WORD dev, WORD sector, WORD track, WORD side, WORD count)
{
    int res;

    if(testConf.fddViaTos) {
        res = Floprd(buf, 0, dev, sector, track, side, count);
    } else {
        //-------------
        // first seek
        argFuncId       = FDC_FUN_SEEK; // seek to the right track
        argDrive        = dev;
        argTrack        = track;
        argSide         = side;
        argSector       = sector;
        argLeaveOn      = 1;
        argBufferPtr    = (DWORD) buf;

        runFdcAsm();                    // do the requested action

        if(argSuccess != 0) {           // failed?
            return argSuccess;          // return that error
        }
        //-------------
        // then read
        argFuncId       = FDC_FUN_READ; // now read the sector
        runFdcAsm();                    // do the requested action
        res = argSuccess;               // return success / failure
    }

    return res;
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
        argLeaveOn      = 0;                // turn MOTOR OFF
        argBufferPtr    = (DWORD) buf;

        runFdcAsm();                        // do the requested action
    }
}
