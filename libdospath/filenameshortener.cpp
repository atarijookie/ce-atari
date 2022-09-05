// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "filenameshortener.h"
#include "utils.h"

FilenameShortener::FilenameShortener(const std::string &path)
: allowExtUse(true)
{
    this->forWhichPath = path;
}

void FilenameShortener::clear(void)
{
    mapFilenameWithExt.clear();
    mapReverseFilename.clear();
    mapFilenameNoExt.clear();
}

bool FilenameShortener::longToShortFileName(const char *longFileName, char *shortFileName)
{
    static char fileName[MAX_FILENAME_LEN];
    static char fileExt[MAX_FILEEXT_LEN];

    memset(fileName,    0, 10);
    memset(fileExt,     0, 10);

    // find out if we do have this long file name already
    std::map<std::string, std::string>::iterator it;
    it = mapFilenameWithExt.find(longFileName);                  // try to find the string in the map

    if(it != mapFilenameWithExt.end()) {                        // if we have this fileName already, use it!
        const char *shortFileNameFromMap = it->second.c_str();
        strcpy(shortFileName, shortFileNameFromMap);
        Utils::out(LOG_DEBUG, "FilenameShortener found mapping %s <=> %s", shortFileName, longFileName);
        return true;
    }

    // divide longFileName to filename and extension
    splitFilenameFromExtension(longFileName, fileName, fileExt);

    replaceNonLetters(fileName);                                // fix bad characters
    replaceNonLetters(fileExt);

    if(strlen(fileName) == 0 && strlen(fileExt) == 0) {         // if empty, fill with '_' - this shouldn't happen
        strcpy(fileName, "________");
    }

    // shorten filename and extension if needed
    char shortName[9];
    memset(shortName, 0, 9);
    char shortExt[4];
    memset(shortExt, 0, 4);

    if((strlen(fileExt) == 0) && allowExtUse) {
        // no extension
        if(strlen(fileName) <= 8) {
            strcpy(shortName, fileName);
        } else {
            if(!shortenNameUsingExt(fileName, shortName, shortExt)) {
                Utils::out(LOG_ERROR, "FilenameShortener::longToShortFileName failed to shortenName %s", fileName);
                return false;
            }
        }
    } else {
        // there is an extension
        if(strlen(fileName) > 8) {                                  // filename too long? Shorten it.
            if(!shortenName(fileName, shortName)) {
                Utils::out(LOG_ERROR, "FilenameShortener::longToShortFileName failed to shortenName %s", fileName);
                return false;
            }
        } else {                                                    // filename not long? ok...
            strcpy(shortName, fileName);
        }

        if(strlen(fileExt) > 3) {                                  // file extension too long? Shorten it.
            if(!shortenExtension(shortName, fileExt, shortExt)) {
                Utils::out(LOG_ERROR, "FilenameShortener::longToShortFileName failed to shortenExtension %s.%s", shortName, fileExt);
                return false;
            }
        } else {                                                    // file extension not long? ok...
            strcpy(shortExt, fileExt);
        }
    }

    // create final short name
    mergeFilenameAndExtension(shortName, shortExt, false, shortFileName);

    // store the long to short filename as key - value
    std::string longFn, shortFn;
    longFn  = longFileName;
    shortFn = shortFileName;

    mapFilenameWithExt.insert( std::pair<std::string, std::string>(longFn, shortFn) );  // store this key-value pair
    mapReverseFilename.insert( std::pair<std::string, std::string>(shortFn, longFn) );  // for reverse transformation

    Utils::out(LOG_DEBUG, "FilenameShortener mapped %s <=> %s", shortFileName, longFileName);
    return true;
}

void FilenameShortener::mergeFilenameAndExtension(const char *shortFn, const char *shortExt, bool extendWithSpaces, char *merged)
{
    if(extendWithSpaces) {                                      // space extended - 'FILE    .C  '
        memset(merged, ' ', 12);                                // clear the
        merged[8]   = '.';
        merged[12]  = 0;

        int lenFn = strlen(shortFn);
        int lenEx = strlen(shortExt);

        lenFn = MIN(lenFn, 8);
        lenEx = MIN(lenEx, 3);

        memcpy(merged,     shortFn,    lenFn);                 // copy in the filename
        memcpy(merged + 9, shortExt,   lenEx);                 // copy in the file extension
    } else {                                                    // not extended - 'FILE.C'
        memset(merged, 0, 13);                                  // clear the final string first

        strncpy(merged,     shortFn,    8);                     // copy in the filename

        if(strlen(shortExt) != 0) {                             // if there is extension, merge the rest
            strcat(merged,      ".");
            strncat(merged,     shortExt,   3);                 // merge in the extension
        }
    }
}

void FilenameShortener::removeSpaceExtension(const char *extendedFn, char *extRemovedFn)
{
    char fname[12];
    char ext[4];

    splitFilenameFromExtension(extendedFn, fname, ext);     // split 'FILE    .C  ' to 'FILE    ' and 'C  '
    removeTrailingSpaces(fname);                            // convert 'FILE    ' to 'FILE'
    removeTrailingSpaces(ext);                              // convert 'C  ' to 'C'

    mergeFilenameAndExtension(fname, ext, false, extRemovedFn);
}

