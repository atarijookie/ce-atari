// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <stdio.h>
#include <string.h>

#include <fnmatch.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <linux/msdos_fs.h>

#include "defs.h"
#include "utils.h"
#include "dirtranslator.h"
#include "filenameshortener.h"

#define GEMDOS_FILE_MAXSIZE (2147483647)
// TOS 1.x cannot display size with more than 8 digits
//#define GEMDOS_FILE_MAXSIZE (100*1000*1000-1)

DirTranslator::DirTranslator()
{
    fsDirs.count    = 0;
    fsFiles.count   = 0;
}

DirTranslator::~DirTranslator()
{
    clear();
}

void DirTranslator::clear(void)
{
    std::map<std::string, FilenameShortener *>::iterator it;

    for(it = mapPathToShortener.begin(); it != mapPathToShortener.end(); ++it) {        // go through the map
        FilenameShortener *fs = it->second;                                             // get the filename shortener
        delete fs;                                                                      // and delete it
    }

    mapPathToShortener.clear();
}

void DirTranslator::updateFileName(std::string hostPath, std::string oldFileName, std::string newFileName)
{
    FilenameShortener *fs = getShortenerForPath(hostPath, false);   // try to find a shortener for our path, but don't create it if not found

    if(fs) {        // if existing shortener was found, update filename
        fs->updateLongFileName(oldFileName, newFileName);
    }
}

void DirTranslator::shortToLongPath(const std::string &shortPath, std::string &longPath)
{
    std::string inPath = shortPath;
    Utils::toHostSeparators(inPath);                    // from dos/atari separators to host separators only

    std::vector<std::string> shortPathParts;
    Utils::splitString(inPath, '/', shortPathParts);    // explode string to individual parts

    unsigned int i, partsCount = shortPathParts.size();
    std::vector<std::string> longPathParts;

    // now convert all the short names to long names
    for(i=0; i<partsCount; i++) {
        std::string subPath;
        Utils::joinStrings(shortPathParts, subPath, i);             // create just sub-path from the whole path (e.g. just '/mnt/shared' from '/mnt/shared/images/games')

        FilenameShortener *fs = getShortenerForPath(subPath);       // find or create shortener for this path

        std::string longName;

        if(fs->shortToLongFileName(shortPathParts[i], longName)) {  // try to convert the name
            longPathParts.push_back(std::string(longName));         // if there was a long version of the file name, replace the short one
        } else {
            longPathParts.push_back(shortPathParts[i]);             // failed to find long path, use the original in this place
            Utils::out(LOG_DEBUG, "DirTranslator::shortToLongPath - shortToLongFileName() failed for short name: %s , subPath=%s", shortPathParts[i].c_str(), subPath.c_str());
        }
    }

    Utils::joinStrings(longPathParts, longPath);        // join all output parts to one long path
}

bool DirTranslator::longToShortFilename(const std::string &longHostPath, const std::string &longFname, std::string &shortFname)
{
    FilenameShortener *fs = getShortenerForPath(longHostPath);
    return fs->longToShortFileName(longFname, shortFname);          // try to convert the name from long to short
}

FilenameShortener *DirTranslator::getShortenerForPath(std::string path, bool createIfNotFound)
{
    // remove trailing '/' if it's present and if this is not the root path (just a single '/' here)
    if(path.size() > 1 && path[path.size() - 1] == HOSTPATH_SEPAR_CHAR) {
        path.erase(path.size() - 1, 1);
    }

    std::map<std::string, FilenameShortener *>::iterator it;
    it = mapPathToShortener.find(path);     // find the shortener for that host path
    FilenameShortener *fs;

    if(it != mapPathToShortener.end()) {            // already got the shortener
        //Utils::out(LOG_DEBUG, "DirTranslator::getShortenerForPath - shortener for %s found", path.c_str());
        fs = it->second;
    } else {                                        // don't have the shortener yet
        if(createIfNotFound) {  // should create?
            Utils::out(LOG_DEBUG, "DirTranslator::getShortenerForPath - shortener for %s NOT found, creating", path.c_str());
            fs = createShortener(path);
        } else {                // shoul NOT create?
            //Utils::out(LOG_DEBUG, "DirTranslator::getShortenerForPath - shortener for %s NOT found, returning NULL", path.c_str());
            fs = NULL;
        }
    }
    return fs;
}

