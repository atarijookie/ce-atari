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

#include "libdospath.h"
#include "utilslib.h"
#include "dirtranslator.h"
#include "filenameshortener.h"

// This value sets the maximum count of shortened names we will have in our shorteners, to limit ram usage.
// With 1 name being around 433 B the limit of 100k names means about 43 MB of RAM used before cleanup happens.
int MAX_NAMES_IN_TRANSLATOR = 100000;

#define GEMDOS_FILE_MAXSIZE (2147483647)
// TOS 1.x cannot display size with more than 8 digits
//#define GEMDOS_FILE_MAXSIZE (100*1000*1000-1)

DirTranslator::DirTranslator()
{
    lastCleanUpCheck = UtilsLib::getCurrentMs();
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

void DirTranslator::updateFileName(const std::string& hostPath, const std::string& oldFileName, const std::string& newFileName)
{
    FilenameShortener *fs = getShortenerForPath(hostPath, false);   // try to find a shortener for our path, but don't create it if not found

    if(fs) {        // if existing shortener was found, update filename
        fs->updateLongFileName(oldFileName, newFileName);
    }
}

void DirTranslator::shortToLongPath(const std::string& shortPath, std::string& longPath, bool refreshOnMiss, std::vector<std::string>* pSymlinksApplied, int recursionLevel)
{
    if(pSymlinksApplied && recursionLevel == 0) {           // clear the vector of applied symlinks on 0th level of recursion
        pSymlinksApplied->clear();
    }

    std::string inPath = shortPath;
    UtilsLib::toHostSeparators(inPath);                    // from dos/atari separators to host separators only

    std::vector<std::string> shortPathParts;
    UtilsLib::splitString(inPath, '/', shortPathParts);    // explode string to individual parts

    unsigned int i, partsCount = shortPathParts.size();
    std::vector<std::string> longPathParts;

    // now convert all the short names to long names
    for(i=0; i<partsCount; i++) {
        std::string subPath;
        UtilsLib::joinStrings(shortPathParts, subPath, i);             // create just sub-path from the whole path (e.g. just '/mnt/shared' from '/mnt/shared/images/games')

        FilenameShortener *fs = getShortenerForPath(subPath);       // find or create shortener for this path

        std::string longName;
        bool ok;

        ok = fs->shortToLongFileName(shortPathParts[i], longName);  // try to convert short-to-long filename

        if(!ok && refreshOnMiss) {          // if conversion from short-to-long failed, and we should do a refresh on dict-miss (cache-miss)
            UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::shortToLongPath - shortToLongFileName() failed for short name: %s , subPath=%s, but will do a refresh now!", shortPathParts[i].c_str(), subPath.c_str());

            feedShortener(subPath, fs);     // feed shortener with possibly new filenames
            ok = fs->shortToLongFileName(shortPathParts[i], longName);      // 2nd attempt to convert short-to-long filename (after refresh)
        }

        if(ok) {        // filename translation from short-to-long was successful - replace the short part with long part
            longPathParts.push_back(std::string(longName));
        } else {        // failed to find long path, use the original in this place
            longPathParts.push_back(shortPathParts[i]);
            UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::shortToLongPath - shortToLongFileName() failed for short name: %s , subPath=%s", shortPathParts[i].c_str(), subPath.c_str());
        }
    }

    UtilsLib::joinStrings(longPathParts, longPath);                 // join all output parts to one long path
    bool symlinkWasApplied = applySymlinkIfPossible(longPath, pSymlinksApplied);        // apply symlink if possible

    // if we applied symlink, we might need to do the short-to-long translation again, as parts of path after symlink
    // need additional resolving. But only if recursionLevel is small, otherwise we could be stuck in some endless recursion.
    if(symlinkWasApplied && recursionLevel < 3) {
        UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::shortToLongPath - additional short-to-long path after symlink applied is needed for: %s", longPath.c_str());

        std::string longPath2;
        shortToLongPath(longPath, longPath2, refreshOnMiss, pSymlinksApplied, recursionLevel + 1);
        longPath = longPath2;       // copy the new long path to the expected place

        UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::shortToLongPath - additional short-to-long path after symlink applied resulted in: %s", longPath.c_str());
    }
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
        //UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::getShortenerForPath - shortener for %s found", path.c_str());
        fs = it->second;
    } else {                                        // don't have the shortener yet
        if(createIfNotFound) {  // should create?
            UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::getShortenerForPath - shortener for %s NOT found, creating", path.c_str());
            fs = createShortener(path);
        } else {                // shoul NOT create?
            //UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::getShortenerForPath - shortener for %s NOT found, returning NULL", path.c_str());
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
            UtilsLib::out(LDP_LOG_DEBUG, "TranslatedDisk::feedShortener -- skipped %s because the type %d is not supported!", de->d_name, de->d_type);
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
    UtilsLib::splitFilenameFromExt(searchString, ss1, ss2);

    std::string fn1, fn2;
    UtilsLib::splitFilenameFromExt(filename, fn1, fn2);

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

    uint32_t now = UtilsLib::getCurrentMs();

    if((now - lastCleanUpCheck) < 1000) {   // too soon after last clean up? quit
        UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::cleanUpShortenersIfNeeded - too soon, ignoring");
        return 0;
    }

    lastCleanUpCheck = now;     // store current time, so we won't try to clean up too soon after this

    int currentSize = size();
    if(currentSize < MAX_NAMES_IN_TRANSLATOR) {  // still not above the maximum limit? quit
        UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::cleanUpShortenersIfNeeded - still below the limit, ignoring");
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

    for(size_t i=0; i<accessVsSize.size(); i++) {   // go through the vector
        newSize -= accessVsSize[i].second;          // subtract the size of this file shortener

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

    UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::cleanUpShortenersIfNeeded - removed %d items", removed);
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

void DirTranslator::clearDiskItem(DiskItem& di)
{
    di.attribs = 0;
    memset(&di.datetime, 0, sizeof(di.datetime));
    di.name = "";
    di.size = 0;
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

    clearDiskItem(di);          // clear disk item, so we won't have any remains from previous run

    // on the 1st call of this method do some things only once
    if(sp.internal == NULL) {
        sp.closeNow = false;                                // don't terminate the findFirstAndNext loop just yet
        sp.internal = (void *) new SearchParamsInternal();  // allocate SearchParamsInternal struct
        Internal(sp)->isVFAT = true;           // start with assumption that this is a VFAT filesystem
        UtilsLib::splitFilenameFromPath(sp.path, Internal(sp)->hostPath, Internal(sp)->searchString);

        // here we handle the special case where the search string contains no wildcard but
        // matches one particular file
        if (Internal(sp)->searchString.find('*') == std::string::npos && Internal(sp)->searchString.find('?') == std::string::npos) {
            struct stat attr;
            UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::findFirstAndNext - no wildcard in %s, checking %s", Internal(sp)->searchString.c_str(), sp.path.c_str());

            // this might be a short filename, so try to translate it from short to long version
            std::string longPath;
            shortToLongPath(sp.path, longPath, false);
            UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::findFirstAndNext - translated possible short path: %s to long path: %s", sp.path.c_str(), longPath.c_str());

            if (UtilsLib::fileExists(longPath)) {      // matching just one file and it exists? good!
                std::string justPath, justLongFname;
                UtilsLib::splitFilenameFromPath(longPath, justPath, justLongFname);       // extract just long filename from the long path

                setDiskItem(sp, di, Internal(sp)->hostPath, Internal(sp)->searchString, justLongFname, S_ISDIR(attr.st_mode));
                closeDirSetFlags(sp);
                return true;        // got some valid item
            } else {
                UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::findFirstAndNext - long path: %s doesn't exist, will try to find file one-by-one anyway", longPath.c_str());
            }
        }

        // we didn't match single file, we will scan the whole dir
        toUpperCaseString(Internal(sp)->searchString);

        // then build the found files list
        Internal(sp)->dir = opendir(Internal(sp)->hostPath.c_str());                        // try to open the dir

        if(Internal(sp)->dir == NULL) {     // not found?
            UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::findFirstAndNext - opendir(\"%s\") FAILED", Internal(sp)->hostPath.c_str());
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
            UtilsLib::out(LDP_LOG_DEBUG, "TranslatedDisk::findFirstAndNext -- skipped %s because the type %d is not supported!", de->d_name, de->d_type);
            continue;
        }

        bool isDir = (de->d_type == DT_DIR);                // is this a directory?
        bool wantDirs = (sp.attribs & FA_DIR) == FA_DIR;    // if FA_DIR bit is set in search param attribs, we want also dirs to be included

        /* Note on next condition: 
            - if FA_DIR is set, results wil contain FILES and DIRECTORIES
            - if FA_DIR is clear, results will contain ONLY FILES
           It seems there is no option to include ONLY DIRECTORIES without files - Atari Compendium states, 
           that if you set FA_DIR bit, then should include subdirectories.
        */
        if(isDir && !wantDirs) {        // if is_dir but dont_want_dirs, skip it
            UtilsLib::out(LDP_LOG_DEBUG, "TranslatedDisk::findFirstAndNext -- skipped %s because the it's a dir and we're not looking for dir now", de->d_name);
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

    di.attribs |= (isDir ? FA_DIR : 0);                     // if it's a dir, add dir flag
    di.attribs |= ((longFname[0] == '.') ? FA_HIDDEN : 0);  // enforce Mac/Unix convention of hidding files startings with '.'

    std::string fullEntryPath = hostPath;
    UtilsLib::mergeHostPaths(fullEntryPath, longFname);

    int res;
    struct stat attr;
    res = stat(fullEntryPath.c_str(), &attr);                   // get the file status

    if(res != 0) {
        UtilsLib::out(LDP_LOG_ERROR, "TranslatedDisk::setDiskItem -- stat(%s) failed, errno %d", fullEntryPath.c_str(), errno);
        return false;       // failed, ignore this item
    }

    di.datetime = *localtime(&attr.st_mtime);   // convert time_t to tm structure and store it

    if(!isDir) {        // file? store size
        di.size = (attr.st_size > 0xffffffff) ? 0xffffffff : (uint32_t) attr.st_size;   // bigger than 4 GB? that won't fit into uint32_t, so set just max uint32_t value
    } else {            // dir? store zero size
        di.size = 0;
    }

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
        UtilsLib::out(LDP_LOG_ERROR, "TranslatedDisk::setDiskItem -- %s - %s does not match pattern %s", fullEntryPath.c_str(), shortFname.c_str(), searchString.c_str());
        return false;       // not matching? ignore this item
    } else {
        UtilsLib::out(LDP_LOG_DEBUG, "TranslatedDisk::setDiskItem -- %s matches pattern %s", shortFname.c_str(), searchString.c_str());
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
            UtilsLib::out(LDP_LOG_ERROR, "TranslatedDisk::appendFoundToFindStorage -- open(%s) failed, errno %d", fullEntryPath.c_str(), errno);
        }
    }

    return true;                // success
}

// Create virtual symlink from source to destination. Use empty destination to remove symlink.
void DirTranslator::symlink(const std::string& longPathSource, const std::string& longPathDest)
{
    std::string src = longPathSource;
    UtilsLib::removeTrailingSeparator(src);     // remove '/' if needed

    std::string dest = longPathDest;
    UtilsLib::removeTrailingSeparator(dest);    // remove '/' if needed

    std::map<std::string, std::string>::iterator it;
    it = mapSymlinks.find(src);
    bool exists = it != mapSymlinks.end();

    if(dest.empty()) {                      // destination empty? this means remove symlink
        if(exists) {                        // source was found in our map
            UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::symlink - removing symlink for source: %s", src.c_str());
            mapSymlinks.erase(src);         // remove it
        }
    } else {                                // destination not empty? this means add / update
        if(exists) {    // exists? update
            UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::symlink - updating symlink %s -> %s", src.c_str(), dest.c_str());
            mapSymlinks[src] = dest;
        } else {        // doesn't exist? insert
            UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::symlink - adding symlink %s -> %s", src.c_str(), dest.c_str());
            mapSymlinks.insert(std::pair<std::string,std::string>(src, dest));
        }
    }
}

bool DirTranslator::applySymlinkIfPossible(std::string& inLongPath, std::vector<std::string>* pSymlinksApplied)
{
    bool foundMatch = false;
    std::string src, dest;

    // go through all the stored symlinks in mapSymlink
    std::map<std::string, std::string>::iterator it;
    for(it = mapSymlinks.begin(); it != mapSymlinks.end(); ++it) {          // go through the map
        // do a quick check if inLongPath matches any source of symlink - this is quick as it's just comparing 2 string
        src = it->first;
        dest = it->second;
        int res = strncmp(inLongPath.c_str(), src.c_str(), src.length());   // compare one symlink source to part of inLongPath

        if(res != 0) {          // source was not completely found in inLongPath? skip the rest
            continue;
        }

        // if symlink now seems to be at the start of the inLongPath, we now want to do a better but slower check
        // if the symlink matches matches whole part of the path, or falsely matches part of of path, e.g.:
        // inLongPath = "/tmp/sourceDir/something"
        // source     = "/tmp/source"
        // In This case we it's not really a match - source matches start of the inLongPath, but not the whole sourceDir,
        // so this would be a false positive.

        // split source into parts, see how many parts it has.
        std::vector<std::string> srcParts;
        UtilsLib::splitString(src, '/', srcParts);          // explode string to individual parts
        unsigned int srcPartsCount = srcParts.size();       // count source parts

        // split inLongPath into parts, but take only as many parts as the source has (e.g. only 2 in our example above).
        std::vector<std::string> inParts;
        UtilsLib::splitString(inLongPath, '/', inParts);    // explode string to individual parts

        std::string inLongSubPath;
        UtilsLib::joinStrings(inParts, inLongSubPath, srcPartsCount);   // create part of inLongPath made out of same count of parts as link source path

        // now compare the right count of parts of source and inLongPath. If not a match, check next symlink.
        res = strcmp(src.c_str(), inLongSubPath.c_str());

        if(res == 0) {          // exact match on path was found, good!
            foundMatch = true;
            break;
        }
    }

    // if an exact match was found, replace source in inLongPath with destination
    if(foundMatch) {
        if(pSymlinksApplied) {                  // got pointer to applied symlinks vector?
            pSymlinksApplied->push_back(src);   // add this symlink source to vector
        }

        std::string rest = inLongPath.substr(src.length());     // remove the source length from inLongPath, keep just rest
        UtilsLib::mergeHostPaths(dest, rest);           // merge destination + rest, result will be stored in dest
        UtilsLib::out(LDP_LOG_DEBUG, "DirTranslator::applySymlinkIfPossible - %s -> %s", inLongPath.c_str(), dest.c_str());
        inLongPath = dest;                              // copy new path to inLongPath
    }

    return foundMatch;      // returns true if symlink was applied
}

// returns true if symlink for the source is present
bool DirTranslator::gotSymlink(const std::string& longPathSource)
{
    std::string src = longPathSource;
    UtilsLib::removeTrailingSeparator(src);     // remove '/' if needed

    std::map<std::string, std::string>::iterator it;
    it = mapSymlinks.find(src);
    bool exists = it != mapSymlinks.end();
    return exists;
}
