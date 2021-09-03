#ifndef DIRTRANSLATOR_H
#define DIRTRANSLATOR_H

#include <iostream>
#include <map>

#include <stdint.h>

class FilenameShortener;

class TFindStorage {
public:
    TFindStorage();
    ~TFindStorage();

    int getSize(void);
    void clear(void);
    void copyDataFromOther(TFindStorage *other);

    uint32_t dta;

    uint8_t *buffer;
    uint16_t count;             // count of items found

    uint16_t maxCount;          // maximum count of items that this buffer can hold
};

class DirTranslator
{
public:
    DirTranslator();
    ~DirTranslator();

    // clear all the maps
    void clear(void);

    // convert single filename from long to short
    bool longToShortFilename(const std::string &longHostPath, const std::string &longFname, std::string &shortFname);

    // convert whole path from short to long
    void shortToLongPath(const std::string &rootPath, const std::string &shortPath, std::string &longPath);    // convert 'long_p~1\\sub_fo~1\\anothe~1' to 'long path/sub folder/another one'

    // call this for find first / find next for Gemdos
    bool buildGemdosFindstorageData(TFindStorage *fs, std::string hostSearchPathAndWildcards, uint8_t findAttribs, bool isRootDir, bool useZipdirNotFile);

    void updateFileName(std::string hostPath, std::string oldFileName, std::string newFileName);
    static void toUpperCaseString(std::string &st);

private:
    std::map<std::string, FilenameShortener *>  mapPathToShortener;

    TFindStorage    fsDirs;
    TFindStorage    fsFiles;

    FilenameShortener *getShortenerForPath(std::string path, bool createIfNotFound=true);
    FilenameShortener *createShortener(const std::string &path);
    static void splitFilenameFromPath(std::string &pathAndFile, std::string &path, std::string &file);

    void appendFoundToFindStorage(std::string &hostPath, const char *searchString, TFindStorage *fs, struct dirent *de, uint8_t findAttribs);
    void appendFoundToFindStorage(std::string &hostPath, const char *searchString, TFindStorage *fs, const char *name, bool isDir, uint8_t findAttribs);
    void appendFoundToFindStorage_dirUpDirCurr(std::string &hostPath, const char *searchString, TFindStorage *fs, struct dirent *de, uint8_t findAttribs);

    static int compareSearchStringAndFilename(const char *searchString, const char *filename);
};

#endif // DIRTRANSLATOR_H
