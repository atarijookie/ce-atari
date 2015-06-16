#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdio.h>
#include "stdlib.h"
#include "out.h"

extern int drive;

void test040x(void);
void test041x(void);
void test043x(void);
void test044x(void);
void test045x(void);
void test046x(void);

BYTE filenameExists(char *filename);

void deleteRecursive(char *subPath);
void deleteIfExists(char *fname);

WORD testDcreate(char *fname);

void testSingleValidChar(WORD testCaseNo, char *fname);
void testSingleInvalidChar(WORD testCaseNo, char *fname, WORD goodErrorCode);

void test04(void)
{
    out_s("Dcreate, Ddelete, Frename, Fdelete");
    
    Dsetdrv(drive);
    (void) Dcreate("\\TEST04");
    Dsetpath("\\TEST04");

    test040x();
    test041x();
    test043x();
    test044x();
    test045x();
    test046x();
    
    Ddelete("\\TEST04");
    out_s("");
}

void test040x(void)
{
    BYTE ok;
    WORD res;

    res = testDcreate("TESTDIR");
    (res == 0) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0401, "Dcreate - just filename", ok, res);

    res = testDcreate("TEST.D");
    (res == 0) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0402, "Dcreate - filename & extension", ok, res);
    
    res = testDcreate("TESTTEST.DIR");
    (res == 0) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0403, "Dcreate - full filename & full extension", ok, res);
    
    res = testDcreate(".DIR");
    (res == 0) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0404, "Dcreate - no filename, just extension", ok, res);

    (void) Dcreate("TESTDIR");
    res = Dcreate("TESTDIR");
    (res == 0xffdc) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0405, "Dcreate - already existing dir", ok, res);
}

void test041x(void)
{
    testSingleValidChar(0x0410, "A");
    testSingleValidChar(0x0411, "a");
    testSingleValidChar(0x0412, "0");
    testSingleValidChar(0x0413, "!");
    testSingleValidChar(0x0414, "#");
    testSingleValidChar(0x0415, "$");
    testSingleValidChar(0x0416, "%");
    testSingleValidChar(0x0417, "&");
    testSingleValidChar(0x0418, "'");
    testSingleValidChar(0x0419, "(");
    testSingleValidChar(0x0420, ")");
    testSingleValidChar(0x0421, "-");
    testSingleValidChar(0x0422, "@");
    testSingleValidChar(0x0423, "^");
    testSingleValidChar(0x0424, "_");
    testSingleValidChar(0x0425, "{");
    testSingleValidChar(0x0426, "}");
    testSingleValidChar(0x0427, "~");
}

void test043x(void)
{
    testSingleInvalidChar(0x0431, "*", 0xffdc);
    testSingleInvalidChar(0x0432, "?", 0xffdc);
    testSingleInvalidChar(0x0433, "\\", 0xffde);        // steem returns 0xffdc (access denied), native drive returns 0xffde (folder not found)
}

void test044x(void)
{
    WORD res1, res2, ok;

    (void) Dcreate("TOBEDELE");
    res1 = Ddelete("TOBEDELE");
    res2 = filenameExists("TOBEDELE");
    (res1 == 0 && res2 == 0) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0441, "Ddelete - delete empty dir", ok, res1);

    (void) Dcreate("DELETEME");
    int f = Fcreate("DELETEME\\SUBFILE", 0);
    Fclose(f);
    res1 = Ddelete("DELETEME");
    res2 = filenameExists("DELETEME");
    (res1 == 0xffdc && res2 == 1) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0442, "Ddelete - delete non-empty dir", ok, res1);
    
    res1 = Ddelete("HOGOFOGO");
    (res1 == 0xffde || res1 == 0xffdc) ? (ok = 1) : (ok = 0);           // native drive returns 0xffde, steem returns 0xffdc
    out_tr_bw(0x0443, "Ddelete - delete non-existing dir", ok, res1);

    res1 = Ddelete("DELETEME\\SUBFILE");
    (res1 == 0xffde || res1 == 0xffdc) ? (ok = 1) : (ok = 0);           // native drive returns 0xffde, steem returns 0xffdc
    out_tr_bw(0x0444, "Ddelete - delete a file", ok, res1);

    Fdelete("DELETEME\\SUBFILE");
    Ddelete("DELETEME");
}