FilenameShortener *DirTranslator::createShortener(const std::string &path)
{
    FilenameShortener *fs = new FilenameShortener(path);
    mapPathToShortener.insert( std::pair<std::string, FilenameShortener *>(path, fs) );

    DIR *dir = opendir(path.c_str());                       // try to open the dir

    if(dir == NULL) {                                               // not found?
        return fs;
    }

    while(1) {                                                      // while there are more files, store them
        struct dirent *de = readdir(dir);                           // read the next directory entry

        if(de == NULL) {                                            // no more entries?
            break;
        }

        if(de->d_type != DT_DIR && de->d_type != DT_REG) {          // not  a file, not a directory?
            Utils::out(LOG_DEBUG, "TranslatedDisk::createShortener -- skipped %s because the type %d is not supported!", de->d_name, de->d_type);
            continue;
        }

        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        std::string shortName;
        fs->longToShortFileName(std::string(de->d_name), shortName);
    }

    closedir(dir);
    return fs;
}

bool DirTranslator::buildGemdosFindstorageData(TFindStorage *fs, std::string hostSearchPathAndWildcards, uint8_t findAttribs, bool isRootDir)
{
    std::string hostPath, searchString;

    // initialize partial find results (dirs and files separately)
    fsDirs.clear();
    fsFiles.clear();

    Utils::splitFilenameFromPath(hostSearchPathAndWildcards, hostPath, searchString);

    // here we handle the special case where the search string contains no wildcard but
    // matches one particular file
    // see https://github.com/atarijookie/ce-atari/issues/190
    if (searchString.find('*') == std::string::npos && searchString.find('?') == std::string::npos) {
        struct stat attr;
        Utils::out(LOG_DEBUG, "DirTranslator::buildGemdosFindstorageData - no wildcard in %s, checking %s", searchString.c_str(), hostSearchPathAndWildcards.c_str());
        if (stat(hostSearchPathAndWildcards.c_str(), &attr) == 0) {
            appendFoundToFindStorage(hostPath, "*.*", fs, searchString.c_str(), S_ISDIR(attr.st_mode), findAttribs);
        }
    } else {
        toUpperCaseString(searchString);

        // then build the found files list
        DIR *dir = opendir(hostPath.c_str());                           // try to open the dir

        if(dir == NULL) {                                               // not found?
            Utils::out(LOG_DEBUG, "DirTranslator::buildGemdosFindstorageData - opendir(\"%s\") FAILED", hostPath.c_str());
            return false;
        }

        // initialize find storage in case anything goes bad
        fs->clear();

        while(fs->count < fs->maxCount) {        // avoid buffer overflow
            struct dirent *de = readdir(dir);    // read the next directory entry

            if(de == NULL) {                     // no more entries?
                break;
            }

            if(de->d_type != DT_DIR && de->d_type != DT_REG) {  // not a file, not a directory?
                Utils::out(LOG_DEBUG, "TranslatedDisk::onFsfirst -- skipped %s because the type %d is not supported!", de->d_name, de->d_type);
                continue;
            }

            std::string longFname = de->d_name;

            // special handling of '.' and '..'
            if(longFname == "." || longFname == "..") {
                if((isRootDir)||((findAttribs&FA_DIR)==0)) {    // for root dir or when no subdirs are requested (FA_DIR) - don't add '.' or '..'
                    continue;
                } else {                                        // for non-root dir                                       - must add '.' or '..' (TOS does this, and it makes the TOS dir copying work)
                    appendFoundToFindStorage_dirUpDirCurr(hostPath, searchString.c_str(), fs, de, findAttribs);
                    continue;
                }
            }

            // finally append to the find storage
            appendFoundToFindStorage(hostPath, searchString.c_str(), fs, de, findAttribs);
        }

        closedir(dir);
    }

    // now in the end merge found dirs and found files
    int dirsSize    = fsDirs.count  * 23;
    int filesSize   = fsFiles.count * 23;

    memcpy(fs->buffer,              fsDirs.buffer,  dirsSize);          // copy to total find search - first the dirs
    memcpy(fs->buffer + dirsSize,   fsFiles.buffer, filesSize);         // copy to total find search - then the files

    return true;
}

