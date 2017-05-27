// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _DOWNLOADER_H_
#define _DOWNLOADER_H_

#include <pthread.h>
#include <curl/curl.h>
#include <string>
#include <vector>

#define DWNTYPE_ANY             0xff
#define DWNTYPE_UNKNOWN         0x00
#define DWNTYPE_UPDATE_LIST     0x01     
#define DWNTYPE_UPDATE_COMP     0x02
#define DWNTYPE_FLOPPYIMG_LIST  0x04
#define DWNTYPE_FLOPPYIMG       0x08
#define DWNTYPE_TIMESYNC        0x10
#define DWNTYPE_SEND_CONFIG     0x20
#define DWNTYPE_REPORT_VERSIONS 0x40
#define DWNTYPE_LOG_HTTP        0x80

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

    volatile BYTE *pStatusByte; // if set to non-null, will be updated with the download status DWNSTATUS_*
    
    volatile int downPercent;   // defines progress - from 0 to 100
} TDownloadRequest;

class Downloader
{
public:
    static void initBeforeThreads(void);                                // call this to init libcurl
    static void cleanupBeforeQuit(void);                                // call this to deinit libcurl

    static void add(TDownloadRequest &tdr);                             // add download request for single file

    static int  count(int downloadTypeMask);                            // count pending and running downloads according to mask
    static void status(std::string &status, int downloadTypeMask);      // create status report string of pending and running downloads according to mask

    static bool verifyChecksum(const char *filename, WORD checksum);
    static bool handleZIPedImage(const char *destDirectory, const char *zipFilePath);

    static void run(void);
    static void stop(void);

private:
    static void formatStatus(TDownloadRequest &tdr, std::string &line);
    static void updateStatusByte(TDownloadRequest &tdr, BYTE newStatus);

    static void handleReportVersions(CURL *curl, const char *reportUrl);
    static size_t my_write_func_reportVersions(void *ptr, size_t size, size_t nmemb, FILE *stream);
    static size_t my_write_func(void *ptr, size_t size, size_t nmemb, FILE *stream);
    static size_t my_read_func(void *ptr, size_t size, size_t nmemb, FILE *stream);
    static int my_progress_func(void *clientp, double downTotal, double downNow, double upTotal, double upNow);


    static pthread_mutex_t                  downloadQueueMutex;
    static pthread_cond_t                   downloadQueueNotEmpty;
    static std::vector<TDownloadRequest>    downloadQueue;
    static TDownloadRequest                 downloadCurrent;

    static volatile bool shouldStop;
};

void *downloadThreadCode(void *ptr);

#endif
