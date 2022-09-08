#include <iostream>
#include <map>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>

#include "dirtranslator.h"
#include "filenameshortener.h"
#include "utils.h"

#include "libcpptests.h"

extern bool logsEnabled;
extern int MAX_NAMES_IN_TRANSLATOR;

void TestClass::runTests(void)
{
    splitPathAndFilename();
    testMerge();
    longToShortAndBack();
    testFeedingShortener();
    testNamesCountLimiting();
}

void TestClass::splitPathAndFilename(void)
{
    //---------------------------
    printf("TEST: splitFilenameFromPath\n");
    std::string path, file;
    Utils::splitFilenameFromPath(std::string("/this/is/some/path/filename.ext"), path, file);
    assert(path == "/this/is/some/path");
    assert(file == "filename.ext");

    //---------------------------
    printf("TEST: splitFilenameFromExt\n");
    std::string filename, ext;
    Utils::splitFilenameFromExt(std::string("filename.ext"), filename, ext);
    assert(filename == "filename");
    assert(ext == "ext");
}

void TestClass::testMerge(void)
{
    printf("TEST: mergeFilenameAndExtension\n");
    std::string shortFn;
    FilenameShortener::mergeFilenameAndExtension(std::string("file"), std::string("ext"), false, shortFn);
    assert(shortFn == "file.ext");

    FilenameShortener::mergeFilenameAndExtension(std::string("fi"), std::string("e"), false, shortFn);
    assert(shortFn == "fi.e");

    FilenameShortener::mergeFilenameAndExtension(std::string("file"), std::string(""), false, shortFn);
    assert(shortFn == "file");

    FilenameShortener::mergeFilenameAndExtension(std::string(""), std::string("ext"), false, shortFn);
    assert(shortFn == ".ext");

    FilenameShortener::mergeFilenameAndExtension(std::string("file"), std::string("ex"), true, shortFn);
    assert(shortFn == "file    .ex ");
}

void TestClass::longToShortAndBack(void)
{
    bool res, created;
    std::string shortFn;

    printf("TEST: mergeFilenameAndExtension\n");
    FilenameShortener *fs = new FilenameShortener(std::string("/tmp"));
    fs->longToShortFileName(std::string("LongFile Name A.extension"), shortFn, &created);     // shorten unknown filename
    assert(shortFn == "LONGFI~1.EXT");
    assert(created);        // short version was created

    fs->longToShortFileName(std::string("LongFile Name A.extension"), shortFn, &created);     // shorten same filename to same short filename
    assert(shortFn == "LONGFI~1.EXT");
    assert(!created);       // short version was found

    std::string longFileName;
    res = fs->shortToLongFileName(std::string("LONGFI~1.EXT"), longFileName);       // reverse translation on existing - success
    assert(res);        // should be found
    assert(longFileName == "LongFile Name A.extension");

    res = fs->shortToLongFileName(std::string("LONGFI~2.EXT"), longFileName);       // reverse translation on non-existing - fail
    assert(!res);        // not found!

    fs->longToShortFileName(std::string("LongFile Name B.extension"), shortFn, &created);     // shorten other filename, get increment in short name
    assert(shortFn == "LONGFI~2.EXT");
    assert(created);        // short version was created

    res = fs->shortToLongFileName(std::string("LONGFI~2.EXT"), longFileName);       // reverse translation now on 2nd existing - success
    assert(res);        // should be found
    assert(longFileName == "LongFile Name B.extension");
}

void TestClass::testFeedingShortener(void)
{
    printf("TEST: testFeedingShortener\n");
    FilenameShortener *fs = new FilenameShortener(std::string("/tmp/f"));

    int foundCount;

    system("rm -rf /tmp/f");            // remove this test dir with all contents
    system("mkdir -p /tmp/f/a /tmp/f/b /tmp/f/c");          // create 3 dirs
    system("touch /tmp/f/m /tmp/f/n /tmp/f/o /tmp/f/p");    // create 4 files

    DirTranslator* dt = new DirTranslator();
    foundCount = dt->feedShortener(std::string("/tmp/f"), fs);
    assert(foundCount == 7);            // find all the dirs and files

    foundCount = dt->feedShortener(std::string("/tmp/f"), fs);
    assert(foundCount == 0);            // feeding same dir will result in 0 new found items

    system("mkdir -p /tmp/f/d");        // 1 new dirs
    system("touch /tmp/f/q");           // 1 new file
    foundCount = dt->feedShortener(std::string("/tmp/f"), fs);
    assert(foundCount == 2);            // 2 new items should be found now!

    delete dt;
}

