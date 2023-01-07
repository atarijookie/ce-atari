#include <iostream>
#include <map>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include "dirtranslator.h"
#include "utilslib.h"

#include "libdospath.h"
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
    This function will rename / update single file name in the filename shortener. Call this when you rename a file from old to new name.
    :param hostPath: path where the updated file is located
    :param oldFileName: original filename from which we rename the file
    :param newFileName: new file name to which we rename the file
*/

extern "C" void ldp_updateFileName(const std::string& hostPath, const std::string& oldFileName, const std::string& newFileName)
{
    if(!dt) {
        dt = new DirTranslator();
    }

    dt->updateFileName(hostPath, oldFileName, newFileName);
}

/*
    Call this function to find all the files and dirs in the specified path.

    :param sp: all the search params you set before the first call, and then you keep all the values here without modifying them. 
                sp.internal - pointer to internal struct (SearchParamsInternal*) - set to NULL before 1st call, don't touch on subsequent calls
                sp.path     - path where we should look for files, can contain wildcards (*, ?) or even name matching single file
                sp.attribs  - file attributes we should look for, e.g. FA_DIR for directories, or jus 0xff for any kind of dir content
                sp.addUpDir - when set to true, then '.' and '..' folders will be reported back, otherwise they will be skipped
                sp.closeNow - if you need to terminate the file search before you reach the last file, set to true and call the findFirstAndNext() function

    :param di: the found disk item will be stored here (name, size, date and time)

    :return: true if any disk item was found, false if nothing more was found and the search can be terminated (no need to call findFirstAndNext() again, it will just return false)
*/
extern "C" bool ldp_findFirstAndNext(SearchParams& sp, DiskItem& di)
{
    if(!dt) {
        dt = new DirTranslator();
    }

    return dt->findFirstAndNext(sp, di);
}

/*
    This function will create a virtual symlink from source to destination, this means that:
        - if ldp_shortToLongPath() constructs a long path, which matches source, then it's replaced with destination,
          e.g. if you create a symlink - source: /this/is/source
                                         dest:   /tmp/something
          and the ldp_shortToLongPath() creates: /this/is/source/and/rest
          then instead of that it will return    /tmp/something/and/rest
        - the symlink is virtual, that means that after your app terminates, it doesn't leave any actual links on disk
          (= no need to clean up)
        - to remove this symlink, use the same source as when the symlink was created, but use empty destination value.

    :param longPathSource: if this is found in long path, it will be replaced with longPathDest
    :param longPathDest: this is what replaces the source path
*/
extern "C" void ldp_symlink(const std::string& longPathSource, const std::string& longPathDest)
{
    if(!dt) {
        dt = new DirTranslator();
    }

    dt->symlink(longPathSource, longPathDest);
}

/*
    Function returns true if symlink for the source is present.
    :param longPathSource: if this is present, will return true.
*/
extern "C" bool ldp_gotSymlink(const std::string& longPathSource)
{
    if(!dt) {
        dt = new DirTranslator();
    }

    return dt->gotSymlink(longPathSource);
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
