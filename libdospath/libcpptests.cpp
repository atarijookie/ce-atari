#include <iostream>
#include <map>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include "dirtranslator.h"
#include "filenameshortener.h"
#include "utils.h"

#include "libcpptests.h"

void TestClass::runTests(void)
{
    splitPathAndFilename();
    testMerge();
    longToShortAndBack();
    testFeedingShortener();
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

