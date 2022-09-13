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

#include <algorithm>    // std::sort
#include <vector>       // std::vector

#include <sys/ioctl.h>
#include <linux/msdos_fs.h>

#include "defs.h"
#include "utils.h"
#include "dirtranslator.h"
#include "findstorage.h"
#include "filenameshortener.h"

// This value sets the maximum count of shortened names we will have in our shorteners, to limit ram usage.
// With 1 name being around 433 B the limit of 100k names means about 43 MB of RAM used before cleanup happens.
int MAX_NAMES_IN_TRANSLATOR = 100000;

#define GEMDOS_FILE_MAXSIZE (2147483647)
// TOS 1.x cannot display size with more than 8 digits
//#define GEMDOS_FILE_MAXSIZE (100*1000*1000-1)

DirTranslator::DirTranslator()
{
    lastCleanUpCheck = Utils::getCurrentMs();
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

void DirTranslator::shortToLongPath(const std::string& shortPath, std::string& longPath, bool refreshOnMiss)
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
        bool ok;

        ok = fs->shortToLongFileName(shortPathParts[i], longName);  // try to convert short-to-long filename

        if(!ok && refreshOnMiss) {          // if conversion from short-to-long failed, and we should do a refresh on dict-miss (cache-miss)
            Utils::out(LOG_DEBUG, "DirTranslator::shortToLongPath - shortToLongFileName() failed for short name: %s , subPath=%s, but will do a refresh now!", shortPathParts[i].c_str(), subPath.c_str());

            feedShortener(subPath, fs);     // feed shortener with possibly new filenames
            ok = fs->shortToLongFileName(shortPathParts[i], longName);      // 2nd attempt to convert short-to-long filename (after refresh)
        }

        if(ok) {        // filename translation from short-to-long was successful - replace the short part with long part
            longPathParts.push_back(std::string(longName));
        } else {        // failed to find long path, use the original in this place
            longPathParts.push_back(shortPathParts[i]);
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

    if(fs) {        // if valid shortener was found or created, touch it
        fs->touch();
    }

    return fs;
}

FilenameShortener *DirTranslator::createShortener(const std::string &path)
{
    // do clean up if going above maximum count, then create shortener for specified path and feed it with dir content
    cleanUpShortenersIfNeeded();

    FilenameShortener *fs = new FilenameShortener(path);
    mapPathToShortener.insert( std::pair<std::string, FilenameShortener *>(path, fs) );
    
    feedShortener(path, fs);    // feed the shortener to initialize it with dir content

    return fs;
}

int DirTranslator::feedShortener(const std::string &path, FilenameShortener *fs)
{
    // Feed the shortener with content of the specified dir. 
    // It's used when creating shornerer (initial feeding) and when refreshing shortener with new dir content

    int createdCount = 0;       // count of items that were created. can be used to see if new items have been added to shortener

    DIR *dir = opendir(path.c_str());               // try to open the dir

    if(dir == NULL) {                               // not found?
        return createdCount;
    }

    while(1) {                                      // while there are more files, store them
        struct dirent *de = readdir(dir);           // read the next directory entry

        if(de == NULL) {                            // no more entries?
            break;
        }

        if(de->d_type != DT_DIR && de->d_type != DT_REG) {          // not  a file, not a directory?
            Utils::out(LOG_DEBUG, "TranslatedDisk::feedShortener -- skipped %s because the type %d is not supported!", de->d_name, de->d_type);
            continue;
        }

        if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;

        bool ok, created;
        std::string shortName;
        ok = fs->longToShortFileName(std::string(de->d_name), shortName, &created);

        if(ok && created) {     // if was able to come up with short name and it was created (not just found in dict), increment counter
            createdCount++;
        }
    }

    closedir(dir);
    return createdCount;
}

void DirTranslator::toUpperCaseString(std::string &st)
{
    int i, len;
    len = st.length();

    for(i=0; i<len; i++) {
        st[i] = toupper(st[i]);
    }
}

// Return 0 for a match, -1 for no match
int DirTranslator::compareSearchStringAndFilename(const std::string& searchString, const std::string& filename)
{
    std::string ss1, ss2;
    Utils::splitFilenameFromExt(searchString, ss1, ss2);

    std::string fn1, fn2;
    Utils::splitFilenameFromExt(filename, fn1, fn2);

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

int DirTranslator::size(void)
{
    // go through all the existing shorteners, sum up their size and return that sum
    int cnt = 0;

    std::map<std::string, FilenameShortener *>::iterator it;

    for(it = mapPathToShortener.begin(); it != mapPathToShortener.end(); ++it) {        // go through the map
        FilenameShortener *fs = it->second;                                             // get the filename shortener
        cnt += fs->size();
    }

    return cnt;
}

bool compareAccessTimeInPairs(std::pair<uint32_t, int> a, std::pair<uint32_t, int> b)
{
    // return if a < b (for their access time (their first value))
    return a.first < b.first;
}

int DirTranslator::cleanUpShortenersIfNeeded(void)
{
    /*
    This method should check if the total count of stored items is not above the MAX_NAMES_IN_TRANSLATOR value
    and if it is, it should do a clean up - remove last few used filename shorteners, so we would be under this limit.
    This method should be called only when new items or shortener is being added to the dir translator - no need to 
    call it when just using the stored values (so call it only when memory usage is growing).
    Also - it will skip doing check and clean up if last check and clean up were done just now - see the check against current time below.
    */

    uint32_t now = Utils::getCurrentMs();

    if((now - lastCleanUpCheck) < 1000) {   // too soon after last clean up? quit
        Utils::out(LOG_DEBUG, "DirTranslator::cleanUpShortenersIfNeeded - too soon, ignoring");
        return 0;
    }

    lastCleanUpCheck = now;     // store current time, so we won't try to clean up too soon after this

    int currentSize = size();
    if(currentSize < MAX_NAMES_IN_TRANSLATOR) {  // still not above the maximum limit? quit
        Utils::out(LOG_DEBUG, "DirTranslator::cleanUpShortenersIfNeeded - still below the limit, ignoring");
        return 0;
    }

    // get lastAccessTime vs shortener.size() for each shortener to vector
    std::vector< std::pair<uint32_t, int> > accessVsSize;
    std::map<std::string, FilenameShortener *>::iterator it;

    for(it = mapPathToShortener.begin(); it != mapPathToShortener.end(); ++it) {        // go through the map
        FilenameShortener *fs = it->second;                                             // get the filename shortener
        std::pair<uint32_t, int> one = std::make_pair(fs->getLastAccessTime(), fs->size());
        accessVsSize.push_back(one);
    }

    // sort items by lastAccessTime from oldest to newest
    std::sort(accessVsSize.begin(), accessVsSize.end(), compareAccessTimeInPairs);

    // go through the items from oldest, keep subtracting their size until newSize < MAX_NAMES_IN_TRANSLATOR, remember lastAccessTime as oldestAccessTime
    int newSize = currentSize;                      // start with current size
    uint32_t timeBoundary = 0;                      // we will try to find this time boundary

    for(int i=0; i<accessVsSize.size(); i++) {     // go through the vector
        newSize -= accessVsSize[i].second;         // subtract the size of this file shortener

        if(newSize < MAX_NAMES_IN_TRANSLATOR) {     // if the new size is less allowed maximum, the last access time of this shortener will be our time boundary
            timeBoundary = accessVsSize[i].first;   // use lastAccessTime if this file shortener
            break;
        }
    }

    // remove items with lastAccessTime <= oldestAccessTime
    int removed = 0;
    for(it = mapPathToShortener.begin(); it != mapPathToShortener.end(); /* no increment is intentional here */) {
        bool shouldDelete = (it->second->getLastAccessTime() <= timeBoundary);

        if (shouldDelete) {
            removed++;
            mapPathToShortener.erase(it++);
        } else {
            ++it;
        }
    }

    Utils::out(LOG_DEBUG, "DirTranslator::cleanUpShortenersIfNeeded - removed %d items", removed);
    return removed;
}

// special value for sp.dir which will tell us we have finished the search for files
#define FF_FINISHED     ((void*)0xdecea5ed)

#define Internal(X)     ((SearchParamsInternal*) (X.internal))

void DirTranslator::closeDirSetFlags(SearchParams& sp)
{
    // special value or NULL value? 
    if(sp.internal == FF_FINISHED || sp.internal == NULL) {
        sp.internal = FF_FINISHED;  // mark this cycle as finished
        return;
    }

    if(Internal(sp)->dir != NULL) {      // sp.dir seems to have reasonable value
        closedir(Internal(sp)->dir);    // close the dir
    }

    sp.closeNow = false;            // don't handle closeNow command next time

    delete Internal(sp);            // delete internal struct
    sp.internal = FF_FINISHED;      // mark this cycle as finished
}

bool DirTranslator::findFirstAndNext(SearchParams& sp, DiskItem& di)
{
    // this is a subsequent method call, and the 1st call was exact match OR the whole findFirstAndNext cycle finished,
    // now just return false and don't do anything more
    if(sp.internal == FF_FINISHED) {
        return false;           // no item found
    }

    // if the caller wants to terminate the findFirstAndNext loop sooner (closeNow flag selected)
    if(sp.internal != NULL && sp.closeNow) {
        closeDirSetFlags(sp);
        return false;           // no item found
    }

    // on the 1st call of this method do some things only once
    if(sp.internal == NULL) {
        sp.closeNow = false;                                // don't terminate the findFirstAndNext loop just yet
        sp.internal = (void *) new SearchParamsInternal;    // allocate SearchParamsInternal struct
        Internal(sp)->isVFAT = true;           // start with assumption that this is a VFAT filesystem
        Utils::splitFilenameFromPath(sp.path, Internal(sp)->hostPath, Internal(sp)->searchString);

        // here we handle the special case where the search string contains no wildcard but
        // matches one particular file
        if (Internal(sp)->searchString.find('*') == std::string::npos && Internal(sp)->searchString.find('?') == std::string::npos) {
            struct stat attr;
            Utils::out(LOG_DEBUG, "DirTranslator::findFirstAndNext - no wildcard in %s, checking %s", Internal(sp)->searchString.c_str(), sp.path.c_str());

            if (stat(sp.path.c_str(), &attr) == 0) {     // matching just one file and it exists? good!
                setDiskItem(sp, di, Internal(sp)->hostPath, Internal(sp)->searchString, Internal(sp)->searchString, S_ISDIR(attr.st_mode));
                closeDirSetFlags(sp);
                return true;        // got some valid item
            }
        }

        // we didn't match single file, we will scan the whole dir
        toUpperCaseString(Internal(sp)->searchString);

        // then build the found files list
        Internal(sp)->dir = opendir(Internal(sp)->hostPath.c_str());                        // try to open the dir

        if(Internal(sp)->dir == NULL) {     // not found?
            Utils::out(LOG_DEBUG, "DirTranslator::findFirstAndNext - opendir(\"%s\") FAILED", Internal(sp)->hostPath.c_str());
            closeDirSetFlags(sp);
            return false;
        }
    }

    // we still need a loop here, because we will be skipping items (e.g. sockets, pipes, ...) 
    // which are not supported on old DOS filesystem and won't be returned
    while(true) {
        // read the next directory entry
        struct dirent *de = readdir(Internal(sp)->dir);

        if(de == NULL) {        // no more entries in the directory? exit the loop, it will close dir and report no-more-items
            break;
        }

        if(de->d_type != DT_DIR && de->d_type != DT_REG) {  // not a file, not a directory?
            Utils::out(LOG_DEBUG, "TranslatedDisk::findFirstAndNext -- skipped %s because the type %d is not supported!", de->d_name, de->d_type);
            continue;
        }

        bool isDir = (de->d_type == DT_DIR);                // is this a directory?

        if(isDir && (sp.attribs & FA_DIR) == 0) {           // is dir, but not searching for dir
            Utils::out(LOG_DEBUG, "TranslatedDisk::findFirstAndNext -- skipped %s because the it's a dir and we're not looking for dir now", de->d_name);
            continue;
        }

        std::string longFname = de->d_name;
        bool isUpDir = longFname == "." || longFname == "..";   // is this '.' or '..' dir?

        // is this up-dir and we should be adding those, or this is not up-dir? 
        if((isUpDir && sp.addUpDir) || !isUpDir) {
            if(setDiskItem(sp, di, Internal(sp)->hostPath, Internal(sp)->searchString, longFname, isDir)) {   // try to set it and if success, we can quit with success
                return true;            // success
            }
        }
    }

    // if we got here, there's no more items in dir that we should report
    closeDirSetFlags(sp);
    return false;           // fail (no more items)
}

bool DirTranslator::setDiskItem(SearchParams& sp, DiskItem& di, const std::string& hostPath, const std::string& searchString, const std::string& longFname, bool isDir)
{
    bool isUpDir = isDir && (longFname == "." || longFname == "..");    // is this '.' or '..'?

    di.attribs |= isDir ? FA_DIR : 0;                       // if it's a dir, add dir flag
    di.attribs |= (longFname[0] == '.') ? FA_HIDDEN : 0;    // enforce Mac/Unix convention of hidding files startings with '.'

    std::string fullEntryPath = hostPath;
    Utils::mergeHostPaths(fullEntryPath, longFname);

    int res;
    struct stat attr;
    res = stat(fullEntryPath.c_str(), &attr);                   // get the file status

    if(res != 0) {
        Utils::out(LOG_ERROR, "TranslatedDisk::setDiskItem -- stat(%s) failed, errno %d", fullEntryPath.c_str(), errno);
        return false;       // failed, ignore this item
    }

    di.datetime = *localtime(&attr.st_mtime);   // convert time_t to tm structure and store it
    di.size = (attr.st_size > 0xffffffff) ? 0xffffffff : (uint32_t) attr.st_size;   // bigger than 4 GB? that won't fit into uint32_t, so set just max uint32_t value

    std::string shortFname = longFname;     // init short name to original long name

    if(!isUpDir) {                          // if this is not '.' or '..', then try to convert it to short name
        res = longToShortFilename(hostPath, longFname, shortFname); // convert long to short filename

        if(!res) {
            return false;                   // failed, ignore this item
        }
    }

    di.name = shortFname;       // copy in filename

    // check the current name against searchString using fnmatch
    if (compareSearchStringAndFilename(searchString, shortFname) != 0) {
        Utils::out(LOG_ERROR, "TranslatedDisk::setDiskItem -- %s - %s does not match pattern %s", fullEntryPath.c_str(), shortFname.c_str(), searchString);
        return false;       // not matching? ignore this item
    }

    // get MS-DOS VFAT attributes if this is a VFAT filesystem
    if(Internal(sp)->isVFAT) {
        int fd = open(fullEntryPath.c_str(), O_RDONLY);

        if(fd >= 0) {
            __u32 dosattrs = 0;

            if (ioctl(fd, FAT_IOCTL_GET_ATTRIBUTES, &dosattrs) >= 0) {      // try to get attributes and set them in disk item
                if(dosattrs & ATTR_RO)      di.attribs |= FA_READONLY;
                if(dosattrs & ATTR_HIDDEN)  di.attribs |= FA_HIDDEN;
                if(dosattrs & ATTR_SYS)     di.attribs |= FA_SYSTEM;
            } else {    // it will fail if the underlying FileSystem is not FAT, also mark this as not vfat fs, so we won't try on next items
                Internal(sp)->isVFAT = false;
            }

            close(fd);
        } else {
            Utils::out(LOG_ERROR, "TranslatedDisk::appendFoundToFindStorage -- open(%s) failed, errno %d", fullEntryPath.c_str(), errno);
        }
    }

    return true;                // success
}

void DirTranslator::diskItemToAtariFindStorageItem(DiskItem& di, uint8_t* buf)
{
    uint16_t atariTime = Utils::fileTimeToAtariTime(&di.datetime);
    uint16_t atariDate = Utils::fileTimeToAtariDate(&di.datetime);

    // GEMDOS File Attributes
    buf[0] = di.attribs;

    // GEMDOS Time
    buf[1] = atariTime >> 8;
    buf[2] = atariTime &  0xff;

    // GEMDOS Date
    buf[3] = atariDate >> 8;
    buf[4] = atariDate &  0xff;

    // file size with respect to GEMDOS maximum file size
    uint32_t size = (di.size > GEMDOS_FILE_MAXSIZE) ? GEMDOS_FILE_MAXSIZE : di.size;
    buf[5] = (size >>  24) & 0xff;
    buf[6] = (size >>  16) & 0xff;
    buf[7] = (size >>   8) & 0xff;
    buf[8] =  size         & 0xff;

    // Filename -- d_fname[14]
    memset(&buf[9], 0, 14);                             // first clear the mem
    strncpy((char *) &buf[9], di.name.c_str(), 14);     // copy the filename - 'FILE.C'
}
