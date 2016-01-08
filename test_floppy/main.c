#include <osbind.h>

#include "vt52.h"
#include "stdlib.h"

int  getIntFromStr (const char *str, int len);
void showAppVersion(void);

void showMenu(void);

void makeFloppy(void);

void showLineStart(BYTE sequentialNotRandom, int trNo);
void updateFloppyPosition(BYTE sequentialNotRandom, BYTE endlessNotOnce);

void showInt(int value, int length);
BYTE readSector(int sector, int track, int side, BYTE checkData);       // 0 means good, 1 means failed READ operation, 2 means READ good but DATA bad
void writeSector(int sector, int track, int side);

void deleteOneProgressLine(void);
void calcCheckSum(BYTE *bfr, int sector, int track, int side);
WORD getWholeCheckSum(void);
void showHexByte(int val);
void showHexWord(WORD val);

void showDecimal(int num);
void showDiff(BYTE* bfr, int track, int side, int sector, char*range);

DWORD getTicks(void);

void print_status(void);

void setImageGeometry(int tracks, int sides, int sectors);
void guessImageGeometry(void);

BYTE writeBfr[512];

struct {
    DWORD runs;
    
    DWORD good;
    DWORD lazy;
    
    DWORD errRead;
    DWORD errData;
} counts;

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

void readTest(BYTE sequentialNotRandom, BYTE imageTestNotAny, BYTE endlessNotOnce);

int main(void)
{
    int i;
    for(i=0; i<512; i++) {
        writeBfr[i] = (BYTE) i;
    }
	
	while(1) {
        showMenu();
		char req = Cnecin();
        
        if(req  >= 'A' && req <= 'Z') {
            req = req + 32;     // upper to lower case
        }
        
		if(req == 'q') {        // quit?
			return 0;
		}
        
        if(req == 'w') {        // write test floppy image
            makeFloppy();
        }

        //---------------------
        if(req == 'r') {        // random     on TEST image ONCE
            setImageGeometry(80, 2, 9);
            readTest(0, 1, 0);
        }

        if(req == 'n') {        // random     on TEST image ENDLESS
            setImageGeometry(80, 2, 9);
            readTest(0, 1, 1);
        }
        
        if(req == 's') {        // sequential on TEST image ONCE
            setImageGeometry(80, 2, 9);
            readTest(1, 1, 0);
        }
        //---------------------
        
        if(req == 'a') {        // random     on ANY image ONCE
            guessImageGeometry();
            readTest(0, 0, 0);
        }

        if(req == 'd') {        // random     on ANY image ENDLESS
            guessImageGeometry();
            readTest(0, 0, 1);
        }
        
        if(req == 'e') {        // sequential on ANY image ONCE
            guessImageGeometry();
            readTest(1, 0, 0);
        }
	}
}