void test045x(void)
{
    WORD res1, res2, res3, res4, ok;

    Fdelete("MYFILE");
    Fdelete("NEWFILE");
    Fdelete("NOTEXIST");
    Fdelete("DERP*");
    
    int f = Fcreate("MYFILE", 0);
    Fclose(f);
    
    res1 = filenameExists("MYFILE");
    res2 = Frename(0, "MYFILE", "NEWFILE");
    res3 = filenameExists("MYFILE");
    res4 = filenameExists("NEWFILE");
    ok = 0;
    if(res1 == 1 && res2 == 0 && res3 == 0 && res4 == 1) {
        ok = 1;
    }
    out_tr_bw(0x0451, "Frename - rename a existing file", ok, res2);

    res1 = Frename(0, "NOTEXIST", "NEWFILE");
    (res1 == 0xffdf) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0452, "Frename - rename a non-existing file", ok, res1);
    
    res1 = Frename(0, "NEWFILE", "DERP*");
    (res1 == 0 || res1 == 0xffdc) ? (ok = 1) : (ok = 0);           // TOS bug? Native drive returns 0, steem returns 0xffdc
    out_tr_bw(0x0453, "Frename - rename to invalid file", ok, res1);
    
    Ddelete("MYDIR");
    Ddelete("NEWDIR");
    (void) Dcreate("MYDIR");
    res1 = Frename(0, "MYDIR", "NEWDIR");
    res2 = filenameExists("MYDIR");
    res3 = filenameExists("NEWDIR");
    (res1 == 0 && res2 == 0 && res3 == 1) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0454, "Frename - rename a directory", ok, res1);

    Ddelete("MYDIR");
    Ddelete("NEWDIR");
    Fdelete("MYFILE");
    Fdelete("NEWFILE");
    Fdelete("NOTEXIST");
    Fdelete("DERP*");
}

void test046x(void)
{
    WORD res1, res2, res3, ok;

    Fdelete("MYFILE");
    Fdelete("NEWFILE");
    Fdelete("NOTEXIST");
    Fdelete("DERP*");

    int f = Fcreate("MYFILE", 0);
    Fclose(f);
    res1 = filenameExists("MYFILE");
    res2 = Fdelete("MYFILE");
    res3 = filenameExists("MYFILE");
    (res1 == 1 && res2 == 0 && res3 == 0) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0461, "Fdelete - delete existing file", ok, res1);
    
    res1 = filenameExists("NOTEXIST");
    res2 = Fdelete("NOTEXIST");
    (res1 == 0 && res2 == 0xffdf) ? (ok = 1) : (ok = 0);
    out_tr_bw(0x0462, "Fdelete - delete non-existing file", ok, res2);
    
    (void) Dcreate("MYDIR");    
    res1 = filenameExists("MYDIR");
    res2 = Fdelete("MYDIR");
    (res1 == 1 && (res2 == 0xffdf || res2 == 0xffdc)) ? (ok = 1) : (ok = 0);        // native drive returns 0xffdf, Steem drive returns 0xffdc
    out_tr_bw(0x0463, "Fdelete - delete a directory", ok, res2);
    
    Ddelete("MYDIR");
}
    
void testSingleValidChar(WORD testCaseNo, char *fname)
{
    WORD res, ok;
    
    res = testDcreate(fname);
    (res == 0) ? (ok = 1) : (ok = 0);

    char tmp[64] = {"Dcreate - single valid char: 'A'"};
    tmp[30] = fname[0];
    out_tr_bw(testCaseNo, tmp, ok, res);
}

void testSingleInvalidChar(WORD testCaseNo, char *fname, WORD goodErrorCode)
{
    WORD res, ok;
    res = Dcreate(fname);
    
    (res == goodErrorCode) ? (ok = 1) : (ok = 0);
    
    char tmp[64] = {"Dcreate - single invalid char: 'A'"};
    tmp[32] = fname[0];
    out_tr_bw(testCaseNo, tmp, ok, res);
}

WORD testDcreate(char *fname)
{
    deleteIfExists(fname);          // if the item exists, delete it
    if(filenameExists(fname)) {     // if delete failed, report it as a failure
        return 0xff01;
    }
    
    int res = Dcreate(fname);       // try to create dir
    
    if(res != 0) {                  // if failed to create, return its error code
        return (WORD) res;
    }

    if(!filenameExists(fname)) {    // check if the dir really exists, and if not...
        return 0xff02;
    }
    
    deleteIfExists(fname);          // delete this test dir

    if(filenameExists(fname)) {     // if delete failed, report it as a failure
        return 0xff03;
    }

    return 0;
}

void deleteIfExists(char *fname)
{
    if(filenameExists(fname)) {
        Ddelete(fname);
    }
}

BYTE filenameExists(char *filename)
{
    char dta[44];
    memset(dta, 0, 44);
    
    Fsetdta(dta);
    int res = Fsfirst(filename, 0x3f);

    if(res) {               // doesn't exist
        return 0;
    }
    
    while(1) {
        if(strcmpi(filename, &dta[30]) == 0) {       // filename matches?
            return 1;
        }
    
        res = Fsnext();
        if(res) {           // next doesn't exist?
            break;
        }
    }
    
    return 0;
}
