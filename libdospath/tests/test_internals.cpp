/*
    Tests of the internal C++ classes (DirTranslator, FilenameShortener, UtilsLib).

    These classes declare 'friend class TestClass', so the TestClass below is
    used only as a gateway to their private members - tests themselves are
    regular doctest test cases.
*/
#include <map>
#include <vector>
#include <string>
#include <stdio.h>
#include <dirent.h>
#include <string.h>

#include "doctest.h"
#include "test_helpers.h"

#include "dirtranslator.h"
#include "filenameshortener.h"
#include "utilslib.h"

extern int MAX_NAMES_IN_TRANSLATOR;

class TestClass
{
public:
    // FilenameShortener privates
    static void mergeFilenameAndExtension(const std::string& fn, const std::string& ext, bool extendWithSpaces, std::string& merged) {
        FilenameShortener::mergeFilenameAndExtension(fn, ext, extendWithSpaces, merged);
    }
    static uint32_t& lastAccessTime(FilenameShortener* fs)  { return fs->lastAccessTime; }

    // DirTranslator privates
    static int feedShortener(DirTranslator* dt, const std::string& path, FilenameShortener* fs) { return dt->feedShortener(path, fs); }
    static bool longToShortFilename(DirTranslator* dt, const std::string& longHostPath, const std::string& longFname, std::string& shortFname) {
        return dt->longToShortFilename(longHostPath, longFname, shortFname);
    }
    static FilenameShortener* getShortenerForPath(DirTranslator* dt, const std::string& path) { return dt->getShortenerForPath(path); }
    static int size(DirTranslator* dt)                                  { return dt->size(); }
    static int shortenerCount(DirTranslator* dt)                        { return dt->mapPathToShortener.size(); }
    static int cleanUpShortenersIfNeeded(DirTranslator* dt)             { return dt->cleanUpShortenersIfNeeded(); }
    static uint32_t& lastCleanUpCheck(DirTranslator* dt)                { return dt->lastCleanUpCheck; }
    static std::map<std::string, std::string>& mapSymlinks(DirTranslator* dt) { return dt->mapSymlinks; }
    static unsigned char getEntryType(const std::string& dirPath, const struct dirent* de) { return DirTranslator::getEntryType(dirPath, de); }
};

typedef TestClass T;

TEST_CASE("UtilsLib::splitFilenameFromPath / splitFilenameFromExt")
{
    std::string path, file;
    UtilsLib::splitFilenameFromPath(std::string("/this/is/some/path/filename.ext"), path, file);
    CHECK(path == "/this/is/some/path");
    CHECK(file == "filename.ext");

    std::string filename, ext;
    UtilsLib::splitFilenameFromExt(std::string("filename.ext"), filename, ext);
    CHECK(filename == "filename");
    CHECK(ext == "ext");
}

TEST_CASE("FilenameShortener::mergeFilenameAndExtension")
{
    std::string shortFn;

    T::mergeFilenameAndExtension("file", "ext", false, shortFn);
    CHECK(shortFn == "file.ext");

    T::mergeFilenameAndExtension("fi", "e", false, shortFn);
    CHECK(shortFn == "fi.e");

    T::mergeFilenameAndExtension("file", "", false, shortFn);
    CHECK(shortFn == "file");

    T::mergeFilenameAndExtension("", "ext", false, shortFn);
    CHECK(shortFn == ".ext");

    T::mergeFilenameAndExtension("file", "ex", true, shortFn);
    CHECK(shortFn == "file    .ex ");
}

TEST_CASE("FilenameShortener long to short and back")
{
    bool res, created;
    std::string shortFn, longFileName;
    FilenameShortener fs(LDP_TEST_ROOT);

    fs.longToShortFileName("LongFile Name A.extension", shortFn, &created);     // shorten unknown filename
    CHECK(shortFn == "LONGFI~1.EXT");
    CHECK(created);         // short version was created

    fs.longToShortFileName("LongFile Name A.extension", shortFn, &created);     // shorten same filename to same short filename
    CHECK(shortFn == "LONGFI~1.EXT");
    CHECK(!created);        // short version was found

    res = fs.shortToLongFileName("LONGFI~1.EXT", longFileName);     // reverse translation on existing - success
    CHECK(res);
    CHECK(longFileName == "LongFile Name A.extension");

    res = fs.shortToLongFileName("LONGFI~2.EXT", longFileName);     // reverse translation on non-existing - fail
    CHECK(!res);

    fs.longToShortFileName("LongFile Name B.extension", shortFn, &created);     // shorten other filename, get increment in short name
    CHECK(shortFn == "LONGFI~2.EXT");
    CHECK(created);

    res = fs.shortToLongFileName("LONGFI~2.EXT", longFileName);     // reverse translation now on 2nd existing - success
    CHECK(res);
    CHECK(longFileName == "LongFile Name B.extension");
}

