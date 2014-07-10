#ifndef _DOWNLOADER_H_
#define _DOWNLOADER_H_

#include <string>

#define DWNTYPE_ANY             0xff
#define DWNTYPE_UNKNOWN         0
#define DWNTYPE_UPDATE_LIST     1     
#define DWNTYPE_UPDATE_COMP     2
#define DWNTYPE_FLOPPYIMG_LIST  4
#define DWNTYPE_FLOPPYIMG       8

#define DWNSTATUS_WAITING       0              
#define DWNSTATUS_DOWNLOADING   1 
#define DWNSTATUS_VERIFYING     2
#define DWNSTATUS_DOWNLOAD_OK   3              
#define DWNSTATUS_DOWNLOAD_FAIL 4              

typedef struct {
    std::string srcUrl;         // src url, e.g. http://whatever.com/file.zip
    std::string dstDir;         // dest dir, e.g. /mnt/sda1

    int downloadType;           // defines what is downloaded - update, floppy image, ...
    WORD checksum;              // used to verify file integrity after download

    BYTE *pStatusByte;          // if set to non-null, will be updated with the download status DWNSTATUS_*
    
    int downPercent;            // defines progress - from 0 to 100
} TDownloadRequest;

class Downloader
{
public:
    static void initBeforeThreads(void);                                // call this to init libcurl
    static void cleanupBeforeQuit(void);                                // call this to deinit libcurl

    static void add(TDownloadRequest &tdr);                             // add download request for single file

    static int  count(int downloadTypeMask);                            // count pending and running downloads according to mask
    static void status(std::string &status, int downloadTypeMask);      // create status report string of pending and running downloads according to mask

    static bool verifyChecksum(char *filename, WORD checksum);

private:
    static void formatStatus(TDownloadRequest &tdr, std::string &line);
};

void *downloadThreadCode(void *ptr);

#endif

