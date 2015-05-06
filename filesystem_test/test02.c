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

char *testDirs[10] = {"AMSTERDA.M", "BERLIN", "COLOGNE", "DUSSELDO.RF", "ESPOO", "GLASGOW.001", "HELSINKI", "MADRID", "ROME", "WARSAW"};

void test02(void)
{
    out_s("Dsetpath and Dgetpath");
    Dsetdrv(drive);             // switch to selected drive
    
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
}

void createTestDirs(int level)
{
    if(level == 0) {
        out_s("Creating test dirs...");
        Dsetpath("\\");
    }

    if(level >= 8) {
        return;
    }
    
    int i;
    for(i=0; i<10; i++) {
        (void) Dcreate(testDirs[i]);
        
        if(i > (4 - level)) {
            continue;
        }
        
        Dsetpath(testDirs[i]);
        createTestDirs(level + 1);
        Dsetpath("..");
    }
}

void deleteTestDirs(void)
{
    int i;
    out_s("Deleting test dirs...");
    Dsetpath("\\");

    for(i=0; i<10; i++) {
        int res = Ddelete(testDirs[i]);
    
        if(res == -34) {         // path not found? dir doesn't exist, skip it
            continue;
        }
    
        if(res == -36) {         // access denied? dir not empty!
            deleteRecursive(testDirs[i]);
        }
    }
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
    Ddelete(subPath);
}

