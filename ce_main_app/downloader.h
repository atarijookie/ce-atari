// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _DOWNLOADER_H_
#define _DOWNLOADER_H_

#include <pthread.h>
#include <curl/curl.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#define DWNTYPE_ANY             0x0ff
#define DWNTYPE_UNKNOWN         0x000
#define DWNTYPE_UPDATE_LIST     0x001
#define DWNTYPE_UPDATE_COMP     0x002
#define DWNTYPE_FLOPPYIMG_LIST  0x004
#define DWNTYPE_FLOPPYIMG       0x008
#define DWNTYPE_TIMESYNC        0x010
#define DWNTYPE_SEND_CONFIG     0x020
#define DWNTYPE_REPORT_VERSIONS 0x040
#define DWNTYPE_LOG_HTTP        0x080
#define DWNTYPE_HW_LICENSE      0x100

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

    volatile BYTE statusByte;   // this will be used for storing status, even if not read by other thread, for purpose of counting files in queue
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
    static int  progressOfCurrentDownload(void);
    static void status(std::string &status, int downloadTypeMask);      // create status report string of pending and running downloads according to mask
    static void statusJson(std::ostringstream &status, int downloadTypeMask);   // same as status(), but returned as json

    static bool verifyChecksum(const char *filename, WORD checksum);
    static bool handleZIPedImage(const char *destDirectory, const char *zipFilePath);

    static void run(void);
    static void stop(void);

private:
    static void formatStatus(TDownloadRequest &tdr, std::string &line);
    static void formatStatusJson(TDownloadRequest &tdr, std::ostringstream &status);
    static void updateStatusByte(TDownloadRequest &tdr, BYTE newStatus);

    static void handleReportVersions(CURL *curl, const char *reportUrl);
    static void handleGetHwLicense(CURL *curl, const char *getLicenseUrl, const char *settingsKeyForLicense);

    static size_t my_write_func_reportVersions(void *ptr, size_t size, size_t nmemb, FILE *stream);
    static size_t my_write_func_getHwLicense(void *ptr, size_t size, size_t nmemb, std::string *receivedBody);
    static size_t my_write_func(void *ptr, size_t size, size_t nmemb, FILE *stream);
    static size_t my_read_func(void *ptr, size_t size, size_t nmemb, FILE *stream);
    static int my_progress_func(void *clientp, double downTotal, double downNow, double upTotal, double upNow);

    static void handleZIPedFloppyImage(std::string &fileName);

    static pthread_mutex_t                  downloadQueueMutex;
    static pthread_cond_t                   downloadQueueNotEmpty;
    static std::vector<TDownloadRequest>    downloadQueue;
    static TDownloadRequest                 downloadCurrent;

    static volatile bool shouldStop;
};

void *downloadThreadCode(void *ptr);

#endif