void DirTranslator::appendFoundToFindStorage(std::string &hostPath, const char *searchString, TFindStorage *fs, struct dirent *de, uint8_t findAttribs)
{
    // TODO: verify on ST that the find attributes work like this

    // TOS 1.04 searches with findAttribs set to 0x10, that's INCLUDE DIRs

    // first verify if the file attributes are OK
//    if((found->dwFileAttributes & FILE_ATTRIBUTE_READONLY)!=0   && (findAttribs & FA_READONLY)==0)  // is read only, but not searching for that
//        return;

// attribute not supported on linux
//    if((found->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)!=0     && (findAttribs & FA_HIDDEN)==0)    // is hidden, but not searching for that
//        return;

// attribute not supported on linux
//    if((found->dwFileAttributes & FILE_ATTRIBUTE_SYSTEM)!=0     && (findAttribs & FA_SYSTEM)==0)    // is system, but not searching for that
//        return;

    bool isDir = (de->d_type == DT_DIR);
    if(isDir  && (findAttribs & FA_DIR)==0) {      // is dir, but not searching for that
        return;
    }

//    // this one is now disabled as on Win almost everything has archive bit set, and thus TOS didn't show any files
//    if((found->dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)!=0    && (findAttribs & FA_ARCHIVE)==0)   // is archive, but not searching for that
//        return;
    appendFoundToFindStorage(hostPath, searchString, fs, de->d_name, isDir, findAttribs);
}

void DirTranslator::appendFoundToFindStorage(std::string &hostPath, const char *searchString, TFindStorage *fs, const char *name, bool isDir, uint8_t findAttribs)
{
    // TODO: do support for checking the READ ONLY flag on linux
    bool isReadOnly = false;
    //--------
    // add this file
    TFindStorage *fsPart = isDir ? &fsDirs : &fsFiles;          // get the pointer to partial find storage to separate dirs from files when searching

    uint32_t addr  = fsPart->count * 23;                           // calculate offset
    uint8_t *buf   = &(fsPart->buffer[addr]);                      // and get pointer to this location

    uint8_t atariAttribs;                                          // convert host to atari attribs
    Utils::attributesHostToAtari(isReadOnly, isDir, atariAttribs);

    if(name[0] == '.') atariAttribs |= FA_HIDDEN;       // enforce Mac/Unix convention of hidding files startings with '.'

    std::string fullEntryPath   = hostPath;
    std::string longFname       = name;
    Utils::mergeHostPaths(fullEntryPath, longFname);

    int res;
    struct stat attr;
    tm *timestr;
    std::string shortFname;

    res = stat(fullEntryPath.c_str(), &attr);                   // get the file status

    if(res != 0) {
        Utils::out(LOG_ERROR, "TranslatedDisk::appendFoundToFindStorage -- stat(%s) failed, errno %d", fullEntryPath.c_str(), errno);
        return;
    }

    timestr = localtime(&attr.st_mtime);                        // convert time_t to tm structure

    res = longToShortFilename(hostPath, longFname, shortFname); // convert long to short filename
    if(!res) {
        return;
    }

    uint16_t atariTime = Utils::fileTimeToAtariTime(timestr);
    uint16_t atariDate = Utils::fileTimeToAtariDate(timestr);

    // check the current name against searchString using fnmatch
    if (compareSearchStringAndFilename(searchString, shortFname.c_str()) != 0) {
        Utils::out(LOG_ERROR, "TranslatedDisk::appendFoundToFindStorage -- %s - %s does not match pattern %s", fullEntryPath.c_str(), shortFname.c_str(), searchString);
        return; // not matching? quit
    }

    // get MS-DOS VFAT attributes
    {
        int fd = open(fullEntryPath.c_str(), O_RDONLY);
        if(fd >= 0) {
            __u32 dosattrs = 0;
            if (ioctl(fd, FAT_IOCTL_GET_ATTRIBUTES, &dosattrs) >= 0) {
                if(dosattrs & ATTR_RO) atariAttribs |= FA_READONLY;
                if(dosattrs & ATTR_HIDDEN) atariAttribs |= FA_HIDDEN;
                if(dosattrs & ATTR_SYS) atariAttribs |= FA_SYSTEM;
                //if(dosattrs & ATTR_ARCH) atariAttribs |= FA_ARCHIVE;
            } else {
                // it will fail if the underlying FileSystem is not FAT
                //Utils::out(LOG_ERROR, "TranslatedDisk::appendFoundToFindStorage -- ioctl(%s, FAT_IOCTL_GET_ATTRIBUTES) failed errno %d", fullEntryPath.c_str(), errno);
            }
            close(fd);
        } else {
            Utils::out(LOG_ERROR, "TranslatedDisk::appendFoundToFindStorage -- open(%s) failed, errno %d", fullEntryPath.c_str(), errno);
        }
    }

    // GEMDOS File Attributes
    buf[0] = atariAttribs;

    // GEMDOS Time
    buf[1] = atariTime >> 8;
    buf[2] = atariTime &  0xff;

    // GEMDOS Date
    buf[3] = atariDate >> 8;
    buf[4] = atariDate &  0xff;

    // File Length
    uint32_t size;
    if(attr.st_size > GEMDOS_FILE_MAXSIZE) {
        size = GEMDOS_FILE_MAXSIZE;
    } else {
        size = (uint32_t)attr.st_size;
    }
    buf[5] = (size >>  24) & 0xff;
    buf[6] = (size >>  16) & 0xff;
    buf[7] = (size >>   8) & 0xff;
    buf[8] = size & 0xff;

    // Filename -- d_fname[14]
    memset(&buf[9], 0, 14);                                // first clear the mem
//  strncpy((char *) &buf[9], shortFnameExtended, 14);     // copy the filename - 'FILE    .C  '
    strncpy((char *) &buf[9], shortFname.c_str(), 14);     // copy the filename - 'FILE.C'

    fsPart->count++;                                                // increase partial count
    fs->count++;                                                    // increase total count
}

