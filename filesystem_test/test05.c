#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdio.h>
#include "stdlib.h"
#include "out.h"

extern int drive;

void test050x(void);

BYTE filenameExists(char *filename);
void deleteIfExists(char *fname);

void test05(void)
{
    out_s("Fcreate, Fopen, Fclose");
    
    Dsetdrv(drive);
    Dsetpath("\\");

    test050x();
    
    out_s("");
}

void test050x(void)
{
    BYTE ok;
    WORD res1, res2, res3;

    Fdelete("TESTFILE");
    res1 = filenameExists("TESTFILE");
    res2 = Fcreate("TESTFILE", 0);
    res3 = filenameExists("TESTFILE");
    (res1 == 0 && res2 > 0 && res3 == 1) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0501, "Fcreate - non-existing file", ok, res2);
    
    res1 = Fclose(res2);
    (res1 == 0) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0502, "Fclose  - valid handle", ok, res1);
    
    res1 = Fopen("TESTFILE", 0);
    (res1 > 0) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0503, "Fopen   - existing file", ok, res1);
    
    res1 = filenameExists("TESTFILE");
    res2 = Fcreate("TESTFILE", 0);
    res3 = filenameExists("TESTFILE");
    (res1 == 1 && res2 > 0 && res3 == 1) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0504, "Fcreate - existing file", ok, res2);
    (void) Fclose(res2);
    
    res1 = Fclose(res2);
    (res1 == 0xffdb) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0505, "Fclose  - invalid handle", ok, res1);        // steem drive returns 0 even though it should return some error code
    Fdelete("TESTFILE");
    
    res1 = Fopen("TESTFILE", 0);
    (res1 == 0xffdf) ? (ok = 1) : (ok = 0);                         // steem drive creates the file instead of returning error
    out_tr_bw(0x0506, "Fopen   - non-existing file", ok, res1);
    
    if(res1 > 0) {                                                  // if the file was opened (even though it shouldn't be), close it and delete it
        Fclose(res1);
        Fdelete("TESTFILE");
    }
    
    int h[256], i;
    char filename[16];
    int maxHandles = 256;
    for(i=0; i<256; i++) {
        strcpy(filename, "FILE");
        byteToHex((BYTE) i, filename + 4);
        h[i] = Fcreate(filename, 0);
        if(h[i] < 0) {
            maxHandles = i;
            break;
        }
    }
    
    (maxHandles > 20) ? (ok = 1) : (ok = 0);                        // steem drive allows 40 handles, native drive under TOS 2.06 allows 75 handles
    out_tr_bw(0x0507, "Fcreate - maximum handles", ok, maxHandles);

    for(i=0; i<=maxHandles; i++) {
        strcpy(filename, "FILE");
        byteToHex((BYTE) i, filename + 4);
        Fclose(h[i]);
        Fdelete(filename);
    }
}