void TestClass::testNamesCountLimiting(void)
{
    printf("TEST: testNamesCountLimiting\n");
    int foundCount, removed;

    system("rm -rf   /tmp/a   /tmp/b   /tmp/c   /tmp/d   /tmp/e   /tmp/f");            // remove this test dir with all contents
    system("mkdir -p /tmp/a   /tmp/b   /tmp/c   /tmp/d   /tmp/e   /tmp/f");
    system("touch    /tmp/a/a /tmp/b/b /tmp/c/c /tmp/d/d /tmp/e/e /tmp/f/f");

    DirTranslator* dt = new DirTranslator();

    // start with trying to get short names for 3 different dirs - this will internally create shorteners for thise dirs
    std::string shortFname;
    dt->longToShortFilename(std::string("/tmp/a"), std::string("a"), shortFname);
    dt->longToShortFilename(std::string("/tmp/b"), std::string("b"), shortFname);
    dt->longToShortFilename(std::string("/tmp/c"), std::string("c"), shortFname);
    assert(dt->size() == 3);        // we got few shorteners with 3 files in it, so the total count is 3

    int i;
    FilenameShortener *fs1 = dt->getShortenerForPath("/tmp/a");
    FilenameShortener *fs2 = dt->getShortenerForPath("/tmp/b");
    FilenameShortener *fs3 = dt->getShortenerForPath("/tmp/c");

    // add 30 items to each shortener, so 90 items, with the 3 items before we should end up with 93 items total
    for(i=0; i<30; i++) {
        char fn[32];
        sprintf(fn, "%d.ext", i);       // generate fake file name
        std::string shortFileName;

        fs1->longToShortFileName(std::string(fn), shortFileName);
        fs2->longToShortFileName(std::string(fn), shortFileName);
        fs3->longToShortFileName(std::string(fn), shortFileName);
    }
    assert(dt->size() == 93);           // 3 previous + 90 new items = 93 items

    // test if trying to clean too soon will not modify anything
    MAX_NAMES_IN_TRANSLATOR = 65;       // this limit is lower than what we actually have, but not enough time has passed, so nothing should be removed
    removed = dt->cleanUpShortenersIfNeeded();
    assert(dt->size() == 93);           // same as previous
    assert(removed == 0);               // no removed items

    // set the new maximum of allowed items, check if cleanup will remove one oldest translator
    MAX_NAMES_IN_TRANSLATOR = 65;
    dt->lastCleanUpCheck = 0;       // make this some low value so we can pass the 'cleanup just recently' check
    fs1->lastAccessTime = 2;        // set last access time of shorteners to some values which can be sorted
    fs2->lastAccessTime = 3;
    fs3->lastAccessTime = 1;

    FilenameShortener *fs4 = dt->getShortenerForPath("/tmp/d");     // now this will try to create new shortener and also do the clean up
    fs3 = NULL;                     // don't use this pointer, this shortener was just removed!

    // now that the clean up happened, the shortener with lowest access time should be removed, the count should change from 93 - 31 + 1 = 63
    assert(dt->size() == 63);           // 93 - 31 + 1 = 63

    dt->lastCleanUpCheck = 0;           // make this some low value so we can pass the 'cleanup just recently' check
    removed = dt->cleanUpShortenersIfNeeded();  // no items should be removed here, because we are below the MAX_NAMES_IN_TRANSLATOR limit
    assert(dt->size() == 63);           // same as previous
    assert(removed == 0);               // no removed items

    MAX_NAMES_IN_TRANSLATOR = 10;
    dt->lastCleanUpCheck = 0;           // make this some low value so we can pass the 'cleanup just recently' check
    removed = dt->cleanUpShortenersIfNeeded();  // only the last shortener will be kept, 2 will be removed
    assert(dt->size() == 1);            // last shortener has only 1 item
    assert(removed == 2);               // 2 shorteners were removed

    MAX_NAMES_IN_TRANSLATOR = 100000;   // back to initial large value

    delete dt;
}
