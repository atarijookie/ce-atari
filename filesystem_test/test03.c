#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdio.h>
#include "stdlib.h"
#include "out.h"

extern int drive;

void createTestFiles(void);
void deleteRecursive(char *subPath);
int  countFoundItems(char *dta, char *fspec, WORD attribs, BYTE startNotCont, int stopAfter);
BYTE getRandom(void);

void test0301(void);
void test0310(void);

void test03(void)
{
    out_s("Fsfirst, Fsnext, Fsetdta, Fgetdta");
    createTestFiles();
    
    test0301();
    test0310();
    
    
    deleteRecursive("\\TESTDIR");
    out_s("");
}

#define NESTING_LEVEL   16

void test0310(void)
{
    DWORD start = getTicks();

    char dta[NESTING_LEVEL * 44];
    int  cnt[NESTING_LEVEL], i, total;

    memset(dta, 0, 44);
    total = countFoundItems(dta, "*.*", 0x3f, TRUE, -1);
    
    memset(dta, 0, NESTING_LEVEL * 44);
    
    for(i=0; i<NESTING_LEVEL; i++) {
        cnt[i] = 0;
    }
    
    BYTE allDone = 0;
    while(1) {
        //-------------------
        // protection against endless loop in case of failing test
        if((getTicks() - start) > 1000) {       // more than 5 seconds of run? quit, fail
            break;
        }
        //-------------------
        // first check if all searches are done
        allDone = 1;
        
        for(i=0; i<NESTING_LEVEL; i++) {    // go through searches
            if(cnt[i] < total) {            // if found a search which isn't finished yet, mark that we're not done
                allDone = 0;
                break;
            }
        }
        if(allDone) {                       // if we're all done, quit this loop
            break;
        }
        
        //-------------------
        // do a search
        while(1) {
            i = getRandom();            // get index at which we should do the search
        
            if(cnt[i] < total) {        // if found a not finished search, use it
                break;
            }
        }
        
        int stopAfter = getRandom();    // this is a count of items after which we should stop this search and continue with another

        if(stopAfter == 0) {
            stopAfter++;
        }
        
        BYTE startNotContinue = 0;      // flag to mark start or continuation of search
        if(cnt[i] == 0) {               // if don't have anything yet, it's a start (otherwise continue search)
            startNotContinue = 1;
        }
        
        cnt[i] += countFoundItems(dta + (i * 44), "*.*", 0x3f, startNotContinue, stopAfter);
    }
    
    int sum = 0;
    for(i=0; i<NESTING_LEVEL; i++) {
        sum += cnt[i];
    }
    
    out_tr_bw(0x0310, "Fsfirst & Fsnext - nested searching", allDone, sum);
}
    
void test0301(void)
{
    char dta[44];
    int cnt, res;
    BYTE ok;
    
    cnt = countFoundItems(dta, "*.*", 0x3f, TRUE,  -1);
    (cnt == 77) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0301, "Fsfirst & Fsnext - match everything", ok, cnt);

    cnt = countFoundItems(dta, "*.*", 0x10, TRUE,  -1);
    (cnt == 27) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0302, "Fsfirst & Fsnext - match all dirs", ok, cnt);

    cnt = countFoundItems(dta, "*.*", 0x00, TRUE,  -1);
    (cnt == 50) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0303, "Fsfirst & Fsnext - match all files", ok, cnt);

    cnt = countFoundItems(dta, "FILE_1*.*", 0x3f, TRUE,  -1);
    (cnt == 10) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0304, "Fsfirst & Fsnext - wildcard * in fname", ok, cnt);

    cnt = countFoundItems(dta, "FILE_?5.*", 0x3f, TRUE,  -1);
    (cnt == 2) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0305, "Fsfirst & Fsnext - wildcard ? in fname", ok, cnt);

    cnt = countFoundItems(dta, "FILENAME.*2", 0x3f, TRUE,  -1);
    (cnt == 2) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0306, "Fsfirst & Fsnext - wildcard * in ext", ok, cnt);

    cnt = countFoundItems(dta, "FILENAME.X0?", 0x3f, TRUE,  -1);
    (cnt == 15) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0307, "Fsfirst & Fsnext - wildcard ? in ext", ok, cnt);
    
    cnt = countFoundItems(dta, "FILE_05", 0x00, TRUE,  -1);
    (cnt == 1) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0308, "Fsfirst & Fsnext - exact file", ok, cnt);
    
    Fsetdta(dta);                       // set DTA 
    res = Fsfirst("DOESNT.EXS", 0x00);
    (res == -33) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0309, "Fsfirst & Fsnext - non-existing file", ok, res);
}

int countFoundItems(char *dta, char *fspec, WORD attribs, BYTE startNotCont, int stopAfter)
{
    int res, found = 0;
    
    if(startNotCont) {                  // on start clean DTA
        memset(dta, 0, 44);
    }
    
    Fsetdta(dta);                       // set DTA 

    if(startNotCont) {                  // if should start search, not continue
        res = Fsfirst(fspec, attribs);
        
        if(res) {                       // file not found?
            return 0;
        }
        
        found++;
    }
    
    while(1) {
        if(found == stopAfter) {        // if we found all we needed, stop
            return found;
        }
        
        res = Fsnext();
        
        if(res) {                       // if file not found, quit
            return found;
        }
        
        found++;
    }
    
    return found;                       // this should never happen
}

void createTestFiles(void)
{
    Dsetdrv(drive);             // switch to selected drive
    Dsetpath("\\");
    (void) Dcreate("TESTDIR");
    Dsetpath("\\TESTDIR");

    int i, f;
    char numString[8];
    char fileName[16];
    for(i=0; i<25; i++) {
        byteToHex(i + 1, numString);
        
        strcpy(fileName, "FILE_");
        strcat(fileName, numString);
        f = Fcreate(fileName, 0);
        Fwrite(f, 7, fileName);
        Fclose(f);
        
        strcpy(fileName, "FILENAME.X");
        strcat(fileName, numString);
        f = Fcreate(fileName, 0);
        Fwrite(f, 12, fileName);
        Fclose(f);
        
        strcpy(fileName, "DIR_");
        strcat(fileName, numString);
        (void) Dcreate(fileName);
    }
}

BYTE getRandom(void)            // random from 0 to 7
{
    DWORD r = getTicks();
    r = r & 0x0f;
    return r;
}
