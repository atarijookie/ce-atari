/*
    Tests of the public ldp_*() API from libdospath.h
*/
#include <string>

#include "doctest.h"
#include "test_helpers.h"

#include "libdospath.h"

TEST_CASE_FIXTURE(ScratchDir, "ldp_shortToLongPath translates short names to existing long names")
{
    sh("mkdir -p " + p("test"));
    sh("touch " + p("test/short.f"));                           // short filename
    sh("touch '" + p("test/Long FileName 1.extension") + "'");  // long filename

    std::string dir = p("test");
    std::string lp;

    // first time - will create shorteners, feed them with dir content
    ldp_shortToLongPath(dir, lp, false);
    CHECK(lp == dir);

    // second time - will use existing shorteners
    ldp_shortToLongPath(dir, lp, false);
    CHECK(lp == dir);

    // long filename is short
    ldp_shortToLongPath(dir + "\\SHORT.F", lp, false);
    CHECK(lp == dir + "/short.f");

    // long filename is long
    ldp_shortToLongPath(dir + "\\LONG_F~1.EXT", lp, false);
    CHECK(lp == dir + "/Long FileName 1.extension");

    SUBCASE("new file is missed without refresh and found with refresh") {
        sh("touch '" + p("test/Long FileName 2.extension") + "'");

        // NO refresh - will miss cache; on failed translation the short filename will be also in the long version - this is expected on file / dir create
        ldp_shortToLongPath(dir + "\\LONG_F~2.EXT", lp, false);
        CHECK(lp == dir + "/LONG_F~2.EXT");

        // WITH refresh - will hit cache
        ldp_shortToLongPath(dir + "\\LONG_F~2.EXT", lp, true);
        CHECK(lp == dir + "/Long FileName 2.extension");
    }
}

TEST_CASE_FIXTURE(ScratchDir, "ldp_shortToLongPath translates long file in long folders without pre-caching")
{
    sh("mkdir -p " + p("test/thisLongDirectory/thatLongFolder"));
    sh("touch " + p("test/thisLongDirectory/thatLongFolder/SomeLongFileName.extension"));

    std::string lp;
    ldp_shortToLongPath(p("test/THISLO~1/THATLO~1/SOMELO~1.EXT"), lp, true);
    CHECK(lp == p("test/thisLongDirectory/thatLongFolder/SomeLongFileName.extension"));
}

// go through all the search results, return how many items were found
static int countFound(SearchParams& sp, const char* expectName=NULL, bool expectNoDirs=false)
{
    DiskItem di;
    int found = 0;

    sp.internal = NULL;         // NULL before 1st call starts a new search
    sp.closeNow = false;

    while(ldp_findFirstAndNext(sp, di)) {
        if(expectName) {
            CHECK(di.name == expectName);
        }
        if(expectNoDirs) {
            CHECK((di.attribs & FA_DIR) == 0);
        }
        found++;
    }

    return found;
}

