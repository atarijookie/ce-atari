#ifndef __DEFS_H__
#define __DEFS_H__

#include <string>
#include <inttypes.h>
#include <dirent.h>

#define LOG_OFF         0
#define LOG_INFO        1       // info         - info which can be displayed when running at user's place
#define LOG_ERROR       2       // errors       - should be always visible, even to users
#define LOG_DEBUG       3       // debug info   - useful only to developers

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

// struct used for Fsfirst and Fsnext - modified version without first 21 reserved bytes
// now the struct has 23 bytes total, so a buffer of 512 bytes should contain 22 of these + 6 spare bytes
typedef struct
{
    uint8_t     d_attrib;       // GEMDOS File Attributes
    uint16_t    d_time;         // GEMDOS Time
    uint16_t    d_date;         // GEMDOS Date
    uint32_t    d_length;       // File Length
    char        d_fname[14];    // Filename
} DTAshort;

// following struct is used internally by the findFirstAndNext() method
typedef struct {
    std::string hostPath;
    std::string searchString;
    bool        isVFAT;
    DIR*        dir;            // set to NULL before the 1st call, don't touch on subsequent calls - used for going through the directory
} SearchParamsInternal;

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

#endif
