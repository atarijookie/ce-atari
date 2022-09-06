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
    bool res;
    std::string shortFn;

    printf("TEST: mergeFilenameAndExtension\n");
    FilenameShortener *fs = new FilenameShortener(std::string("/tmp"));
    fs->longToShortFileName(std::string("LongFile Name A.extension"), shortFn);     // shorten unknown filename
    assert(shortFn == "LONGFI~1.EXT");

    fs->longToShortFileName(std::string("LongFile Name A.extension"), shortFn);     // shorten same filename to same short filename
    assert(shortFn == "LONGFI~1.EXT");

    std::string longFileName;
    res = fs->shortToLongFileName(std::string("LONGFI~1.EXT"), longFileName);       // reverse translation on existing - success
    assert(res);        // should be found
    assert(longFileName == "LongFile Name A.extension");

    res = fs->shortToLongFileName(std::string("LONGFI~2.EXT"), longFileName);       // reverse translation on non-existing - fail
    assert(!res);        // not found!

    fs->longToShortFileName(std::string("LongFile Name B.extension"), shortFn);     // shorten other filename, get increment in short name
    assert(shortFn == "LONGFI~2.EXT");

    res = fs->shortToLongFileName(std::string("LONGFI~2.EXT"), longFileName);       // reverse translation now on 2nd existing - success
    assert(res);        // should be found
    assert(longFileName == "LongFile Name B.extension");
}

