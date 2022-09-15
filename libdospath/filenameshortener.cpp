// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <stdio.h>
#include <string.h>

#include "libdospath.h"
#include "filenameshortener.h"
#include "utilslib.h"

FilenameShortener::FilenameShortener(const std::string &path)
{
    forWhichPath = path;
    lastAccessTime = UtilsLib::getCurrentMs();
}

FilenameShortener::~FilenameShortener()
{
    clear();
}

void FilenameShortener::touch(void)
{
    // touch this shortener and mark it as used just now
    lastAccessTime = UtilsLib::getCurrentMs();
}

uint32_t FilenameShortener::getLastAccessTime(void)
{
    return lastAccessTime;
}

void FilenameShortener::clear(void)
{
    mapFilenameWithExt.clear();
    mapReverseFilename.clear();
    mapFilenameNoExt.clear();
}

bool FilenameShortener::longToShortFileName(const std::string& longFileName, std::string& shortFileName, bool* createdNotFound)
{
    touch();        // mark that this shortener has just been used

    // find out if we do have this long file name already
    std::map<std::string, std::string>::iterator it;
    it = mapFilenameWithExt.find(longFileName);                  // try to find the string in the map

    if(it != mapFilenameWithExt.end()) {                        // if we have this fileName already, use it!
        shortFileName = it->second;
        UtilsLib::out(LOG_DEBUG, "FilenameShortener found mapping %s <=> %s", shortFileName.c_str(), longFileName.c_str());

        if(createdNotFound) 
            *createdNotFound = false;       // false == translation was found

        return true;
    }

    if(createdNotFound) 
        *createdNotFound = true;            // true == translation was created

    // divide longFileName to filename and extension
    std::string fileName, fileExt;
    UtilsLib::splitFilenameFromExt(longFileName, fileName, fileExt);

    replaceNonLettersAndToUpperCase(fileName);  // fix bad characters and capitalize chars
    replaceNonLettersAndToUpperCase(fileExt);

    if(fileName.empty() && fileExt.empty()) {   // if empty, fill with '_' - this shouldn't happen
        fileName = "________";
    }

    // shorten filename and extension if needed
    std::string shortName, shortExt;

    if(!shortenName(fileName, shortName)) {
        UtilsLib::out(LOG_ERROR, "FilenameShortener::longToShortFileName failed to shortenName %s", fileName.c_str());
        return false;
    }

    shortenExtension(fileExt, shortExt);

    // create final short name
    mergeFilenameAndExtension(shortName, shortExt, false, shortFileName);

    // store the long to short filename as key - value
    mapFilenameWithExt.insert( std::pair<std::string, std::string>(longFileName, shortFileName) );  // store this key-value pair
    mapReverseFilename.insert( std::pair<std::string, std::string>(shortFileName, longFileName) );  // for reverse transformation

    UtilsLib::out(LOG_DEBUG, "FilenameShortener mapped %s <=> %s", shortFileName.c_str(), longFileName.c_str());
    return true;
}

void FilenameShortener::mergeFilenameAndExtension(const std::string& shortFn, const std::string& shortExt, bool extendWithSpaces, std::string& merged)
{
    std::string shortFn2 = shortFn, shortExt2 = shortExt;       // make copies so we don't modify originals

    if(shortFn2.size() > 8)             // filename too long? shorten
        shortFn2.resize(8);

    if(shortExt2.size() > 3)            // file ext too long? shorten
        shortExt2.resize(3);

    if(extendWithSpaces) {              // if should extend
        if(shortFn2.size() < 8)         // filename too short? extend
            shortFn2.resize(8, ' ');

        if(shortExt2.size() < 3)        // extension too short? extend
            shortExt2.resize(3, ' ');
    }

    merged = shortFn2;                  // put in the filename
    
    if(shortExt2.size() > 0)            // if got extension, append extension
        merged += std::string(".") + shortExt2;
}

bool FilenameShortener::shortToLongFileName(const std::string& shortFileName, std::string& longFileName)
{
    touch();        // mark that this shortener has just been used

    if(shortFileName.size() == 0) {      // empty short path? fail
        return false;
    }

    // find out if we do have a long file name for this short filename
    std::map<std::string, std::string>::iterator it;
    it = mapReverseFilename.find(shortFileName);                // try to find the string in the map

    if(it != mapReverseFilename.end()) {                        // if we have this fileName, return it
        longFileName = it->second;
        return true;
    }

    // if short version was not found, check if the dir doesn't exist - if it does, just use it as is
    std::string fullPath = this->forWhichPath;
    UtilsLib::mergeHostPaths(fullPath, shortFileName);     // merge path to this folder with filename inside this folder (e.g. '/mnt/shared' + 'atari' -> '/mnt/shared/atari')

    // if this file in this path exists, just add it as is - this might happen with start of path being native long and rest being short versions, e.g.:
    // for path: '/mnt/shared/ATARI/IMAGES' it's okay to use the part 'mnt' and 'shared' as long and short as they really exist there in that form, the rest needs translation
    if(UtilsLib::fileExists(fullPath)) {
        // insert this filename to maps, so next time we won't have to investigate if the path really exists
        // NOTE: having the shortFileName as KEY and also as VALUE is OK in this case!
        mapFilenameWithExt.insert( std::pair<std::string, std::string>(shortFileName, shortFileName) );  // store this key-value pair
        mapReverseFilename.insert( std::pair<std::string, std::string>(shortFileName, shortFileName) );  // for reverse transformation

        longFileName = shortFileName;
        UtilsLib::out(LOG_DEBUG, "FilenameShortener found that shortFileName %s exists as %s", shortFileName.c_str(), fullPath.c_str());
        return true;
    }

    return false;
}

const bool FilenameShortener::shortenName(const std::string& nLong, std::string& nShort)
{
    if(nLong.size() <= 8) {     // long version is max 8 chars? use it, this is OK
        nShort = nLong;
        return true;
    }

    int ind = 1;
    char num[12];
    std::map<std::string, std::string>::iterator it;

    while(ind < 1000) {
        sprintf(num, "~%d", ind);                       // create numerical end, e.g. '~1'
        int remainingSize = 8 - strlen(num);            // what is the remaining size of name, if we'll use 'num' at the end
        
        std::string newName = nLong;
        newName.resize(remainingSize);                  // shorten to remaining size
        newName += num;                                 // append number

        it = mapFilenameNoExt.find(newName);            // try to find the string in the map

        if(it == mapFilenameNoExt.end()) {              // if we don't have this fileName already, use it!
            nShort = newName;
            mapFilenameNoExt.insert( std::pair<std::string, std::string>(nShort, nLong) );    // store this key-value pair
            return true;
        }

        ind++;                                             // if we have that file, try next index
    }

    return false;                                          // failed
}

const void FilenameShortener::shortenExtension(const std::string& nLongExt, std::string& nShortExt)
{
    nShortExt = nLongExt;

    if(nShortExt.size() > 3) {      // if extension too long, cut it to 3 chars
        nShortExt.resize(3);
    }
}

void FilenameShortener::replaceNonLettersAndToUpperCase(std::string &str)
{
    int i, len, j;
    len = str.size();

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

// find oldFileName in long file names and replace it with newFileName
void FilenameShortener::updateLongFileName(std::string oldFileName, std::string newFileName)
{
    touch();        // mark that this shortener has just been used

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

int FilenameShortener::size(void)
{
    // return how many items we got in this filename shortener
    return mapFilenameWithExt.size();
}
