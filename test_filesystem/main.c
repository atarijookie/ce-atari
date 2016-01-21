#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdio.h>
#include "stdlib.h"
#include "out.h"

WORD getTosVersion(void);
void selectDrive(void);

int  drive;
WORD tosVersion;

void test01(void);
void test02(void);
void test03(void);
void test04(void);
void test05(WORD whichTests);

WORD fromDrive = 0;

void showMenu(void);
void removeAllWaitingKeys(void);

int main(void)
{
    removeAllWaitingKeys();     // read all the possibly waiting keys, so we can ignore them...

    fromDrive = Dgetdrv();

    out_s("\33E\33pFilesystem Test - by Jookie, 2015\33q");

    initBuffer();

    tosVersion = Supexec(getTosVersion);
    out_sw("TOS version     : ", tosVersion);
    out_sc("Running from drv: ", fromDrive + 'A');
    
    selectDrive();
    out_sc("Tested drive    : ", 'A' + drive);
    
    WORD whichTests     = 0;
    WORD which05Tests   = 0xffff;       // all 05 tests
    
    while(1) {
        showMenu();
        
   		char req = Cnecin();
        
        if(req  >= 'A' && req <= 'Z') {
            req = req + 32;                     // upper to lower case
        }
        
		if(req == 'q') {                        // quit?
			break;
		}
        
        if(req == 'a') {
            whichTests = 0xffff;                // all tests
            break;
        }
        
        if(req >= '1' && req <= '5') {          // if it's valid test choice, convert ascii number to bit
            int which = req - '1';
            whichTests = (1 << which);          // just this test
            break;
        }
        
        if(req == 'o' || req == 'r' || req == 's' || req == 'x' || req == 'w' || req == 'v') {
            whichTests = 0x10;                  // test05
            switch(req) {
                case 'o': which05Tests = 0x01; break;   // Fopen, Fcreate, Fclose
                case 'r': which05Tests = 0x02; break;   // Fread
                case 's': which05Tests = 0x04; break;   // Fseek
                case 'x': which05Tests = 0x08; break;   // Fread 10
                case 'w': which05Tests = 0x10; break;   // Fwrite
                case 'v': which05Tests = 0x20; break;   // Fwrite various
                default:  which05Tests = 0x00; break;   // nothing
            }
            break;
        }
    }
    
    out_s("");
    
    if(whichTests & 0x01)   test01();
    if(whichTests & 0x02)   test02();
    if(whichTests & 0x04)   test03();
    if(whichTests & 0x08)   test04();
    if(whichTests & 0x10)   test05(which05Tests);

    writeBufferToFile();
    deinitBuffer();
    
    out_s("Done.");
    sleep(3);
    return 0;
}

void showMenu(void)
{
    (void) Cconws("\33E");
    (void) Cconws("Select which tests to run:\r\n");
    (void) Cconws(" \33p[ 1 ]\33q Dsetdrv and Dgetdrv\r\n");
    (void) Cconws(" \33p[ 2 ]\33q Dsetpath and Dgetpath\r\n");
    (void) Cconws(" \33p[ 3 ]\33q Fsfirst, Fsnext, Fsetdta, Fgetdta\r\n");
    (void) Cconws(" \33p[ 4 ]\33q Dcreate, Ddelete, Frename, Fdelete\r\n");
    (void) Cconws(" \33p[ 5 ]\33q Fcreate, Fopen, Fclose, Fread, Fseek, Fwrite\r\n");
    (void) Cconws("       \33p[ O ]\33q Fopen, Fcreate, Fclose\r\n");
    (void) Cconws("       \33p[ R ]\33q Fread\r\n");
    (void) Cconws("       \33p[ S ]\33q Fseek\r\n");
    (void) Cconws("       \33p[ X ]\33q Fread - 10 files open and read\r\n");
    (void) Cconws("       \33p[ W ]\33q Fwrite\r\n");
    (void) Cconws("       \33p[ V ]\33q Fwrite - various cases\r\n");
    (void) Cconws(" \33p[ A ]\33q ALL TESTS\r\n");
    (void) Cconws(" \33p[ Q ]\33q QUIT\r\n");
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

WORD getTosVersion(void)
{
    BYTE  *pSysBase     = (BYTE *) 0x000004F2;
    BYTE  *ppSysBase    = (BYTE *)  ((DWORD )  *pSysBase);                      // get pointer to TOS address
    WORD  ver           = (WORD  ) *(( WORD *) (ppSysBase + 2));                // TOS +2: TOS version
    return ver;
}

void selectDrive(void)
{
    WORD drives = Drvmap();
    
    (void) Cconws("Drives available: ");
    int i;
    for(i=0; i<16; i++) {
        if(drives & (1 << i)) {
            Cconout('A' + i);
        }
    }
    (void) Cconws("\r\nSelect drive    : ");
    
    char drv = 0;
    while(1) {
        drv = Cnecin();
        if(drv >= 'A' && drv <= 'Z') {
            drv = drv - 'A';
        } else if(drv >= 'a' && drv <= 'z') {
            drv = drv - 'a';
        } else {
            continue;
        }
        
        if(drv > 15) {
            continue;
        }
        
        if(drives & (1 << drv)) {
            break;
        }
    } 

    drive = drv;
    Cconout('A' + drv);
    (void) Cconws("\r\n");
}