void DirTranslator::appendFoundToFindStorage_dirUpDirCurr(std::string &hostPath, const char *searchString, TFindStorage *fs, struct dirent *de, uint8_t findAttribs)
{
    TFindStorage *fsPart = &fsDirs;                     // get the pointer to partial find storage to separate dirs from files when searching

    // add this file
    uint32_t addr  = fsPart->count * 23;                   // calculate offset
    uint8_t *buf   = &(fsPart->buffer[addr]);              // and get pointer to this location

    uint8_t atariAttribs;                                  // convert host to atari attribs
    Utils::attributesHostToAtari(false, true, atariAttribs);

    std::string fullEntryPath   = hostPath;
    std::string longFname       = de->d_name;
    Utils::mergeHostPaths(fullEntryPath, longFname);

    int res;
    struct stat attr;
    tm *timestr;
    std::string shortFname;

    res = stat(fullEntryPath.c_str(), &attr);                   // get the file status

    if(res != 0) {
        Utils::out(LOG_ERROR, "TranslatedDisk::appendFoundToFindStorage -- stat() failed, errno %d", errno);
        return;
    }

    timestr = localtime(&attr.st_mtime);                        // convert time_t to tm structure

    uint16_t atariTime = Utils::fileTimeToAtariTime(timestr);
    uint16_t atariDate = Utils::fileTimeToAtariDate(timestr);

    // check the current name against searchString using fnmatch
    int ires = compareSearchStringAndFilename(searchString, shortFname.c_str());

    if(ires != 0) {     // not matching? quit
        return;
    }

    // GEMDOS File Attributes
    buf[0] = atariAttribs;

    // GEMDOS Time
    buf[1] = atariTime >> 8;
    buf[2] = atariTime &  0xff;

    // GEMDOS Date
    buf[3] = atariDate >> 8;
    buf[4] = atariDate &  0xff;

    // File Length
    buf[5] = 0;
    buf[6] = 0;
    buf[7] = 0;
    buf[8] = 0;

    // Filename -- d_fname[14]
    memset(&buf[9], 0, 14);                                         // first clear the mem
    strncpy((char *) &buf[9], de->d_name, 14);                      // copy the filename - '.' or '..'

    fsPart->count++;                                                // increment partial count
    fs->count++;                                                    // increment total count
}

void DirTranslator::toUpperCaseString(std::string &st)
{
    int i, len;
    len = st.length();

    for(i=0; i<len; i++) {
        st[i] = toupper(st[i]);
    }
}

/// Return 0 for a match, -1 for no match
int DirTranslator::compareSearchStringAndFilename(const char *searchString, const char *filename)
{
    std::string sSearchString = searchString;
    std::string ss1, ss2;
    Utils::splitFilenameFromExt(sSearchString, ss1, ss2);

    std::string sFilename = filename;
    std::string fn1, fn2;
    Utils::splitFilenameFromExt(sFilename, fn1, fn2);

    // check if filename matches
    int ires = fnmatch(ss1.c_str(), fn1.c_str(), FNM_PATHNAME);

    if(ires != 0) {
        return -1;
    }

    // check if extension matches
    ires = fnmatch(ss2.c_str(), fn2.c_str(), FNM_PATHNAME);

    if(ires != 0) {
        return -1;
    }

    return 0;
}

TFindStorage::TFindStorage()
{
    buffer = new uint8_t[getSize()];
    clear();
}

TFindStorage::~TFindStorage()
{
    delete []buffer;
}

void TFindStorage::clear(void)
{
    maxCount    = getSize() / 23;
    count       = 0;
    dta         = 0;
}

int TFindStorage::getSize(void)
{
    return (1024*1024);
}

void TFindStorage::copyDataFromOther(TFindStorage *other)
{
    dta     = other->dta;
    count   = other->count;
    memcpy(buffer, other->buffer, count * 23);
}