TEST_CASE_FIXTURE(ScratchDir, "DirTranslator::feedShortener only reports new items")
{
    std::string f = p("f");
    sh("mkdir -p " + f + "/a " + f + "/b " + f + "/c");                     // create 3 dirs
    sh("touch " + f + "/m " + f + "/n " + f + "/o " + f + "/p");            // create 4 files

    FilenameShortener fs(f);
    DirTranslator dt;

    CHECK(T::feedShortener(&dt, f, &fs) == 7);      // find all the dirs and files
    CHECK(T::feedShortener(&dt, f, &fs) == 0);      // feeding same dir will result in 0 new found items

    sh("mkdir -p " + f + "/d");                     // 1 new dir
    sh("touch " + f + "/q");                        // 1 new file
    CHECK(T::feedShortener(&dt, f, &fs) == 2);      // 2 new items should be found now!
}

// fake directory entry, as readdir() would return it
static struct dirent makeDirent(const char* name, unsigned char type)
{
    struct dirent de;
    memset(&de, 0, sizeof(de));
    strncpy(de.d_name, name, sizeof(de.d_name) - 1);
    de.d_type = type;
    return de;
}

TEST_CASE_FIXTURE(ScratchDir, "DirTranslator::getEntryType falls back to lstat() when filesystem reports DT_UNKNOWN")
{
    sh("mkdir -p " + p("dir"));
    sh("touch " + p("file"));
    sh("ln -s file " + p("link"));

    SUBCASE("type reported by filesystem is used as is") {
        struct dirent de = makeDirent("doesNotExist", DT_REG);
        CHECK(T::getEntryType(root, &de) == DT_REG);
    }

    SUBCASE("DT_UNKNOWN is resolved using lstat()") {
        struct dirent deDir  = makeDirent("dir",  DT_UNKNOWN);
        struct dirent deFile = makeDirent("file", DT_UNKNOWN);
        struct dirent deLink = makeDirent("link", DT_UNKNOWN);
        struct dirent deNone = makeDirent("doesNotExist", DT_UNKNOWN);

        CHECK(T::getEntryType(root, &deDir)  == DT_DIR);
        CHECK(T::getEntryType(root, &deFile) == DT_REG);
        CHECK(T::getEntryType(root, &deLink) == DT_LNK);     // symlink stays symlink - not followed, so it's still skipped
        CHECK(T::getEntryType(root, &deNone) == DT_UNKNOWN);
    }
}

// restores MAX_NAMES_IN_TRANSLATOR when the test case ends, even on failure
struct MaxNamesGuard
{
    int saved;
    MaxNamesGuard() : saved(MAX_NAMES_IN_TRANSLATOR) {}
    ~MaxNamesGuard() { MAX_NAMES_IN_TRANSLATOR = saved; }
};

