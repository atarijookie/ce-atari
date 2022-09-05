#include <iostream>
#include <map>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include "dirtranslator.h"
#include "utils.h"


DirTranslator* dt = NULL;

/*
    functions which use short file names (e.g. fopen()) will call this function
    with short file name and the function will convert if to long file name, which
    then can be used by host OS file functions to access the file

    :param shortPath: incomming short path
    :param longPath: outgoing long path
*/
void shortToLongPath(const std::string &shortPath, std::string &longPath)
{
    if(!dt) {
        dt = new DirTranslator();
    }

    dt->shortToLongPath(shortPath, longPath);
}

/*
    You should call this function to release memory at the program exit.
*/
void libdospath_cleanup(void)
{
    if(dt) {
        delete dt;
        dt = NULL;
    }
}

//--------------------------------

int main(int argc, char**argv)
{
    //-------------
    // TEST: toHostSeparators
    std::string hs = "/mnt/test/atari\\C\\myfolder/what\\ever";
    assert(hs.find('\\') != std::string::npos);         // before call - contains '\\'
    Utils::toHostSeparators(hs);
    printf("hs: %s\n", hs.c_str());
    assert(hs.find('\\') == std::string::npos);         // after call - doesn't contain '\\'
    assert(hs == "/mnt/test/atari/C/myfolder/what/ever");

    //-------------
    // TEST: splitString
    std::vector<std::string> pathParts;
    Utils::splitString(hs, '/', pathParts);         // explode string to individual parts
    assert(pathParts.size() == 7);                  // make sure it's 7 parts after split
    assert(pathParts[0] == std::string("mnt"));
    assert(pathParts[3] == std::string("C"));
    assert(pathParts[6] == std::string("ever"));

    //-------------
    // TEST: joinString
    std::string out;
    Utils::joinStrings(pathParts, out);      // join all
    assert(out == "/mnt/test/atari/C/myfolder/what/ever");

    Utils::joinStrings(pathParts, out, 3);   // join first 3 parts only
    assert(out == "/mnt/test/atari");

    //-------------
    mkdir("/tmp/test", S_IRUSR | S_IRGRP | S_IROTH);
    std::string sp = "/tmp/test";
    std::string lp;

    printf("shortToLongPath -- 1st run\n\n");
    shortToLongPath(sp, lp);                // first time
    printf("output: %s\n", lp.c_str());

    printf("shortToLongPath -- 2nd run\n\n");
    shortToLongPath(sp, lp);                // second time
    printf("output: %s\n", lp.c_str());

    libdospath_cleanup();
    return 0;
}
