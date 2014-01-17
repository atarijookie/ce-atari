#ifndef DIRTRANSLATOR_H
#define DIRTRANSLATOR_H

#include <iostream>
#include <map>

#include "../datatypes.h"
#include "filenameshortener.h"

typedef struct {
    BYTE *buffer;
    WORD count;             // count of items found

    WORD fsnextStart;
    WORD maxCount;          // maximum count of items that this buffer can hold
} TFindStorage;

class DirTranslator
{
public:
    DirTranslator();
    ~DirTranslator();

    // clear all the maps
    void clear(void);

    // convert single filename from long to short
    bool longToShortFilename(std::string &longHostPath, std::string &longFname, std::string &shortFname);

    // convert whole path from short to long
    void shortToLongPath(std::string &rootPath, std::string &shortPath, std::string &longPath);    // convert 'long_p~1\\sub_fo~1\\anothe~1' to 'long path/sub folder/another one'

    // call this for find first / find next for Gemdos
    bool buildGemdosFindstorageData(TFindStorage *fs, std::string hostSearchPathAndWildcards, BYTE findAttribs);

    static void attributesHostToAtari(bool isReadOnly, bool isDir, BYTE &attrAtari);
    static WORD fileTimeToAtariDate(struct tm *ptm);
    static WORD fileTimeToAtariTime(struct tm *ptm);

private:
    std::map<std::string, FilenameShortener *>  mapPathToShortener;

    FilenameShortener *createShortener(std::string &path);
    void mergeHostPaths(std::string &dest, std::string &tail);
    void splitFilenameFromPath(std::string &pathAndFile, std::string &path, std::string &file);

    void appendFoundToFindStorage(std::string &hostPath, TFindStorage *fs, struct dirent *de, BYTE findAttribs);
};

#endif // DIRTRANSLATOR_H