TEST_CASE_FIXTURE(ScratchDir, "DirTranslator removes least recently used shorteners above names limit")
{
    MaxNamesGuard guard;
    int removed;
    std::string a = p("a"), b = p("b"), c = p("c"), d = p("d");

    sh("mkdir -p " + a + " " + b + " " + c + " " + d);
    sh("touch " + a + "/a " + b + "/b " + c + "/c " + d + "/d");

    DirTranslator dt;

    // start with trying to get short names for 3 different dirs - this will internally create shorteners for those dirs
    std::string shortFname;
    T::longToShortFilename(&dt, a, "a", shortFname);
    T::longToShortFilename(&dt, b, "b", shortFname);
    T::longToShortFilename(&dt, c, "c", shortFname);
    REQUIRE(T::size(&dt) == 3);         // we got 3 shorteners with 1 file in each, so the total count is 3

    FilenameShortener *fs1 = T::getShortenerForPath(&dt, a);
    FilenameShortener *fs2 = T::getShortenerForPath(&dt, b);
    FilenameShortener *fs3 = T::getShortenerForPath(&dt, c);

    // add 30 items to each shortener, so 90 items, with the 3 items before we should end up with 93 items total
    for(int i=0; i<30; i++) {
        char fn[32];
        sprintf(fn, "%d.ext", i);       // generate fake file name
        std::string shortFileName;

        fs1->longToShortFileName(fn, shortFileName);
        fs2->longToShortFileName(fn, shortFileName);
        fs3->longToShortFileName(fn, shortFileName);
    }
    REQUIRE(T::size(&dt) == 93);        // 3 previous + 90 new items = 93 items

    // limit is lower than what we actually have, but not enough time has passed, so nothing should be removed
    MAX_NAMES_IN_TRANSLATOR = 65;
    removed = T::cleanUpShortenersIfNeeded(&dt);
    CHECK(T::size(&dt) == 93);          // same as previous
    CHECK(removed == 0);                // no removed items

    // check if cleanup will remove one oldest translator
    T::lastCleanUpCheck(&dt) = 0;       // make this some low value so we can pass the 'cleanup just recently' check
    T::lastAccessTime(fs1) = 2;         // set last access time of shorteners to some values which can be sorted
    T::lastAccessTime(fs2) = 3;
    T::lastAccessTime(fs3) = 1;

    T::getShortenerForPath(&dt, d);     // now this will try to create new shortener and also do the clean up
    fs3 = NULL;                         // don't use this pointer, this shortener was just removed!

    // the shortener with lowest access time should be removed, the count should change from 93 - 31 + 1 = 63
    CHECK(T::size(&dt) == 63);

    T::lastCleanUpCheck(&dt) = 0;
    removed = T::cleanUpShortenersIfNeeded(&dt);    // no items should be removed here, because we are below the limit
    CHECK(T::size(&dt) == 63);
    CHECK(removed == 0);

    MAX_NAMES_IN_TRANSLATOR = 10;
    T::lastCleanUpCheck(&dt) = 0;
    removed = T::cleanUpShortenersIfNeeded(&dt);    // only the last shortener will be kept, 2 will be removed
    CHECK(T::size(&dt) == 1);           // last shortener has only 1 item
    CHECK(removed == 2);
}

TEST_CASE("DirTranslator symlinks")
{
    DirTranslator dt;
    std::map<std::string, std::string>& symlinks = T::mapSymlinks(&dt);
    std::string src = "/this/is/source";
    std::string dst = "/that/iz/dest";

    REQUIRE(symlinks.empty());

    SUBCASE("add, update and remove symlink") {
        dt.symlink(src, dst);                   // add symlink
        REQUIRE(symlinks.size() == 1);
        CHECK(symlinks.begin()->first == src);  // make sure it's stored properly
        CHECK(symlinks.begin()->second == dst);

        CHECK(dt.gotSymlink(src));              // lookup by source
        CHECK(!dt.gotSymlink("/we/dont/have/this"));

        dst = "/thot/new/dezt";
        dt.symlink(src, dst);                   // update existing symlink
        REQUIRE(symlinks.size() == 1);          // still only 1
        CHECK(symlinks.begin()->first == src);
        CHECK(symlinks.begin()->second == dst);

        dt.symlink(src, "");                    // remove existing symlink - use empty destination
        CHECK(symlinks.empty());
    }

    SUBCASE("apply symlink in shortToLongPath") {
        std::string shortPath = "/THIS/PATH/SOME/FILE.TXT";
        std::string longPath;

        // the long path will match short path when the short path isn't stored in shorteners and it even doesn't exist on disk
        dt.shortToLongPath(shortPath, longPath, false);
        CHECK(longPath == shortPath);

        dt.symlink("/THIS/PATH", "/THAT/DIR");
        dt.shortToLongPath(shortPath, longPath, false);     // translate and apply symlink
        CHECK(longPath == "/THAT/DIR/SOME/FILE.TXT");

        // symlink matching only part of a path component must not be applied: /THIS/PATH vs /THIS/PATHS
        std::string similarShortPath = "/THIS/PATHS/SOME/FILE.TXT";
        dt.shortToLongPath(similarShortPath, longPath, false);
        CHECK(longPath == similarShortPath);
    }

    SUBCASE("most specific symlink wins when more symlinks match") {
        std::string longPath;
        std::vector<std::string> applied;

        dt.symlink("/THIS", "/ONE");                        // less specific
        dt.symlink("/THIS/PATH", "/TWO");                   // more specific - sorted after '/THIS' in the map

        dt.shortToLongPath("/THIS/PATH/FILE.TXT", longPath, false, &applied);
        CHECK(longPath == "/TWO/FILE.TXT");
        REQUIRE(applied.size() == 1);
        CHECK(applied[0] == "/THIS/PATH");                  // reported the symlink which was really applied

        dt.shortToLongPath("/THIS/OTHER/FILE.TXT", longPath, false);    // only the less specific symlink matches
        CHECK(longPath == "/ONE/OTHER/FILE.TXT");

        dt.shortToLongPath("/THIS/PATHS/FILE.TXT", longPath, false);    // partial match of '/THIS/PATH' must not win
        CHECK(longPath == "/ONE/PATHS/FILE.TXT");
    }

    SUBCASE("trailing '/' in symlink source / destination / path") {
        std::string longPath;

        SUBCASE("no '/' at end")                { dt.symlink("/THIS/PATH",  "/THAT/DIR");  }
        SUBCASE("source with '/'")              { dt.symlink("/THIS/PATH/", "/THAT/DIR");  }
        SUBCASE("source and dest with '/'")     { dt.symlink("/THIS/PATH/", "/THAT/DIR/"); }

        dt.shortToLongPath("/THIS/PATH/SOME/FILE.TXT", longPath, false);
        CHECK(longPath == "/THAT/DIR/SOME/FILE.TXT");

        dt.shortToLongPath("/THIS/PATH", longPath, false);      // nothing more after symlink, without '/'
        CHECK(longPath == "/THAT/DIR");

        dt.shortToLongPath("/THIS/PATH/", longPath, false);     // nothing more after symlink, ends with '/'
        CHECK(longPath == "/THAT/DIR");
    }
}

