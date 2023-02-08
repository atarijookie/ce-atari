#ifndef __LIBDOSPATH_H__
#define __LIBDOSPATH_H__

#include <iostream>
#include <stdint.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <inttypes.h>
#include <dirent.h>

#define LDP_LOG_OFF     0
#define LDP_LOG_INFO    1       // info         - info which can be displayed when running at user's place
#define LDP_LOG_ERROR   2       // errors       - should be always visible, even to users
#define LDP_LOG_WARNING 3       // warning
#define LDP_LOG_DEBUG   4       // debug info   - useful only to developers

#define HOSTPATH_SEPAR_STRING       "/"
#define HOSTPATH_SEPAR_CHAR         '/'
#define ATARIPATH_SEPAR_CHAR        '\\'

// file attributes
#define FA_READONLY     (1 << 0)
#define FA_HIDDEN       (1 << 1)
#define FA_SYSTEM       (1 << 2)
#define FA_VOLUME       (1 << 3)
#define FA_DIR          (1 << 4)
#define FA_ARCHIVE      (1 << 5)

#define FA_ALL          (0x3f)

// following struct is used internally by the findFirstAndNext() method
class SearchParamsInternal
{
public:
    SearchParamsInternal(){
        hostPath = "";
        searchString = "";
        isVFAT = true;
        dir = NULL;
    }

    std::string hostPath;
    std::string searchString;
    bool        isVFAT;
    DIR*        dir;            // set to NULL before the 1st call, don't touch on subsequent calls - used for going through the directory
};

typedef struct {
    std::string path;           // path where we should look for files, can contain wildcards (*, ?) or even name matching single file
    uint8_t     attribs;        // file attributes we should look for 
    bool        addUpDir;       // when set to true, then '.' and '..' folders will be reported back
    bool        closeNow;       // if you need to terminate the file search before you reach the last file, set to true and call the findFirstAnddNext() function

    void*       internal;       // pointer to internal struct (SearchParamsInternal*)- set to NULL before 1st call, don't touch on subsequent calls
} SearchParams;

typedef struct {
    std::string name;           // short filename
    uint8_t     attribs;        // file attributes
    tm          datetime;       // file date/time
    uint32_t    size;           // size of file
} DiskItem;

extern "C" {
    void ldp_setParam(int paramNo, uint64_t paramVal);
    void ldp_shortToLongPath(const std::string& shortPath, std::string& longPath, bool refreshOnMiss, std::vector<std::string>* pSymlinksApplied=NULL);
    void ldp_updateFileName(const std::string& hostPath, const std::string& oldFileName, const std::string& newFileName);
    bool ldp_findFirstAndNext(SearchParams& sp, DiskItem& di);
    void ldp_symlink(const std::string& longPathSource, const std::string& longPathDest);
    bool ldp_gotSymlink(const std::string& longPathSource);
    void ldp_cleanup(void);
    void ldp_runCppTests(void);
}

#endif