TEST_CASE_FIXTURE(ScratchDir, "ldp_findFirstAndNext")
{
    sh("mkdir -p " + p("t/a") + " " + p("t/b") + " " + p("t/c") + " " + p("t/ThisIsLongDir"));
    sh("touch " + p("t/m") + " " + p("t/n") + " " + p("t/o") + " " + p("t/ThisIzLongFile.extension"));
    sh("echo 'hello, this is non empty file' > " + p("t/m"));
    sh("echo 'different file size' > " + p("t/n"));
    sh("echo 'this content will produce another different size' > " + p("t/o"));

    SearchParams sp;
    sp.path = p("t/*.*");
    sp.attribs = 0xff;          // find anything
    sp.addUpDir = true;         // will add '.' and '..', so +2 items

    SUBCASE("find everything with up-dir") {
        CHECK(countFound(sp) == 10);    // 4 dirs + 4 files + 2 up-dir items = 10
    }

    SUBCASE("find everything without up-dir") {
        sp.addUpDir = false;            // don't add '.' and '..'
        CHECK(countFound(sp) == 8);     // 4 dirs + 4 files = 8
    }

    SUBCASE("find only files") {
        // NOTE: there is no way to include only DIRs in the result, as the attribs make items be included
        // in the results (FA_DIR = include dirs) and there is no separate bit for files only
        sp.attribs = ~FA_DIR;           // everything except DIR
        sp.addUpDir = true;             // won't add updir because looking just for files
        CHECK(countFound(sp, NULL, true) == 4);     // just 4 files
    }

    SUBCASE("match 2 items with * wildcard") {
        sp.path = p("t/THIS*.*");
        CHECK(countFound(sp) == 2);
    }

    SUBCASE("match 2 items with ? wildcard") {
        sp.path = p("t/THISI?~?.*");
        CHECK(countFound(sp) == 2);
    }

    SUBCASE("match 1 file without wildcards") {
        sp.path = p("t/THISIZ~1.EXT");
        CHECK(countFound(sp, "THISIZ~1.EXT") == 1);
    }

    SUBCASE("match 1 dir without wildcards") {
        sp.path = p("t/THISIS~1");
        CHECK(countFound(sp, "THISIS~1") == 1);
    }

    SUBCASE("exact match reports dir / file attribute correctly") {
        DiskItem di;

        sp.internal = NULL;
        sp.path = p("t/THISIS~1");                  // dir
        REQUIRE(ldp_findFirstAndNext(sp, di));
        CHECK((di.attribs & FA_DIR) != 0);
        CHECK(di.size == 0);                        // dirs have zero size
        CHECK(!ldp_findFirstAndNext(sp, di));       // nothing more after exact match

        sp.internal = NULL;
        sp.path = p("t/m");                         // non-empty file
        REQUIRE(ldp_findFirstAndNext(sp, di));
        CHECK((di.attribs & FA_DIR) == 0);
        CHECK(di.size > 0);
        CHECK(!ldp_findFirstAndNext(sp, di));
    }

    SUBCASE("match 1 file without wildcards in dir given by short name") {
        sh("echo 'inner file content' > '" + p("t/ThisIsLongDir/InnerLongFile.txt") + "'");

        DiskItem di;
        sp.internal = NULL;
        sp.path = p("t/THISIS~1/INNERL~1.TXT");    // short dir name + short file name
        REQUIRE(ldp_findFirstAndNext(sp, di));
        CHECK(di.name == "INNERL~1.TXT");
        CHECK((di.attribs & FA_DIR) == 0);
        CHECK(di.size == 19);                       // 'inner file content' + new line
        CHECK(!ldp_findFirstAndNext(sp, di));       // nothing more after exact match
    }

    SUBCASE("wildcard search in dir given by short name") {
        sh("touch '" + p("t/ThisIsLongDir/InnerLongFile.txt") + "' " + p("t/ThisIsLongDir/x.txt"));

        DiskItem di;
        int found = 0;
        bool gotInner = false;

        sp.internal = NULL;
        sp.path = p("t/THISIS~1/*.*");             // short dir name + wildcards
        sp.addUpDir = false;

        while(ldp_findFirstAndNext(sp, di)) {
            found++;
            gotInner |= (di.name == "INNERL~1.TXT");
        }

        CHECK(found == 2);
        CHECK(gotInner);
    }

    SUBCASE("not existing exact name falls back to scan of dir given by short name") {
        DiskItem di;
        sp.internal = NULL;
        sp.path = p("t/THISIS~1/NOTHERE.TXT");     // dir exists, file doesn't
        CHECK(!ldp_findFirstAndNext(sp, di));
    }

    SUBCASE("'*' without extension matches only names without extension") {
        sp.path = p("t/*");
        sp.addUpDir = false;
        CHECK(countFound(sp) == 7);     // dirs A, B, C, THISIS~1 + files M, N, O - but not THISIZ~1.EXT
    }

    SUBCASE("lowercase search pattern still matches uppercase short names") {
        sp.path = p("t/*.ext");
        CHECK(countFound(sp, "THISIZ~1.EXT") == 1);
    }

    SUBCASE("lowercase exact name falls back to scan and is found") {
        sp.path = p("t/thisiz~1.ext");
        CHECK(countFound(sp, "THISIZ~1.EXT") == 1);
    }

    SUBCASE("don't match anything with wildcards") {
        sp.path = p("t/DOESNT*.*");
        CHECK(countFound(sp) == 0);
    }

    SUBCASE("don't match anything without wildcards") {
        sp.path = p("t/DOESNT~1.MTC");
        CHECK(countFound(sp) == 0);
    }
}

// count of file descriptors open in this process
static int openFdCount(void)
{
    int count = 0;
    DIR* dir = opendir("/proc/self/fd");

    while(dir && readdir(dir)) {
        count++;
    }

    if(dir) {
        closedir(dir);
    }

    return count;
}

