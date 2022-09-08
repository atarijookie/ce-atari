#ifndef DIRTRANSLATOR_H
#define DIRTRANSLATOR_H

#include <iostream>
#include <map>

#include <stdint.h>
#include "findstorage.h"

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

    // convert single filename from long to short
    bool longToShortFilename(const std::string &longHostPath, const std::string &longFname, std::string &shortFname);

    // convert whole path from short to long
    void shortToLongPath(const std::string& shortPath, std::string& longPath, bool refreshOnMiss);    // convert 'long_p~1\\sub_fo~1\\anothe~1' to 'long path/sub folder/another one'

    // call this for find first / find next for Gemdos
    bool buildGemdosFindstorageData(TFindStorage *fs, std::string hostSearchPathAndWildcards, uint8_t findAttribs, bool isRootDir);

    void updateFileName(std::string hostPath, std::string oldFileName, std::string newFileName);
    static void toUpperCaseString(std::string &st);

private:
    std::map<std::string, FilenameShortener *>  mapPathToShortener;
    uint32_t lastCleanUpCheck;

    TFindStorage    fsDirs;
    TFindStorage    fsFiles;

    FilenameShortener *getShortenerForPath(std::string path, bool createIfNotFound=true);
    FilenameShortener *createShortener(const std::string &path);
    int feedShortener(const std::string &path, FilenameShortener *fs);

    static void splitFilenameFromPath(std::string &pathAndFile, std::string &path, std::string &file);

    void appendFoundToFindStorage(std::string &hostPath, const char *searchString, TFindStorage *fs, struct dirent *de, uint8_t findAttribs);
    void appendFoundToFindStorage(std::string &hostPath, const char *searchString, TFindStorage *fs, const char *name, bool isDir, uint8_t findAttribs);
    void appendFoundToFindStorage_dirUpDirCurr(std::string &hostPath, const char *searchString, TFindStorage *fs, struct dirent *de, uint8_t findAttribs);

    static int compareSearchStringAndFilename(const char *searchString, const char *filename);

    int size(void);
    int cleanUpShortenersIfNeeded(void);
};

#endif // DIRTRANSLATOR_H