void readTest(BYTE sequentialNotRandom, BYTE imageTestNotAny, BYTE endlessNotOnce)
{
  	BYTE bfr[512];
    BYTE initval=0;
    
    counts.runs     = 0;
    counts.good     = 0;
    counts.lazy     = 0;
    counts.errRead  = 0;
    counts.errData  = 0;

    int sect,tr,si;
    for(si=0; si<imgGeometry.sides; si++) {
        for(tr=0; tr<imgGeometry.tracks; tr++) {
            for(sect=0; sect<imgGeometry.sectors; sect++) {
                checksums[tr][si][sect] = 0;
            }
        }
    }
    
    VT52_Clear_home();
    print_status();
    
    fl.finish   = 0;                    // don't finish yet
    fl.newLine  = 0;
    fl.space    = 0;
    
    fl.side     = 0;
    fl.track    = 0;
    fl.sector   = 1;
    
    fl.randCount = imgGeometry.totalSectors - 1;
    
    VT52_Goto_pos(0, 24);
    int x = 11;
    
    showLineStart(sequentialNotRandom, fl.track);
    
    while(1) {
        //---------------------------------
        // code for termination of test by keyboard
        BYTE res = Cconis();        // see if there's something waiting from keyboard 
		
		if(res != 0) {              // something waiting?
            char req = Cnecin();
            
            if(req  >= 'A' && req <= 'Z') {
                req = req + 32;     // upper to lower case
            }
            
            if(req == 'q' || req == 'c') {  // quit?
                return;
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
        // evaluate the result
        counts.runs++;          // increment the count of runs
        
        VT52_Goto_pos(x, 24);
        
        if(bRes == 0) {         // read and DATA good
            if(ms > 500) {      // operation was taking too much time? Too lazy!
                counts.lazy++;
                Cconout('L');
            } else {            // operation was fast enough
                counts.good++;
                Cconout('*');
            }
        } else if(bRes == 1) {  // operation failed - Floprd failed
            counts.errRead++;
            Cconout('!');
        } else if(bRes == 2) {  // operation failed - data mismatch
            counts.errData++;
            Cconout('D');
        }

        print_status();
        
        //---------------------------------
        // update the next floppy position for reading
        updateFloppyPosition(sequentialNotRandom, endlessNotOnce);
        
        if(fl.finish) {                         // if should quit after this
            deleteOneProgressLine();

            (void) Cconws("Finished.");
            
            if(sequentialNotRandom) {
                WORD cs = getWholeCheckSum();
                (void)Cconws("\r\nImage checksum: ");
                showHexWord(cs);
                (void)Cconws("\r\n");
            }
            
            print_status();
            Cnecin();
            return;
        }
        
        if(fl.newLine) {                        // should start new line?
            fl.newLine = 0;
        
            deleteOneProgressLine();
            x = 11;
            
            showLineStart(sequentialNotRandom, fl.track);
        } else {                                // just go to next char
            x++;
        }
        
        if(fl.space) {
            fl.space = 0;
            x++;
        }
    }
}

void deleteOneProgressLine(void)
{
    VT52_Goto_pos(0,4);
    VT52_Del_line();
    VT52_Goto_pos(0, 24);
}

void showLineStart(BYTE sequentialNotRandom, int trNo) 
{
    if(sequentialNotRandom) {
        (void) Cconws("TRACK ");
        showInt(trNo, 2);
        (void) Cconws(": ");
    } else {
        (void) Cconws("Rand read: ");
    }
}

void updateFloppyPosition(BYTE sequentialNotRandom, BYTE endlessNotOnce)
{
    //-----------------
    // for random order
    if(!sequentialNotRandom) {      
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

            if(!endlessNotOnce) {   // if once, quit
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
                
                if(!endlessNotOnce) {   // if once, quit
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

	res = Floprd(bfr, 0, 0, sector, track, side, 1);
				
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

WORD getWholeCheckSum(void)
{
    WORD cs = 0;
    
    int sect,tr,si;
    for(si=0; si<imgGeometry.sides; si++) {
        for(tr=0; tr<imgGeometry.tracks; tr++) {
            for(sect=0; sect<imgGeometry.sectors; sect++) {
                cs += (WORD) checksums[tr][si][sect];
            }
        }
    }

    return cs;
}

void writeSector(int sector, int track, int side)
{
    // customize write data
    writeBfr[0] = track;
    writeBfr[1] = side;
    writeBfr[2] = sector;
    
    // issue write command
	(void) Flopwr(writeBfr, 0, 0, sector, track, side, 1);
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

void print_status(void)
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

void showMenu(void)
{
    VT52_Clear_home();
    
	(void)Cconws("\33p[ Floppy test tool      ver ");
    showAppVersion();
	(void)Cconws(" ]\33q\r\n");

    (void)Cconws("\r\n");
    (void)Cconws("Tests for \33pTEST\33q floppy image:\r\n");
	(void)Cconws(" \33p[ R ]\33q - random     read test ONCE\r\n");
	(void)Cconws(" \33p[ N ]\33q - random     read test ENDLESS\r\n");
	(void)Cconws(" \33p[ S ]\33q - sequential read test ONCE\r\n");
    (void)Cconws("\r\n");
    (void)Cconws("Tests for \33pANY\33q floppy image:\r\n");
	(void)Cconws(" \33p[ A ]\33q - random     read test ONCE\r\n");
	(void)Cconws(" \33p[ D ]\33q - random     read test ENDLESS\r\n");
	(void)Cconws(" \33p[ E ]\33q - sequential read test ONCE\r\n");
    (void)Cconws("\r\n");
	(void)Cconws(" \33p[ W ]\33q - write TEST floppy image\r\n");
	(void)Cconws(" \33p[ Q ]\33q - quit this app\r\n");
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
    
    for(spt=9; spt<15; spt++) {
        res = readSector(spt, 0, 0, 0);
        
        if(res != 0) {                          // if failed to read this sector, than it has this many sectors per track
            imgGeometry.sectors = spt - 1;
            break;
        }
        
        Cconout('.');
    }
    
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

