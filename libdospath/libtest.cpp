#include <iostream>
#include <map>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>

#include "libdospath.h"

void testShortToLongName(void)
{
    system("rm -rf /tmp/test");
    system("mkdir -p /tmp/test");
    system("touch /tmp/test/short.f");                  // short filename
    system("touch '/tmp/test/Long FileName 1.extension'");  // long filename

    std::string lp;
    printf("\n\nshortToLongPath -- 1st run\n");
    ldp_shortToLongPath(std::string("/tmp/test"), lp, false);                // first time - will create shorteners, feed them with dir content
    assert(lp == "/tmp/test");

    printf("\n\nshortToLongPath -- 2nd run\n");
    ldp_shortToLongPath(std::string("/tmp/test"), lp, false);                // second time - will use existing shorteners
    assert(lp == "/tmp/test");

    printf("\n\nshortToLongPath -- long filename is short\n");
    ldp_shortToLongPath(std::string("/tmp/test\\SHORT.F"), lp, false);      // test short filename 
    assert(lp == "/tmp/test/short.f");

    printf("\n\nshortToLongPath -- long filename is long\n");
    ldp_shortToLongPath(std::string("/tmp/test\\LONG_F~1.EXT"), lp, false); // test longer filename
    assert(lp == "/tmp/test/Long FileName 1.extension");

    // create another long file, see if we can miss it without refresh and hit it with refresh
    system("touch '/tmp/test/Long FileName 2.extension'");

    printf("\n\nshortToLongPath -- cache-miss without refresh\n");
    ldp_shortToLongPath(std::string("/tmp/test\\LONG_F~2.EXT"), lp, false); // NO refresh - will miss cache
    assert(lp == "/tmp/test/LONG_F~2.EXT");     // on failed translation the short filename will be also in the long version - this is expected on file / dir create

    printf("\n\nshortToLongPath -- cache-miss with refresh, then cache-hit\n");
    ldp_shortToLongPath(std::string("/tmp/test\\LONG_F~2.EXT"), lp, true);  // WITH refresh - will hit cache
    assert(lp == "/tmp/test/Long FileName 2.extension");
}

void testFindFirstAndNext(void)
{
    system("rm -rf /tmp/t");
    system("mkdir -p /tmp/t/a /tmp/t/b /tmp/t/c /tmp/t/ThisIsLongDir");
    system("touch /tmp/t/m /tmp/t/n /tmp/t/o /tmp/t/ThisIzLongFile.extension");
    system("echo 'hello, this is non empty file' > /tmp/t/m");
    system("echo 'different file size' > /tmp/t/n");
    system("echo 'this content will produce another different size' > /tmp/t/o");

    SearchParams sp;
    DiskItem di;
    int found = 0;

    // TEST 1: find everything + UP-DIR
    printf("\n\nFIND EVERYTHING WITH UP-DIR\n");
    sp.internal = NULL;
    sp.path = "/tmp/t/*.*";
    sp.attribs = 0xff;      // find anything
    sp.addUpDir = true;     // will add '.' and '..', so +2 items

    found = 0;
    while(ldp_findFirstAndNext(sp, di)) {
        printf("found %s: %s - %d B\n", (di.attribs & FA_DIR) ? "DIR " : "FILE" ,di.name.c_str(), di.size);
        found++;
    }
    assert(found == 10);    // 4 dirs + 4 files + 2 up-dir items = 10

    // TEST 2: find everything WITHOUT UP-DIR
    printf("\n\nFIND EVERYTHING WITHOUT UP-DIR\n");
    sp.internal = NULL;
    sp.addUpDir = false;    // don't add '.' and '..'
    found = 0;
    while(ldp_findFirstAndNext(sp, di)) {
        found++;
    }
    assert(found == 8);     // 4 dirs + 4 files = 10

    // TEST 3: find only FILES
    printf("\n\nFIND ONLY FILES\n");
    sp.internal = NULL;
    sp.attribs = ~FA_DIR;   // everything except DIR
    sp.addUpDir = true;     // won't add updir because looking just for files
    found = 0;
    while(ldp_findFirstAndNext(sp, di)) {
        assert((di.attribs & FA_DIR) == 0);     // dir flag not set
        found++;
    }
    assert(found == 4);     // just 4 files

    // NOTE: there is no way to include only DIRs in the resul, as the attribs make items be included 
    // in the results (FA_DIR = include dirs) and there is no separate bit for files only 

    // TEST 4: match 2 items with wildcards
    printf("\n\nMATCH 2 ITEMS WITH * WILDCARD\n");
    sp.internal = NULL;
    sp.attribs = 0xff;      // find anything
    sp.path = "/tmp/t/THIS*.*";

    found = 0;
    while(ldp_findFirstAndNext(sp, di)) {
        found++;
    }
    assert(found == 2);

    printf("\n\nMATCH 2 ITEMS WITH ? WILDCARD\n");
    sp.internal = NULL;
    sp.path = "/tmp/t/THISI?~?.*";

    found = 0;
    while(ldp_findFirstAndNext(sp, di)) {
        found++;
    }
    assert(found == 2);

    // TEST 5: match single file without wildcards
    printf("\n\nMATCH 1 FILE WITHOUT WILDCARDS\n");
    sp.internal = NULL;
    sp.path = "/tmp/t/THISIZ~1.EXT";
    sp.attribs = 0xff;      // find anything
    sp.addUpDir = true;     // will add '.' and '..', so +2 items

    found = 0;
    while(ldp_findFirstAndNext(sp, di)) {
        found++;
        assert(di.name == "THISIZ~1.EXT");
    }
    assert(found == 1);
    printf("MATCH 1 FILE WITHOUT WILDCARDS - OK!\n");

    // TEST 6: match single dir without wildcards
    printf("\n\nMATCH 1 DIR WITHOUT WILDCARDS\n");
    sp.internal = NULL;
    sp.path = "/tmp/t/THISIS~1";

    found = 0;
    while(ldp_findFirstAndNext(sp, di)) {
        found++;
        assert(di.name == "THISIS~1");
    }
    assert(found == 1);

    // TEST 7: don't match anything with wildcards
    printf("\n\nDON'T MATCH WITH WILDCARDS\n");
    sp.internal = NULL;
    sp.path = "/tmp/t/DOESNT*.*";

    found = 0;
    while(ldp_findFirstAndNext(sp, di)) {
        found++;
    }
    assert(found == 0);

    // TEST 8: don't match anything without wildcards
    printf("\n\nDON'T MATCH WITHOUT WILDCARDS\n");
    sp.internal = NULL;
    sp.path = "/tmp/t/DOESNT~1.MTC";

    found = 0;
    while(ldp_findFirstAndNext(sp, di)) {
        found++;
    }
    assert(found == 0);
}

int main(int argc, char**argv)
{
    ldp_setParam(0, 4);     // enable console logging

    testShortToLongName();

    testFindFirstAndNext();

    printf("\n\nclean-up\n");
    ldp_cleanup();

    printf("\n\nC++ tests (on private methods)\n");
    ldp_runCppTests();

    printf("\n\nIf you are reading this, all tests have passed! Yay!\n\n");

    return 0;
}
