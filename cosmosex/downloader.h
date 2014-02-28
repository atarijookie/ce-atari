#ifndef _DOWNLOADER_H_
#define _DOWNLOADER_H_

#include <string>

#define DWNTYPE_ANY         0xff
#define DWNTYPE_UNKNOWN     0
#define DWNTYPE_UPDATE      1
#define DWNTYPE_FLOPPYIMG   2

typedef struct {
    std::string srcUrl;         // src url, e.g. http://whatever.com/file.zip
    std::string dstDir;         // dest dir, e.g. /mnt/sda1

    int downloadType;           // defines what is downloaded - update, floppy image, ...
    int downPercent;            // defines progress - from 0 to 100
} TDownloadRequest;

void downloadInitBeforeThreads(void);
void downloadCleanupBeforeQuit(void);

void downloadAdd(TDownloadRequest &tdr);
void downloadStatus(std::string &status, int downloadType);
void *downloadThreadCode(void *ptr);

#endif

