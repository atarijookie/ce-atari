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

#include "global.h"
#include "../utils.h"
#include "../debug.h"
#include "dirtranslator.h"
#include "translatedhelper.h"
#include "filenameshortener.h"
#include "gemdos.h"

#define GEMDOS_FILE_MAXSIZE	(2147483647)
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

void DirTranslator::shortToLongPath(const std::string &rootPath, const std::string &shortPath, std::string &longPath)
{
    #define MAX_DIR_NESTING     64
    static char longName[MAX_FILENAME_LEN];

    std::string strings[MAX_DIR_NESTING];
    unsigned int start = 0;
    unsigned int i, found = 0;

    // replace all possible atari path separators to host path separators

    // first split the string by separator
    for(i = 0; i < shortPath.length(); i++) {
		if(shortPath[i] != ATARIPATH_SEPAR_CHAR && shortPath[i] != HOSTPATH_SEPAR_CHAR)
			continue;

        strings[found] = shortPath.substr(start, (i - start));
        found++;
        start = i + 1;
        if(found >= MAX_DIR_NESTING) {              // sanitize possible overflow
            break;
        }
    }
    strings[found] = shortPath.substr(start);    // copy in the rest
    found++;

    // now convert all the short names to long names
    std::string pathPart = rootPath;

    for(i=0; i<found; i++) {
        if(strings[i].length() == 0) {                  // skip empty path part
            continue;
        }
        
        FilenameShortener *fs = getShortenerForPath(pathPart);
        if(fs->shortToLongFileName(strings[i].c_str(), longName)) {   // try to convert the name
            strings[i] = longName; // if there was a long version of the file name, replace the short one
        } else {
            Debug::out(LOG_DEBUG, "DirTranslator::shortToLongPath - shortToLongFileName() failed for short name: %s path=%s", strings[i].c_str(), pathPart.c_str());
        }

        Utils::mergeHostPaths(pathPart, strings[i]);   // build the path slowly
    }

    // and finally - put the string back together
    std::string final = "";

    for(i=0; i<found; i++) {
        if(strings[i].length() != 0) {      // not empty string?
            Utils::mergeHostPaths(final, strings[i]);
        }
    }

    longPath = final;
}

bool DirTranslator::longToShortFilename(const std::string &longHostPath, const std::string &longFname, std::string &shortFname)
{
    FilenameShortener *fs = getShortenerForPath(longHostPath);

    char shortName[32];                             // try to shorten the name
    bool res = fs->longToShortFileName(longFname.c_str(), shortName);   // try to convert the name from long to short

    if(res) {                                       // name shortened - store it
        shortFname = shortName;
    } else {                                        // failed to shorten - clear it
        shortFname.clear();
    }

    return res;
}

FilenameShortener *DirTranslator::getShortenerForPath(std::string path)
{
    // remove trailing '/' if needed
    if(path.size() > 0 && path[path.size() - 1] == HOSTPATH_SEPAR_CHAR) {
        path.erase(path.size() - 1, 1);
    }

    std::map<std::string, FilenameShortener *>::iterator it;
    it = mapPathToShortener.find(path);     // find the shortener for that host path
    FilenameShortener *fs;

    if(it != mapPathToShortener.end()) {            // already got the shortener
        //Debug::out(LOG_DEBUG, "DirTranslator::getShortenerForPath - shortener for %s found", path.c_str());
        fs = it->second;
    } else {                                        // don't have the shortener yet
        Debug::out(LOG_DEBUG, "DirTranslator::getShortenerForPath - shortener for %s NOT found, creating", path.c_str());
        fs = createShortener(path);
    }
	return fs;
}

