#ifndef FILENAMESHORTENER_H
#define FILENAMESHORTENER_H

#include <iostream>
#include <map>

// Average size for one record seems to be around 130 bytes.

#include "translatedhelper.h"

#define MAX_FILENAME_LEN    1024
#define MAX_FILEEXT_LEN     256

/*
problems:
- Include, include, inCluDe -> INCLUDE ---- mapReverseFilename musi byt multi-map
*/


class FilenameShortener
{
public:
    FilenameShortener();

    void clear(void);                                                       // clear maps - e.g. on ST restart

    bool longToShortFileName(char *longFileName, char *shortFileName);      // translates 'long file name' to 'long_f~1'
    bool shortToLongFileName(char *shortFileName, char *longFileName);      // translates 'long_f~1' to 'long file name'

    static void mergeFilenameAndExtension(char *shortFn, char *shortExt, bool extendWithSpaces, char *merged);

    static void removeSpaceExtension(char *extendedFn, char *extRemovedFn); // 'FILE    .C  ' -> 'FILE.C'
    static void extendWithSpaces(char *normalFname, char *extendedFn);      // 'FILE.C'       -> 'FILE    .C  '
    static void splitFilenameFromExtension(char *filenameWithExt, char *fileName, char *ext);

private:
    std::map<std::string, std::string>  mapFilenameWithExt;                 // for file name conversion from long to short
    std::map<std::string, std::string>  mapReverseFilename;                 // for file name conversion from short to long

    std::map<std::string, std::string> mapFilenameNoExt;                    // used by shortenName() to create unique file name with ~

    bool shortenName(char *nLong, char *nShort);
    bool shortenExtension(char *shortFileName, char *nLongExt, char *nShortExt);

    static int  strCharPos(char *str, int maxLen, char ch);
    static void replaceNonLetters(char *str);
    static void extendToLenghtWithSpaces(char *str, int len);
    static void removeTrailingSpaces(char *str);
};

#endif // FILENAMESHORTENER_H
