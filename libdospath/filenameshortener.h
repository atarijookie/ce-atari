#ifndef FILENAMESHORTENER_H
#define FILENAMESHORTENER_H

#include <iostream>
#include <map>

// Average size for one record seems to be around 130 bytes.

#define MAX_FILENAME_LEN    1024
#define MAX_FILEEXT_LEN     256

/*
problems:
- Include, include, inCluDe -> INCLUDE ---- mapReverseFilename musi byt multi-map
*/


class FilenameShortener
{
    friend class TestClass;

public:
    FilenameShortener(const std::string &path);

    void clear(void);                                                       // clear maps - e.g. on ST restart

    bool longToShortFileName(const char *longFileName, char *shortFileName);      // translates 'long file name' to 'long_f~1'
    const bool shortToLongFileName(const std::string& shortFileName, std::string& longFileName);      // translates 'long_f~1' to 'long file name'

    void updateLongFileName(std::string oldFileName, std::string newFileName);    // find oldFileName in long file names and replace it with newFileName

    static void mergeFilenameAndExtension(const std::string& shortFn, const std::string& shortExt, bool extendWithSpaces, std::string& merged);

    static void extendWithSpaces(const char *normalFname, char *extendedFn);      // 'FILE.C'       -> 'FILE    .C  '

private:
    std::string forWhichPath;

    std::map<std::string, std::string>  mapFilenameWithExt;                 // for file name conversion from long to short
    std::map<std::string, std::string>  mapReverseFilename;                 // for file name conversion from short to long

    std::map<std::string, std::string> mapFilenameNoExt;                    // used by shortenName() to create unique file name with ~
    bool allowExtUse;          // Allow use of Extension for shortening (if file without extension)

    const bool shortenName(const char *nLong, char *nShort);
    const bool shortenExtension(const char *shortFileName, const char *nLongExt, char *nShortExt);
    const bool shortenNameUsingExt(const char *fileName, char *shortName, char *shortExt);

    static void replaceNonLetters(std::string &str);
};

#endif // FILENAMESHORTENER_H