// shorten all names in the given order, then check each name got the expected short name and translates back
static void checkShortNames(FilenameShortener& fs, const char* names[][2], int count)
{
    for(int i=0; i<count; i++) {
        std::string shortFn;
        REQUIRE(fs.longToShortFileName(names[i][0], shortFn));
        CHECK_MESSAGE(shortFn == names[i][1], "long name: ", names[i][0]);
    }

    for(int i=0; i<count; i++) {                    // after all were added, every short name must still lead to its own long name
        std::string longFn;
        CHECK_MESSAGE(fs.shortToLongFileName(names[i][1], longFn), "short name: ", names[i][1]);
        CHECK(longFn == names[i][0]);
    }
}

TEST_CASE("FilenameShortener creates unique 8.3 names when shortened names collide")
{
    FilenameShortener fs(LDP_TEST_ROOT);

    SUBCASE("extension cut to 3 chars") {
        const char* names[][2] = { {"index.htm", "INDEX.HTM"}, {"index.html", "INDEX~1.HTM"} };
        checkShortNames(fs, names, 2);
    }

    SUBCASE("names different only in case") {
        const char* names[][2] = { {"ABC.TXT", "ABC.TXT"}, {"abc.txt", "ABC~1.TXT"}, {"Abc.Txt", "ABC~2.TXT"} };
        checkShortNames(fs, names, 3);
    }

    SUBCASE("not allowed char replaced with '_'") {
        const char* names[][2] = { {"a_b.txt", "A_B.TXT"}, {"a b.txt", "A_B~1.TXT"} };
        checkShortNames(fs, names, 2);
    }

    SUBCASE("names without filename part") {
        const char* names[][2] = { {".bashrc", ".BAS"}, {".bash_history", "~1.BAS"} };
        checkShortNames(fs, names, 2);
    }

    SUBCASE("real file name looking like generated short name") {
        const char* names[][2] = { {"LongFile Name.extension", "LONGFI~1.EXT"}, {"LONGFI~1.EXT", "LONGFI~2.EXT"} };
        checkShortNames(fs, names, 2);
    }

    SUBCASE("long names with different extension keep their numbering") {
        const char* names[][2] = { {"LongFile Name A.txt", "LONGFI~1.TXT"}, {"LongFile Name A.doc", "LONGFI~2.DOC"} };
        checkShortNames(fs, names, 2);
    }
}

TEST_CASE("FilenameShortener special names")
{
    FilenameShortener fs(LDP_TEST_ROOT);

    const char* names[][2] = {
        {"12345678.txt",    "12345678.TXT"},    // exactly 8 chars - kept as is
        {"123456789.txt",   "123456~1.TXT"},    // 9 chars - shortened
        {"exactly8",        "EXACTLY8"},        // no extension
        {"archive.tar.gz",  "ARCHIV~1.GZ"},     // more dots - only the last one separates extension
        {"a.b.c.d",         "A_B_C.D"},         // inner dots replaced with '_'
        {"file.",           "FILE"},            // trailing dot - empty extension
        {"Caf\xc3\xa9.txt",  "CAF__.TXT"},       // UTF-8 'e with acute' - each byte replaced with '_'
        {"   .txt",         "___.TXT"},         // spaces replaced with '_'
    };
    checkShortNames(fs, names, sizeof(names) / sizeof(names[0]));
}

