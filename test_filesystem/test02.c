#include <mint/sysbind.h>
#include <mint/osbind.h>
#include <mint/ostruct.h>
#include <support.h>

#include <stdio.h>
#include "stdlib.h"
#include "out.h"

extern int drive;

void createTestDirs(int level);
void deleteTestDirs(void);
void deleteRecursive(char *subPath);

void test0210(void);

/*
    It seems that current directory '.' in the middle of Dsetpath() can be ignored, 
    because it looks like TOS is slowly changing current path during the Dsetpath() call.
*/

void test02(void)
{
    out_s("Dsetpath and Dgetpath");
    Dsetdrv(drive);                 // switch to selected drive
    
    createTestDirs(0);
    test0210();
    deleteTestDirs();
    
    out_s("");
}

void test0210(void)
{
    int a,b,c,d,e,f,g,h;
    
    //------------
    // do relative path changes down
    a = Dsetpath("\\");
    b = Dsetpath("AMSTERDA.M");
    c = Dsetpath("DUSSELDO.RF");
    d = Dsetpath("COLOGNE");
    e = Dsetpath("BERLIN");
    f = Dsetpath("AMSTERDA.M");
    g = Dsetpath("BERLIN");

    BYTE ok = 0;
    if(!a && !b && !c && !d && !e && !f && !g) {
        ok = 1;
    }
    out_tr_b (0x0201, "Dsetpath relative, doing deeper", ok);

    //------------
    // verify if the previous Dsetpath calls went OK
    char curPath[128];
    a = Dgetpath(curPath, 0);
    b = strcmp(curPath, "\\AMSTERDA.M\\DUSSELDO.RF\\COLOGNE\\BERLIN\\AMSTERDA.M\\BERLIN");
    
    ok = 0;
    if(!a && !b) {
        ok = 1;
    }
    out_tr_b (0x0202, "Dsetpath verified by Dgetpath", ok);

    //------------
    // try to go up 3x and then 3x down
    a = Dsetpath("..");
    b = Dsetpath("..");
    c = Dsetpath("..");
    d = Dsetpath("BERLIN");
    e = Dsetpath("AMSTERDA.M");
    f = Dsetpath("BERLIN");

    ok = 0;
    if(!a && !b && !c && !d && !e && !f) {
        g = Dgetpath(curPath, 0);
        h = strcmp(curPath, "\\AMSTERDA.M\\DUSSELDO.RF\\COLOGNE\\BERLIN\\AMSTERDA.M\\BERLIN");
        
        if(!g && !h) {
            ok = 1;
        } else {
            ok = 0;
        }
    }
    out_tr_b (0x0203, "Dsetpath 3x .. and then going deeper", ok);
    
    //------------
    // try to do 3x up and 3x down in single shots
    a = Dsetpath("..\\..\\..");
    b = Dgetpath(curPath, 0);
    c = strcmp(curPath, "\\AMSTERDA.M\\DUSSELDO.RF\\COLOGNE");
    
    d = Dsetpath("BERLIN\\AMSTERDA.M\\BERLIN");
    e = Dgetpath(curPath, 0);
    f = strcmp(curPath, "\\AMSTERDA.M\\DUSSELDO.RF\\COLOGNE\\BERLIN\\AMSTERDA.M\\BERLIN");
    
    ok = 0;
    if(!a && !b && !c && !d && !e && !f) {
        ok = 1;
    }
    out_tr_b (0x0204, "Dsetpath ..\\..\\.. and then going deeper", ok);

    //------------
    // set to drive root
    a = Dsetpath("\\");
    b = Dgetpath(curPath, 0);
    c = strlen(curPath);            // when Dsetpath to root, then returns empty string
    
    ok = 0;
    if(!a && !b && !c) {
        ok = 1;
    }
    out_tr_b (0x0205, "Dsetpath to drive root", ok);
    
    //------------
    // set to long deep path
    a = Dsetpath("\\AMSTERDA.M\\DUSSELDO.RF\\COLOGNE\\BERLIN\\AMSTERDA.M\\BERLIN");
    b = Dgetpath(curPath, 0);
    c = strcmp(curPath, "\\AMSTERDA.M\\DUSSELDO.RF\\COLOGNE\\BERLIN\\AMSTERDA.M\\BERLIN");

    ok = 0;
    if(!a && !b && !c) {
        ok = 1;
    }
    out_tr_b (0x0206, "Dsetpath absolute and deep", ok);
    
    //------------
    // switch to non-existing dir
    a = Dsetpath("derp");       

    ok = 0;
    if(a == -34) {
        ok = 1;
    }
    out_tr_bw(0x0207, "Dsetpath to non-existing dir", ok, a);
    
    //------------
    // set to current dir (.)
    char curPath2[128];
    
    a = Dgetpath(curPath, 0);
    b = Dsetpath(".");                  
    c = Dgetpath(curPath2, 0);
    d = strcmp(curPath, curPath2);

    ok = 0;
    if(!a && !b && !c && !d) {
        ok = 1;
    }
    out_tr_b (0x0208, "Dsetpath to current dir", ok);
    
    //-------------
    // Dsetpath - current and deeper
    a = Dsetpath("\\AMSTERDA.M\\DUSSELDO.RF\\COLOGNE");
    b = Dsetpath(".\\BERLIN\\AMSTERDA.M");
    c = Dgetpath(curPath, 0);
    d = strcmp(curPath, "\\AMSTERDA.M\\DUSSELDO.RF\\COLOGNE\\BERLIN\\AMSTERDA.M");

    ok = 0;
    if(!a && !b && !c && !d) {
        ok = 1;
    }
    out_tr_b(0x0210, "Dsetpath - deeper from current", ok);

    //-------------
    // Dsetpath - current in the middle
    Dsetpath("\\");
    a = Dsetpath("\\AMSTERDA.M\\DUSSELDO.RF");
    b = Dsetpath("COLOGNE\\.\\BERLIN");
    c = Dgetpath(curPath, 0);
    d = strcmp(curPath, "\\AMSTERDA.M\\DUSSELDO.RF\\COLOGNE\\BERLIN");

    ok = 0;
    if(!a && !b && !c && !d) {
        ok = 1;
    }
    out_tr_b(0x0211, "Dsetpath - current in the middle", ok);

    //-------------
    // Dsetpath - current at the end
    Dsetpath("\\");
    a = Dsetpath("\\AMSTERDA.M\\DUSSELDO.RF");
    b = Dsetpath("COLOGNE\\BERLIN\\.");
    c = Dgetpath(curPath, 0);
    d = strcmp(curPath, "\\AMSTERDA.M\\DUSSELDO.RF\\COLOGNE\\BERLIN");          // '.' at the end is ignored, or means that it switched to the same (current) dir at the end

    ok = 0;
    if(!a && !b && !c && !d) {
        ok = 1;
    }
    out_tr_b(0x0212, "Dsetpath - current at the end", ok);

    //-------------
    // Dsetpath - current and up
    a = Dsetpath("\\AMSTERDA.M\\DUSSELDO.RF");
    b = Dsetpath(".\\..");
    c = Dgetpath(curPath, 0);
    d = strcmp(curPath, "\\AMSTERDA.M");
    
    ok = 0;
    if(!a && !b && !c && !d) {
        ok = 1;
    }
    out_tr_b(0x0213, "Dsetpath - current and up", ok);
}