FilenameShortener *DirTranslator::createShortener(const std::string &path)
{
    FilenameShortener *fs = new FilenameShortener();
    mapPathToShortener.insert( std::pair<std::string, FilenameShortener *>(path, fs) );

	DIR *dir = opendir(path.c_str());						// try to open the dir
	
    if(dir == NULL) {                                 				// not found?
        return fs;
    }
	
    char shortName[14];
	
	while(1) {                                                  	// while there are more files, store them
		struct dirent *de = readdir(dir);							// read the next directory entry
	
		if(de == NULL) {											// no more entries?
			break;
		}
	
		if(de->d_type != DT_DIR && de->d_type != DT_REG) {			// not 	a file, not a directory?
			Debug::out(LOG_DEBUG, "TranslatedDisk::createShortener -- skipped %s because the type %d is not supported!", de->d_name, de->d_type);
			continue;
		}
		if(strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;

		fs->longToShortFileName(de->d_name, shortName);
    }

	closedir(dir);	
    return fs;
}

bool DirTranslator::buildGemdosFindstorageData(TFindStorage *fs, std::string hostSearchPathAndWildcards, BYTE findAttribs, bool isRootDir, bool useZipdirNotFile)
{
	std::string hostPath, searchString;

    // initialize partial find results (dirs and files separately)
    fsDirs.clear();
    fsFiles.clear();
    
    Utils::splitFilenameFromPath(hostSearchPathAndWildcards, hostPath, searchString);

	toUpperCaseString(searchString);
	
    // then build the found files list
	DIR *dir = opendir(hostPath.c_str());							// try to open the dir
	
    if(dir == NULL) {                                 				// not found?
        return false;
    }

    // initialize find storage in case anything goes bad
    fs->clear();

    while(fs->count < fs->maxCount) {	        					// avoid buffer overflow
		struct dirent *de = readdir(dir);							// read the next directory entry
	
		if(de == NULL) {											// no more entries?
			break;
		}

		if(de->d_type != DT_DIR && de->d_type != DT_REG) {			// not a file, not a directory?
			Debug::out(LOG_DEBUG, "TranslatedDisk::onFsfirst -- skipped %s because the type %d is not supported!", de->d_name, de->d_type);
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
		
        // if ZIP directories are supported
        if(useZipdirNotFile) {                                                  // if ZIP DIRs are enabled
            if(de->d_type == DT_REG) {                                          // if it's a file
                int len = strlen(de->d_name);                                   // get filename length
                
                if(len > 4) {                                                   // if filename is at least 5 chars long
                    char *found = strcasestr(de->d_name + len - 4, ".ZIP");    // see if it ends with .ZIP

                    if(found != NULL) {                                         // if filename ends with .ZIP
                        std::string fullZipPath = hostPath + "/" + longFname;   // create full path to that zip file
                    
                        struct stat attr;
                        int res = stat(fullZipPath.c_str(), &attr);    // get the status of the possible zip file

                        if(res == 0) {                                          // if stat() succeeded
                            if(attr.st_size <= MAX_ZIPDIR_ZIPFILE_SIZE) {       // file not too big? change flags from file to dir
                                de->d_type = DT_DIR;
                            }
                        }
                    }
                }
            }
        }
        
		// finnaly append to the find storage
		appendFoundToFindStorage(hostPath, searchString.c_str(), fs, de, findAttribs);
    }

	closedir(dir);
    
    // now in the end merge found dirs and found files
    int dirsSize    = fsDirs.count  * 23;
    int filesSize   = fsFiles.count * 23;
    
    memcpy(fs->buffer,              fsDirs.buffer,  dirsSize);          // copy to total find search - first the dirs
    memcpy(fs->buffer + dirsSize,   fsFiles.buffer, filesSize);         // copy to total find search - then the files
    
	return true;
}

void DirTranslator::appendFoundToFindStorage(std::string &hostPath, const char *searchString, TFindStorage *fs, struct dirent *de, BYTE findAttribs)
{
    // TODO: verify on ST that the find attributes work like this

    // TOS 1.04 searches with findAttribs set to 0x10, that's INCLUDE DIRs

    // first verify if the file attributes are OK
	// TODO: do support for checking the READ ONLY flag on linux
	bool isReadOnly = false;
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

    //--------
    // add this file
    TFindStorage *fsPart = isDir ? &fsDirs : &fsFiles;          // get the pointer to partial find storage to separate dirs from files when searching
    
    DWORD addr  = fsPart->count * 23;               	        // calculate offset
    BYTE *buf   = &(fsPart->buffer[addr]);          	        // and get pointer to this location

    BYTE atariAttribs;								            // convert host to atari attribs
    Utils::attributesHostToAtari(isReadOnly, isDir, atariAttribs);

	if(de->d_name[0] == '.') atariAttribs |= FA_HIDDEN;		// enforce Mac/Unix convention of hidding files startings with '.'

	std::string fullEntryPath 	= hostPath;
	std::string longFname		= de->d_name;
	Utils::mergeHostPaths(fullEntryPath, longFname);
	
	int res;
	struct stat attr;
	tm *timestr;
    std::string shortFname;
	
	res = stat(fullEntryPath.c_str(), &attr);					// get the file status
	
	if(res != 0) {
		Debug::out(LOG_ERROR, "TranslatedDisk::appendFoundToFindStorage -- stat() failed, errno %d", errno);
		return;		
	}

	timestr = localtime(&attr.st_mtime);			    	    // convert time_t to tm structure

	res = longToShortFilename(hostPath, longFname, shortFname); // convert long to short filename
    if(!res) {
        return;
    }
	
    WORD atariTime = Utils::fileTimeToAtariTime(timestr);
    WORD atariDate = Utils::fileTimeToAtariDate(timestr);

    // now convert the short 'FILE.C' to 'FILE    .C  '
    char shortFnameExtended[14];
    FilenameShortener::extendWithSpaces(shortFname.c_str(), shortFnameExtended);

    // check the current name against searchString using fnmatch
	int ires = compareSearchStringAndFilename(searchString, shortFname.c_str());
		
	if(ires != 0) {     // not matching? quit
		return;
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
				//Debug::out(LOG_ERROR, "TranslatedDisk::appendFoundToFindStorage -- ioctl(%s, FAT_IOCTL_GET_ATTRIBUTES) failed errno %d", fullEntryPath.c_str(), errno);
			}
			close(fd);
		} else {
			Debug::out(LOG_ERROR, "TranslatedDisk::appendFoundToFindStorage -- open(%s) failed, errno %d", fullEntryPath.c_str(), errno);
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
	DWORD size;
	if(attr.st_size > GEMDOS_FILE_MAXSIZE) {
		size = GEMDOS_FILE_MAXSIZE;
	} else {
		size = (DWORD)attr.st_size;
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

void DirTranslator::appendFoundToFindStorage_dirUpDirCurr(std::string &hostPath, const char *searchString, TFindStorage *fs, struct dirent *de, BYTE findAttribs)
{
    TFindStorage *fsPart = &fsDirs;                     // get the pointer to partial find storage to separate dirs from files when searching

    // add this file
    DWORD addr  = fsPart->count * 23;               	// calculate offset
    BYTE *buf   = &(fsPart->buffer[addr]);          	// and get pointer to this location

    BYTE atariAttribs;								    // convert host to atari attribs
    Utils::attributesHostToAtari(false, true, atariAttribs);

	std::string fullEntryPath 	= hostPath;
	std::string longFname		= de->d_name;
	Utils::mergeHostPaths(fullEntryPath, longFname);
	
	int res;
	struct stat attr;
	tm *timestr;
    std::string shortFname;
	
	res = stat(fullEntryPath.c_str(), &attr);					// get the file status
	
	if(res != 0) {
		Debug::out(LOG_ERROR, "TranslatedDisk::appendFoundToFindStorage -- stat() failed, errno %d", errno);
		return;		
	}

	timestr = localtime(&attr.st_mtime);			    		// convert time_t to tm structure

    WORD atariTime = Utils::fileTimeToAtariTime(timestr);
    WORD atariDate = Utils::fileTimeToAtariDate(timestr);

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

int DirTranslator::compareSearchStringAndFilename(const char *searchString, const char *filename)
{
	char ss1[16], ss2[16];
	char fn1[16], fn2[16];
	
	FilenameShortener::splitFilenameFromExtension(searchString, ss1, ss2);
	FilenameShortener::splitFilenameFromExtension(filename, fn1, fn2);

	// check if filename matches
	int ires = fnmatch(ss1, fn1, FNM_PATHNAME);

	if(ires != 0) {
		return -1;
	}

	// check if extension matches
	ires = fnmatch(ss2, fn2, FNM_PATHNAME);

	if(ires != 0) {
		return -1;
	}
	
	return 0;
}

TFindStorage::TFindStorage()
{
    buffer = new BYTE[getSize()];
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


