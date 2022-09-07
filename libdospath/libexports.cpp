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

/*
    Call this function to set other than default value to some lib param

    paramNo     meaning                     paramVal
    0           log to console on/off       0/1
*/
extern "C" void ldp_setParam(int paramNo, int paramVal)
{
    switch(paramNo) {
        case 0:     logsEnabled = paramVal;     break;
    }
}

/*
    functions which use short file names (e.g. fopen()) will call this function
    with short file name and the function will convert if to long file name, which
    then can be used by host OS file functions to access the file

    :param shortPath: incomming short path
    :param longPath: outgoing long path
*/
extern "C" void ldp_shortToLongPath(const std::string &shortPath, std::string &longPath)
{
    if(!dt) {
        dt = new DirTranslator();
    }

    dt->shortToLongPath(shortPath, longPath);
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

extern "C" void ldp_runCppTests(void)
{
    TestClass *tc = new TestClass();

    tc->runTests();

    delete tc;
}