TEST_CASE("FilenameShortener numbering gets shorter prefix for longer numbers")
{
    FilenameShortener fs(LDP_TEST_ROOT);
    std::string shortFn[102];

    for(int i=0; i<102; i++) {
        char fn[64];
        sprintf(fn, "LongFile Name %03d.ext", i);
        REQUIRE(fs.longToShortFileName(fn, shortFn[i]));
    }

    CHECK(shortFn[0]   == "LONGFI~1.EXT");      // ~1 .. ~9     - 6 chars prefix
    CHECK(shortFn[8]   == "LONGFI~9.EXT");
    CHECK(shortFn[9]   == "LONGF~10.EXT");      // ~10 .. ~99   - 5 chars prefix
    CHECK(shortFn[98]  == "LONGF~99.EXT");
    CHECK(shortFn[99]  == "LONG~100.EXT");      // ~100 .. ~999 - 4 chars prefix
    CHECK(shortFn[101] == "LONG~102.EXT");
}

TEST_CASE_FIXTURE(ScratchDir, "FilenameShortener::shortToLongFileName special cases")
{
    FilenameShortener fs(root);
    std::string longFn, shortFn;

    SUBCASE("empty short name is not found") {
        CHECK(!fs.shortToLongFileName("", longFn));
    }

    SUBCASE("not cached name which exists on disk as is gets found and cached") {
        sh("touch " + p("UNCACHED.TXT"));
        REQUIRE(fs.size() == 0);

        CHECK(fs.shortToLongFileName("UNCACHED.TXT", longFn));
        CHECK(longFn == "UNCACHED.TXT");
        CHECK(fs.size() == 1);                          // now cached
    }

    SUBCASE("short names are case sensitive - lowercase version is not translated") {
        REQUIRE(fs.longToShortFileName("LongFile Name.txt", shortFn));
        REQUIRE(shortFn == "LONGFI~1.TXT");

        CHECK(!fs.shortToLongFileName("longfi~1.txt", longFn));
    }
}

TEST_CASE("FilenameShortener::updateLongFileName onto already known long name")
{
    // e.g. file A renamed over existing file B - only B exists after that, both short names lead to it
    FilenameShortener fs(LDP_TEST_ROOT);
    std::string shortA, shortB, longFn, shortFn;

    REQUIRE(fs.longToShortFileName("LongFile Name A.txt", shortA));
    REQUIRE(fs.longToShortFileName("LongFile Name B.txt", shortB));
    REQUIRE(shortA == "LONGFI~1.TXT");
    REQUIRE(shortB == "LONGFI~2.TXT");

    fs.updateLongFileName("LongFile Name A.txt", "LongFile Name B.txt");

    CHECK(fs.shortToLongFileName(shortA, longFn));
    CHECK(longFn == "LongFile Name B.txt");
    CHECK(fs.shortToLongFileName(shortB, longFn));
    CHECK(longFn == "LongFile Name B.txt");

    CHECK(fs.longToShortFileName("LongFile Name B.txt", shortFn));
    CHECK(shortFn == shortB);                           // B keeps its original short name
}

TEST_CASE_FIXTURE(ScratchDir, "DirTranslator::updateFileName doesn't cache not cached dir")
{
    sh("touch '" + p("LongFile Name.txt") + "'");

    DirTranslator dt;
    dt.updateFileName(root, "LongFile Name.txt", "Renamed Long.txt");
    CHECK(T::shortenerCount(&dt) == 0);                 // nothing to update, so nothing should be created
}

TEST_CASE_FIXTURE(ScratchDir, "DirTranslator assigns short names in sorted order, not in readdir() order")
{
    // create colliding names in shuffled order - readdir() order then depends on the filesystem (creation order,
    // reverse creation order, hash order, ...), but the short names must be the same anyway
    const int order[10] = { 7, 2, 9, 0, 5, 3, 8, 1, 6, 4 };
    std::string dir = p("sorted");
    sh("mkdir -p " + dir);

    for(int i=0; i<10; i++) {
        char fn[64];
        sprintf(fn, "/LongFile Name %d.ext", order[i]);
        sh("touch '" + dir + fn + "'");
    }

    DirTranslator dt;

    for(int i=0; i<10; i++) {
        char fn[64], expected[16];
        sprintf(fn, "LongFile Name %d.ext", i);
        sprintf(expected, (i < 9) ? "LONGFI~%d.EXT" : "LONGF~%d.EXT", i + 1);     // from '~10' the number takes one more char

        std::string shortFn;
        REQUIRE(T::longToShortFilename(&dt, dir, fn, shortFn));    // first call creates and feeds the shortener with the whole dir
        CHECK_MESSAGE(shortFn == expected, "long name: ", fn);
    }
}