void createTestDirs(int level)
{
    out_s("Creating test dirs...");
    Dsetpath("\\");

    char *testDirs[6] = {"AMSTERDA.M", "DUSSELDO.RF", "COLOGNE", "BERLIN", "AMSTERDA.M", "BERLIN"};
    int i;
    
    for(i=0; i<6; i++) {
        (void) Dcreate(testDirs[i]);
        Dsetpath(testDirs[i]);
    }
    Dsetpath("\\");
}

void deleteTestDirs(void)
{
    out_s("Deleting test dirs...");
    Dsetpath("\\");
    deleteRecursive("AMSTERDA.M");

    Dsetpath("\\");
    Ddelete("AMSTERDA.M");
}

void deleteRecursive(char *subPath)
{
    if(strcmp(subPath, ".") == 0 || strcmp(subPath, "..") == 0) {
        return;
    }
    
    Dsetpath(subPath);                          // go in that dir
    
    char dta[44];
    memset(dta, 0, 44);
    
    Fsetdta(dta);
    WORD res = Fsfirst("*.*", 0x3f);
    
    while(res == 0) {                           // something found? 
        if((dta[21] & 0x10) != 0) {             // it's a dir
            deleteRecursive(dta + 30);          // delete it recursively
        } else {
            Fdelete(dta + 30);
        }
        
        Fsetdta(dta);
        res = Fsnext();                         // try to find next dir
    }
    
    Dsetpath("..");                             // go out of that dir
    
    char curPath[128];
    Dgetpath(curPath, 0);
    
    if(strcmp(curPath, "\\") == 0 || curPath[0] == 0) { // if we're in the root of drive, don't delete this
        return;
    }
    
    Ddelete(subPath);
}

