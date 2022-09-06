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
    printf("TEST: splitFilenameFromPath\n");
    std::string path, file;
    Utils::splitFilenameFromPath(std::string("/this/is/some/path/filename.ext"), path, file);
    assert(path == "/this/is/some/path");
    assert(file == "filename.ext");

    printf("TEST: splitFilenameFromExt\n");
    std::string filename, ext;
    Utils::splitFilenameFromExt(std::string("filename.ext"), filename, ext);
    assert(filename == "filename");
    assert(ext == "ext");

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

