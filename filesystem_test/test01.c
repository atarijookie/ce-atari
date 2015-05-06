#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdio.h>
#include "stdlib.h"
#include "out.h"

extern int drive;

void test0101(void);
void test010203(void);
void test0104(void);

void test01(void)
{
    test0101();                 // switch to selected drive
    test010203();               // switch to existing and non-existing drive
    test0104();                 // switch to drive which is out of range (> 15)

    Dsetdrv(drive);             // back to selected drive
}

void test0101(void)
{
    WORD res;
    BYTE ok;
    
    WORD drives = Drvmap();

    res = Dsetdrv(drive);
    ok  = drives == res;        // OK if returning bitmap of drives
    
    if(ok) {
        WORD drv = Dgetdrv();
        ok = drive == drv;      // OK if drive changed
    }
    
    out_tr_b(0x0101, "Dsetdrv to selected drive", ok);
}

void test010203(void)
{
    WORD res;
    BYTE ok;
    
    WORD drives = Drvmap();
    WORD dr2    = drives & (~drive);        // remove tested drive
    
    int drvExisting = -1;
    int drvNonexisting = -1;
    int i;
    
    for(i=15; i>= 0; i--) {
        if((dr2 & (1 << i)) != 0) {         // drive exists?
            drvExisting = i;
        }
        
        if((dr2 & (1 << i)) == 0) {         // drive doesn't exist?
            if(i == drive) {                // if it's not in dr2, but it's our drive, skip it
                continue;
            }
        
            drvNonexisting = i;         
        }
    }
    
    res = Dsetdrv(drvExisting);             // switch to existing drive
    ok  = drives == res;                    // OK when returns existing drives
    
    if(ok) {
        WORD drv = Dgetdrv();
        ok = drvExisting == drv;            // OK if drive changed
        if(!ok) {
            out_swsw("Was switching to ", drvExisting, ", Dgetdrv returned ", drv);
        }
    }

    out_tr_b(0x0102, "Dsetdrv to existing drive", ok);

    res = Dsetdrv(drvNonexisting);          // switch to non-existing drive
    ok  = drives == res;                    // OK when returns existing drives
    
    if(ok) {
        WORD drv = Dgetdrv();
        ok = drvNonexisting == drv;         // TOS 1.02: OK if drive change to non-existing anyway
        if(!ok) {
            out_swsw("Was switching to ", drvNonexisting, ", Dgetdrv returned ", drv);
        }
    }

    out_tr_b(0x0103, "Dsetdrv to non-existing drive", ok);
}

void test0104(void)
{
    WORD res;
    BYTE ok;

    #define DRIVE_OUT_OF_RANGE  30
    
    WORD drives = Drvmap();
    
    res = Dsetdrv(DRIVE_OUT_OF_RANGE);      // switch to out of range drive 
    ok  = drives == res;                    // OK when returns existing drives
    
    if(ok) {
        WORD drv = Dgetdrv();
        ok = DRIVE_OUT_OF_RANGE == drv;     // OK if drive changed
        if(!ok) {
            out_swsw("Was switching to ", DRIVE_OUT_OF_RANGE, ", Dgetdrv returned ", drv);
        }
    }

    out_tr_b(0x0104, "Dsetdrv to out of range", ok);
}
