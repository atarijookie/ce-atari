#ifndef DIRTRANSLATOR_H
#define DIRTRANSLATOR_H

#include <iostream>
#include <map>

#include <stdint.h>
#include "findstorage.h"
#include "defs.h"

/*
Memory usage:
-------------
Test with 1 million records, where each record was long filename with 20 characters and short filename was 12 characters (8.3) was done. 
Memory usage with 1 million records was 433 MB, so 1 record was about 433 B.
*/

class FilenameShortener;

class DirTranslator
{
    friend class TestClass;

public:
    DirTranslator();
    ~DirTranslator();

    // clear all the maps
    void clear(void);

    // convert whole path from short to long
    void shortToLongPath(const std::string& shortPath, std::string& longPath, bool refreshOnMiss);    // convert 'long_p~1\\sub_fo~1\\anothe~1' to 'long path/sub folder/another one'

    // call this for find first / find next on host file system with filename shortening already in place
    bool findFirstAndNext(SearchParams& sp, DiskItem& di);
    static void diskItemToAtariFindStorageItem(DiskItem& di, uint8_t* buf);

    void updateFileName(std::string hostPath, std::string oldFileName, std::string newFileName);

private:
    std::map<std::string, FilenameShortener *>  mapPathToShortener;
    uint32_t lastCleanUpCheck;

    FilenameShortener *getShortenerForPath(std::string path, bool createIfNotFound=true);
    FilenameShortener *createShortener(const std::string &path);
    int feedShortener(const std::string &path, FilenameShortener *fs);

    // convert single filename from long to short
    bool longToShortFilename(const std::string &longHostPath, const std::string &longFname, std::string &shortFname);

    static void splitFilenameFromPath(std::string &pathAndFile, std::string &path, std::string &file);
    static int compareSearchStringAndFilename(const std::string& searchString, const std::string& filename);

    int size(void);
    int cleanUpShortenersIfNeeded(void);

    bool setDiskItem(SearchParams& sp, DiskItem& di, const std::string &hostPath, const std::string& searchString, const std::string& lonFname, bool isDir);
    static void closeDirSetFlags(SearchParams& sp);
    static void toUpperCaseString(std::string &st);
};

#endif // DIRTRANSLATOR_H