void FilenameShortener::extendWithSpaces(const char *normalFname, char *extendedFn)
{
    char fname[12];
    char ext[4];

    splitFilenameFromExtension(normalFname, fname, ext);     // split 'FILE.C  ' to 'FILE' and 'C'
    mergeFilenameAndExtension(fname, ext, true, extendedFn);
}

void FilenameShortener::removeTrailingSpaces(char *str)
{
    int i;
    int len = strlen(str);

    for(i=(len-1); i>=0; i--) {         // walk the string from back to front
        if(str[i] == ' ') {             // if it's a space, replace it with zero
            str[i] = 0;
        } else {                        // if it's something different, quit
            break;
        }
    }
}

const bool FilenameShortener::shortToLongFileName(const std::string& shortFileName, std::string& longFileName)
{
    if(shortFileName.size() == 0) {      // empty short path? fail
        return false;
    }

    // find out if we do have a long file name for this short filename
    std::map<std::string, std::string>::iterator it;
    it = mapReverseFilename.find(shortFileName.c_str());        // try to find the string in the map

    if(it != mapReverseFilename.end()) {                        // if we have this fileName, return it
        longFileName = it->second;
        return true;
    }

    // if short version was not found, check if the dir doesn't exist - if it does, just use it as is
    std::string fullPath = this->forWhichPath;
    Utils::mergeHostPaths(fullPath, shortFileName);     // merge path to this folder with filename inside this folder (e.g. '/mnt/shared' + 'atari' -> '/mnt/shared/atari')

    // if this file in this path exists, just add it as is - this might happen with start of path being native long and rest being short versions, e.g.:
    // for path: '/mnt/shared/ATARI/IMAGES' it's okay to use the part 'mnt' and 'shared' as long and short as they really exist there in that form, the rest needs translation
    if(Utils::fileExists(fullPath)) {
        // insert this filename to maps, so next time we won't have to investigate if the path really exists
        mapFilenameWithExt.insert( std::pair<std::string, std::string>(shortFileName, shortFileName) );  // store this key-value pair
        mapReverseFilename.insert( std::pair<std::string, std::string>(shortFileName, shortFileName) );  // for reverse transformation

        longFileName = shortFileName;
        Utils::out(LOG_DEBUG, "FilenameShortener found that shortFileName %s exists as %s", shortFileName.c_str(), fullPath.c_str());
        return true;
    }

    return false;
}

int FilenameShortener::strrCharPos(const char *str, int maxLen, char ch)
{
    int i;

    for(i = MIN((int)strlen(str), maxLen) - 1; i >= 0; i--) {       // find that char!
        if(str[i] == ch) {          // that character found?
            return i;
        }
    }

    return -1;                      // not found
}

void FilenameShortener::splitFilenameFromExtension(const char *filenameWithExt, char *fileName, char *ext)
{
    int filenameLength = strlen(filenameWithExt);

    if(filenameLength >= MAX_FILENAME_LEN) {                    // if the name is too long, use appropriate length
        filenameLength = MAX_FILENAME_LEN - 1;
    }

    // initialize strings
    memset(fileName, 0, 9);
    memset(ext, 0, 4);

    // divide longFileName to filename and extension
    int dotPos = strrCharPos(filenameWithExt, filenameLength, '.'); // find name and extension separator

    if(dotPos == -1) {                                          // not found?
        strncpy(fileName, filenameWithExt, filenameLength + 1); // copy whole name to filename
        ext[0] = 0;                                             // store no extension
    } else {                                                    // separator found?
        strncpy(fileName, filenameWithExt, dotPos);             // copy in the fileName

        int extLen = strlen(filenameWithExt + dotPos + 1);      // find out the length of extension

        if(extLen >= MAX_FILEEXT_LEN) {                         // if the extension would be too long, shorten the length
            extLen = MAX_FILEEXT_LEN - 1;
        }

        strncpy(ext, filenameWithExt + dotPos + 1, extLen + 1);    // copy in the extension
    }
}

const bool FilenameShortener::shortenName(const char *nLong, char *nShort)
{
    int ind = 1;
    char num[12], newName[12];
    std::map<std::string, std::string>::iterator it;

    while(ind < 32000) {
        sprintf(num, "~%d", ind);                       // create numerical end, e.g. '~1'

        strncpy(newName, nLong, 11);                        // get fist half, e.g. 'filena'
        strcpy(newName + 8 - strlen(num), num);             // create new name, e.g. 'filena~1'

        it = mapFilenameNoExt.find(newName);                // try to find the string in the map

        if(it == mapFilenameNoExt.end()) {                 // if we don't have this fileName already, use it!
            strcpy(nShort,newName);

            std::string longN, shortN;
            longN  = nLong;
            shortN = nShort;

            mapFilenameNoExt.insert( std::pair<std::string, std::string>(shortN, longN) );    // store this key-value pair

            return true;
        }

        ind++;                                             // if we have that file, try next index
    }

    return false;                                          // failed
}

