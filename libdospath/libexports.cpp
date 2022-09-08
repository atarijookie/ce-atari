#include <iostream>
#include <map>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include "dirtranslator.h"
#include "utils.h"

#include "libexports.h"
#include "libcpptests.h"

DirTranslator* dt = NULL;
extern bool logsEnabled;
extern int MAX_NAMES_IN_TRANSLATOR;

/*
    Call this function to set other than default value to some lib param

    paramNo     meaning                         paramVal
    0           log to console on/off           0/1
    1           maximum names stored in ram     0-max_int_value
*/
extern "C" void ldp_setParam(int paramNo, int paramVal)
{
    switch(paramNo) {
        case 0:     logsEnabled = paramVal;     break;
        case 1:     MAX_NAMES_IN_TRANSLATOR = paramVal; break;
    }
}

/*
    functions which use short file names (e.g. fopen()) will call this function
    with short file name and the function will convert if to long file name, which
    then can be used by host OS file functions to access the file

    :param shortPath: incomming short path
    :param longPath: outgoing long path

    :param refreshOnMiss: If true, then dict-miss (cache-miss) of short filename will trigger refresh of that dir translator.
                          (use true if filename should exist - e.g. on open(), delete())

                          If false, then dict-miss (cache-miss) of short filename will not try to do refresh of dir translator.
                          (use false if filename probably doesn't exist - e.g. on fcreate(), mkdir())
*/
extern "C" void ldp_shortToLongPath(const std::string &shortPath, std::string &longPath, bool refreshOnMiss)
{
    if(!dt) {
        dt = new DirTranslator();
    }

    dt->shortToLongPath(shortPath, longPath, refreshOnMiss);
}

/*
    You should call this function to release memory at the program exit.
*/
extern "C" void ldp_cleanup(void)
{
    if(dt) {
        delete dt;
        dt = NULL;
    }
}

/*
    this just runs the tests, not needed for normal usage
*/
extern "C" void ldp_runCppTests(void)
{
    TestClass *tc = new TestClass();

    tc->runTests();

    delete tc;
}