TEST_CASE_FIXTURE(ScratchDir, "ldp_findFirstAndNext search termination")
{
    sh("mkdir -p " + p("t"));
    sh("touch " + p("t/a") + " " + p("t/b") + " " + p("t/c"));

    SearchParams sp;
    DiskItem di;
    sp.internal = NULL;
    sp.closeNow = false;
    sp.path = p("t/*.*");
    sp.attribs = 0xff;
    sp.addUpDir = false;

    SUBCASE("closeNow terminates search early and closes the dir") {
        int fdsBefore = openFdCount();

        REQUIRE(ldp_findFirstAndNext(sp, di));      // 1st item found, dir is open now
        CHECK(openFdCount() == fdsBefore + 1);

        sp.closeNow = true;                         // terminate the search before reaching the last item
        CHECK(!ldp_findFirstAndNext(sp, di));
        CHECK(openFdCount() == fdsBefore);          // dir was closed
        CHECK(!ldp_findFirstAndNext(sp, di));       // still nothing on next call
    }

    SUBCASE("calling again after search finished keeps returning false") {
        int found = 0;
        while(ldp_findFirstAndNext(sp, di)) {
            found++;
        }
        CHECK(found == 3);

        CHECK(!ldp_findFirstAndNext(sp, di));
        CHECK(!ldp_findFirstAndNext(sp, di));
    }

    SUBCASE("search in not existing dir returns false") {
        sp.path = p("doesNotExist/*.*");
        CHECK(!ldp_findFirstAndNext(sp, di));
        CHECK(!ldp_findFirstAndNext(sp, di));
    }
}

TEST_CASE_FIXTURE(ScratchDir, "ldp_updateFileName")
{
    std::string dir = p("r");
    sh("mkdir -p " + dir);
    sh("touch '" + dir + "/LongFile Name.txt'");

    std::string lp;
    ldp_shortToLongPath(dir + "/LONGFI~1.TXT", lp, false);     // cache the dir content
    REQUIRE(lp == dir + "/LongFile Name.txt");

    SUBCASE("renamed file keeps its short name, which now leads to the new long name") {
        sh("mv '" + dir + "/LongFile Name.txt' '" + dir + "/Renamed Long.txt'");
        ldp_updateFileName(dir, "LongFile Name.txt", "Renamed Long.txt");

        ldp_shortToLongPath(dir + "/LONGFI~1.TXT", lp, false); // no refresh - must be updated already
        CHECK(lp == dir + "/Renamed Long.txt");
    }

    SUBCASE("renaming unknown name doesn't change anything") {
        ldp_updateFileName(dir, "Unknown Name.txt", "Something Else.txt");

        ldp_shortToLongPath(dir + "/LONGFI~1.TXT", lp, false);
        CHECK(lp == dir + "/LongFile Name.txt");
    }
}

TEST_CASE_FIXTURE(ScratchDir, "ldp_shortToLongPath finds content of dir which didn't exist when first translated")
{
    sh("mkdir -p " + p("n"));
    std::string path = p("n/NEWDIR/LONGFI~1.TXT");
    std::string lp;

    // dir doesn't exist yet - nothing can be translated, but this caches (empty) content of not existing dir
    ldp_shortToLongPath(path, lp, false);
    CHECK(lp == path);

    // dir created (e.g. by Atari with short name), then long named file put in it
    sh("mkdir -p " + p("n/NEWDIR"));
    sh("touch '" + p("n/NEWDIR/LongFile Name.txt") + "'");

    ldp_shortToLongPath(path, lp, true);           // with refresh - cached content gets updated
    CHECK(lp == p("n/NEWDIR/LongFile Name.txt"));
}

extern std::string logFilePath;

// restores the library's log file path when the test case ends, even on failure
struct LogFilePathGuard
{
    std::string saved;
    LogFilePathGuard() : saved(logFilePath) {}
    ~LogFilePathGuard() { logFilePath = saved; }
};

TEST_CASE_FIXTURE(ScratchDir, "ldp_setParam logs the same correct values to console and file")
{
    LogFilePathGuard guard;
    std::string logFile = p("ldp.log");

    // ldp_setParam() always logs to both console and file - both outputs need their own copy of the printf args
    ldp_setParam(2, (uint64_t) logFile.c_str());        // set log file path
    ldp_setParam(1, 0);                                 // log level to file - 0 is the default

    FILE* f = fopen(logFile.c_str(), "r");
    REQUIRE(f != NULL);

    char line[1024];
    std::string content;
    while(fgets(line, sizeof(line), f)) {
        content += line;
    }
    fclose(f);

    INFO("log file content:\n", content);
    CHECK(content.find("ldp_setParam - paramNo: 2, paramVal: " + logFile) != std::string::npos);
    CHECK(content.find("ldp_setParam - paramNo: 1, paramVal: 0\n") != std::string::npos);
}