const bool FilenameShortener::shortenExtension(const char *shortFileName, const char *nLongExt, char *nShortExt)
{
     int ind = 1;

     // first try to simply cut off the rest
     char newName1[13];
     mergeFilenameAndExtension(shortFileName, nLongExt, false, newName1);

     // if we don't have that SHORT filename in the list, this cut extension will work just fine
     std::map<std::string, std::string>::iterator it;
     it = mapReverseFilename.find(newName1);                // try to find the string in the map

     if(it == mapReverseFilename.end()) {                   // if we don't have this fileName already, use it!
         strncpy(nShortExt, nLongExt, 3);                   // store the extension string
         nShortExt[3] = 0;

         return true;
     }

    // if we just can't cut the extension, try to shorten it with number
    char newExt[4];
    char num[4];

    while(ind < 1000) {
        if(ind < 100) {                                     // according to the size of the number
            snprintf(num, sizeof(num), "~%d", ind);         // numbers 1 .. 99 -> ~1 ~99
        } else {
            snprintf(num, sizeof(num), "%d", ind);          // numbers >= 100  -> 100 101 ...
        }

        strncpy(newExt, nLongExt, 3);                       // get fist half, e.g. 'JPE'
        strcpy(newExt + 3 - strlen(num), num);              // create new extension, e.g. 'J~1'
        newExt[3] = 0;

        mergeFilenameAndExtension(shortFileName, newExt, false, newName1);

        it = mapReverseFilename.find(newName1);             // try to find the string in the map

        if(it == mapReverseFilename.end()) {                // if we don't have this fileName already, use it!
            strncpy(nShortExt, newExt, 3);                  // store the extension string
            nShortExt[3] = 0;

            return true;
        }

        ind++;                                              // if we have that file, try next index
    }

    return false;                                           // this should never happen
}

const bool FilenameShortener::shortenNameUsingExt(const char *fileName, char *shortName, char *shortExt)
{
    // use 8 first characters as filename
    memcpy(shortName, fileName, 8);
	shortName[8] = '\0';
    // and following characters as extension !
	const char *longExt = fileName + 8;
	// skip spaces
	while(longExt[0] == '_' && longExt[1] != '\0') longExt++;
    return shortenExtension(shortName, longExt, shortExt);
}

void FilenameShortener::replaceNonLetters(char *str)
{
    int i, len, j;
    len = strlen(str);

    const char *allowed = "!#$%&'()~^@-_{}";

    for(i=0; i<len; i++) {
        if(str[i] >= '0' && str[i] <= '9') {    // numbers are OK
            continue;
        }

        if(str[i] >= 'A' && str[i] <='Z') {     // capital letters are OK
            continue;
        }

        if(str[i] >= 'a' && str[i] <='z') {     // convert small letters to capital
            str[i] = str[i] - 32;
            continue;
        }

        bool isAllowed = false;
        for(j=0; allowed[j] != '\0'; j++) {        // try to find this char in allowed characters array
            if(str[i] == allowed[j]) {
                isAllowed = true;
                break;
            }
        }

        if(isAllowed) {                         // it's allowed char, let it be
            continue;
        }

        str[i] = '_';                           // not allowed char, fix it
     }
 }

void FilenameShortener::extendToLenghtWithSpaces(char *str, int len)
{
    int i;

    for(i=0; i<len; i++) {          // replace all zeros with space
        if(str[i] == 0) {
            str[i] = ' ';
        }
    }

    str[len] = 0;                   // terminate with zero
}

// find oldFileName in long file names and replace it with newFileName
void FilenameShortener::updateLongFileName(std::string oldFileName, std::string newFileName)
{
    // find out if we do have this long file name already
    std::map<std::string, std::string>::iterator it;
    it = mapFilenameWithExt.find(oldFileName);      // try to find the string in the map

    if(it == mapFilenameWithExt.end()) {            // didn't find this old file name? then quit
        return;
    }

    // found the old file name in the long-to-short map

    std::string shortFileName = it->second;         // get the matching short file name (but make a copy of it, we're going to delete this item from std::map)

    // For associative containers of type map and multimap the keys are immutable, so we'll have to
    // remove this key and place a new one in the map instead of just modifying the key.

    mapFilenameWithExt.erase(it);                   // erase item by iterator
    mapFilenameWithExt.insert( std::pair<std::string, std::string>(newFileName, shortFileName) );  // store this key-value pair

    // We still need to find and alter the reverse map, but we can just replace the value there instead of doing delete-insert

    it = mapReverseFilename.find(shortFileName);    // try to find short file name in the reverse map

    if(it == mapReverseFilename.end()) {            // didn't find the short file name? quit
        return;
    }

    it->second = newFileName;                       // in the short-to-long map replace the oldFileName with newFileName
}
