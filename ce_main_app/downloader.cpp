// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <string>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <algorithm>

#include <signal.h>
#include <pthread.h>
#include <vector>

#include <curl/curl.h>
#include "global.h"
#include "debug.h"
#include "downloader.h"
#include "utils.h"
#include "version.h"

// sudo apt-get install libcurl4-gnutls-dev
// gcc main.c -lcurl

extern volatile sig_atomic_t sigintReceived;

extern const char *distroString;        // linux distro string
extern RPiConfig rpiConfig;             // RPi info structure

pthread_mutex_t Downloader::downloadQueueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t Downloader::downloadQueueNotEmpty = PTHREAD_COND_INITIALIZER;
volatile bool Downloader::shouldStop = false;
std::vector<TDownloadRequest>    Downloader::downloadQueue;
TDownloadRequest                Downloader::downloadCurrent;


void Downloader::initBeforeThreads(void)
{
    curl_global_init(CURL_GLOBAL_ALL);                  // curl global init, must be called before any other threads start (not only curl threads)
    downloadCurrent.downPercent = 100;                  // mark that the current download is downloaded (shouldn't display it in status)
}

void Downloader::cleanupBeforeQuit(void)
{
    curl_global_cleanup();                              // curl global clean up, at the end...
}

void Downloader::add(TDownloadRequest &tdr)
{
    pthread_mutex_lock(&downloadQueueMutex);               // try to lock the mutex
    tdr.downPercent = 0;                                    // mark that the download didn't start
    updateStatusByte(tdr, DWNSTATUS_WAITING);               // mark that the download didn't start
    downloadQueue.push_back(tdr);                           // add this to queue
    pthread_cond_signal(&downloadQueueNotEmpty);
    pthread_mutex_unlock(&downloadQueueMutex);             // unlock the mutex
}

void Downloader::formatStatusJson(TDownloadRequest &tdr, std::ostringstream &status)
{
    std::string urlPath, fileName;
    Utils::splitFilenameFromPath(tdr.srcUrl, urlPath, fileName);

    status << "{ \"fileName\": \"";
    status << fileName;
    status << "\", \"progress\": ";
    status << tdr.downPercent;
    status << "}";
}

void Downloader::formatStatus(TDownloadRequest &tdr, std::string &line)
{
    char percString[16];

    std::string urlPath, fileName;
    Utils::splitFilenameFromPath(tdr.srcUrl, urlPath, fileName);

    if(fileName.length() < 20) {                            // filename too short? extend to 20 chars with spaces
        fileName.resize(20, ' ');
    } else if(fileName.length() > 20) {                     // longer than 20 chars? make it shorter, add ... at the end
        fileName.resize(20);
        fileName.replace(17, 3, "...");
    }

    sprintf(percString, " % 3d %%", tdr.downPercent);

    line = fileName + percString;

    Debug::out(LOG_DEBUG, "Downloader::formatStatus() -- created status line: %s", line.c_str());
}

void Downloader::status(std::string &status, int downloadTypeMask)
{
    pthread_mutex_lock(&downloadQueueMutex);               // try to lock the mutex

    std::string line;
    status.clear();

    // create status reports for things waiting to be downloaded
    int cnt = downloadQueue.size();

    Debug::out(LOG_DEBUG, "Downloader::status() -- there are %d items in download queue", cnt);

    for(int i=0; i<cnt; i++) {
        TDownloadRequest &tdr = downloadQueue[i];

        if((tdr.downloadType & downloadTypeMask) == 0) {    // if the mask doesn't match the download type, skip it
            Debug::out(LOG_DEBUG, "Downloader::status() -- skiping item %d", i);
            continue;
        }

        Debug::out(LOG_DEBUG, "Downloader::status() -- format status for item %d", i);
        formatStatus(tdr, line);
        status += line + "\n";
    }

    // and for the currently downloaded thing
    if(downloadCurrent.downPercent < 100) {
        Debug::out(LOG_DEBUG, "Downloader::status() -- currently downloading something", cnt);

        if((downloadCurrent.downloadType & downloadTypeMask) != 0) {    // if the mask matches the download type, add it
            Debug::out(LOG_DEBUG, "Downloader::status() -- format status for currently downloaded item");

            formatStatus(downloadCurrent, line);
            status += line + "\n";
        }
    }

    pthread_mutex_unlock(&downloadQueueMutex);             // unlock the mutex
}

void Downloader::statusJson(std::ostringstream &status, int downloadTypeMask)
{
    pthread_mutex_lock(&downloadQueueMutex);               // try to lock the mutex

    status.clear();

    bool hasItem = false;       // list doesn't have anyitem yet

    // create status for the currently downloaded thing
    if(downloadCurrent.downPercent < 100) {
        if((downloadCurrent.downloadType & downloadTypeMask) != 0) {    // if the mask matches the download type, add it
            formatStatusJson(downloadCurrent, status);
            hasItem = true;     // we have one item now
        }
    }

    // create status reports for things waiting to be downloaded
    int cnt = downloadQueue.size();

    for(int i=0; i<cnt; i++) {
        TDownloadRequest &tdr = downloadQueue[i];

        if((tdr.downloadType & downloadTypeMask) == 0) {    // if the mask doesn't match the download type, skip it
            continue;
        }

        if(hasItem) {           // if something is in array, add separator
            status << ",";
            hasItem = true;     // list has at least 1 item
        }

        formatStatusJson(tdr, status);
    }

    pthread_mutex_unlock(&downloadQueueMutex);             // unlock the mutex
}

int Downloader::count(int downloadTypeMask)
{
    pthread_mutex_lock(&downloadQueueMutex);               // try to lock the mutex

    int typeCnt = 0;

    // create status reports for things waiting to be downloaded
    int cnt = downloadQueue.size();
    for(int i=0; i<cnt; i++) {
        TDownloadRequest &tdr = downloadQueue[i];

        if((tdr.downloadType & downloadTypeMask) == 0) {    // if the mask doesn't match the download type, skip it
            continue;
        }

        typeCnt++;                                          // increment count
    }

    // and for the currently downloaded thing
    if(downloadCurrent.downPercent < 100) {
        if((downloadCurrent.downloadType & downloadTypeMask) != 0) {    // if the mask matches the download type, add it
            typeCnt++;                                      // increment count
        }
    }

    pthread_mutex_unlock(&downloadQueueMutex);             // unlock the mutex

    return typeCnt;
}

bool Downloader::verifyChecksum(const char *filename, WORD checksum)
{
    if(checksum == 0) {                 // special case - when checksum is 0, don't check it buy say that it's OK
        Debug::out(LOG_DEBUG, "Downloader::verifyChecksum - file %s -- supplied 0 as checksum, so not doing checksum and returning that checksum is OK", filename);
        return true;
    }

    FILE *f = fopen(filename, "rb");

    if(!f) {
        Debug::out(LOG_ERROR, "Downloader::verifyChecksum - file %s -- failed to open file, checksum failed", filename);
        return false;
    }

    WORD cs = 0;
    WORD val, val2;

    while(!feof(f)) {                       // for whole file
        val = (BYTE) fgetc(f);              // get upper byte
        val = val << 8;

        if(!feof(f)) {                      // if not end of file
            val2 = (BYTE) fgetc(f);         // read lowe byte from file
        } else {
            val2 = 0;                       // if end of file, put a 0 there
        }

        val = val | val2;                   // create a word out of it

        cs += val;                          // add it to checksum
    }

    fclose(f);

    bool checksumIsGood = (checksum == cs);
    Debug::out(LOG_DEBUG, "Downloader::verifyChecksum - file %s -- checksum is good: %d", filename, (int) checksumIsGood);

    return checksumIsGood;                // return if the calculated cs is equal to the provided cs
}

// ZIPed floppy file image needs some extracting, searching and renaming of content
void Downloader::handleZIPedFloppyImage(std::string &zipFileName)
{
    // try to extract content and find 1st image usable in there
    std::string firstImage;
    bool res = Utils::unZIPfloppyImageAndReturnFirstImage(zipFileName.c_str(), firstImage);

    if(!res) {                  // failed to find image inside zip? quit
        Debug::out(LOG_DEBUG, "Downloader::handleZIPedFloppyImage - no valid image found in ZIP, not doing anything else");
        return;
    }

    // get extension of the 1st image
    const char *firstImageExt = Utils::getExtension(firstImage.c_str());

    if(firstImageExt == NULL) {
        Debug::out(LOG_DEBUG, "Downloader::handleZIPedFloppyImage - failed to get extension of first image, not doing anything else");
        return;
    }

    // replace the extension of input ZIP file with image file extension
    std::string firstImageExtLower = firstImageExt; // char * to std::string
    std::transform(firstImageExtLower.begin(), firstImageExtLower.end(), firstImageExtLower.begin(), ::tolower);    // convert to lowercase

    std::string newFileName;
    Utils::createPathWithOtherExtension(zipFileName, firstImageExtLower.c_str(), newFileName);  // create filename with the new extension

    // delete original ZIP file
    unlink(zipFileName.c_str());

    // move the first image into the place of downloaded ZIPed image
    std::string moveCmd = std::string("mv ") + firstImage + std::string(" ") + newFileName;
    Debug::out(LOG_DEBUG, "Downloader::handleZIPedFloppyImage - %s", moveCmd.c_str());
    system(moveCmd.c_str());
}

size_t Downloader::my_write_func_reportVersions(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    // for now just return fake written size
    return (size * nmemb);
}

size_t Downloader::my_write_func(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  return fwrite(ptr, size, nmemb, stream);
}

size_t Downloader::my_read_func(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  return fread(ptr, size, nmemb, stream);
}

int Downloader::my_progress_func(void *clientp, double downTotal, double downNow, double upTotal, double upNow)
{
    double percD;
    int percI;
    static int percIprev = 0;

    int abortTransfer = 0;                                      // by default - don't abort transfer
    if(sigintReceived != 0) {                                   // but if we did receive SIGINT, set abortTransfer
        abortTransfer = 1;
    }

    if(downTotal > 0.0) {
        percD = (downNow * 100.0) / downTotal;                  // calculate the percents
    } else {
        percD = 0.0;
    }

    percI = (int) percD;                                        // convert them to int

    if(percI == percIprev) {                                    // if percents didn't change, quit
        return abortTransfer;                                   // return 0 to continue transfer, or non-0 to abort transfer
    }
    percIprev = percI;

    if(clientp != NULL) {                                       // if got pointer to currently downloading request
        pthread_mutex_lock(&downloadQueueMutex);               // try to lock the mutex
        TDownloadRequest *tdr = (TDownloadRequest *) clientp;   // convert pointer type
        tdr->downPercent = percI;                               // mark that the download didn't start
        pthread_mutex_unlock(&downloadQueueMutex);             // unlock the mutex
    }

    return abortTransfer;                                       // return 0 to continue transfer, or non-0 to abort transfer
}

void Downloader::updateStatusByte(TDownloadRequest &tdr, BYTE newStatus)
{
    if(tdr.pStatusByte != NULL) {
        *tdr.pStatusByte = newStatus;           // store the new status
    }
}

void handleUsbUpdateFile(const char *localZipFile, const char *localDestDir)
{
    char command[1024];
    snprintf(command, 1023, "unzip -o '%s' -d /tmp/", localZipFile);          // unzip the update file to ce_update folder
    system(command);
}

void handleSendConfig(const char *localConfigFile, const char *remoteUrl)
{
    CURL       *curl = curl_easy_init();
    CURLcode    res;

    if(!curl) {
        Debug::out(LOG_ERROR, "CURL init failed, config was not sent to Jookie!");
        return;
    }

    //-------------
    FILE *f = fopen(localConfigFile, "rt");

    if(!f) {
        Debug::out(LOG_ERROR, "Could not open localConfigFile!");
        curl_easy_cleanup(curl);
        return;
    }

    fseek(f, 0, SEEK_END);              // seek to end of file
    int sz = ftell(f);                  // get current file pointer
    fseek(f, 0, SEEK_SET);              // seek back to beginning of file

    if(sz < 1) {                        // empty / non-existing file?
        Debug::out(LOG_ERROR, "Could not see the size of localConfigFile!");
        curl_easy_cleanup(curl);
        fclose(f);
        return;
    }

    sz = (sz < 50*1024) ? sz : (50 * 1024);                 // limit content to 50 kB

    char *bfrRaw = new char[3 * sz];                        // allocate memory
    fread(bfrRaw, 1, sz, f);                                // read whole file
    fclose(f);

    char *bfrEscaped = curl_easy_escape(curl, bfrRaw, sz);  // escape chars

    if(!bfrEscaped) {
        Debug::out(LOG_ERROR, "Could not escape config file!");
        curl_easy_cleanup(curl);
        delete []bfrRaw;                                    // free raw buffer
        return;
    }

    strcpy(bfrRaw, "action=sendconfig&config=");            // fill action and config tag
    strcat(bfrRaw, bfrEscaped);                             // append escaped config
    curl_free(bfrEscaped);                                  // free the escaped config

    //-------------
    // now fill curl stuff and do post
    curl_easy_setopt(curl, CURLOPT_URL,             remoteUrl);         // POST will go to this URL
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,      bfrRaw);            // this is the data that will be sent

    res = curl_easy_perform(curl);          // Perform the request, res will get the return code

    if(res != CURLE_OK) {
        Debug::out(LOG_ERROR, "CURL curl_easy_perform failed: %s", curl_easy_strerror(res));
    } else {
        Debug::out(LOG_DEBUG, "Config was sent to Jookie.");
    }

    delete []bfrRaw;                                    // free raw buffer
    curl_easy_cleanup(curl);
}

void handleLogHttp(const char *remoteUrl)
{
    CURL       *curl = curl_easy_init();
    CURLcode    res;

    if(!curl) {
        Debug::out(LOG_ERROR, "CURL init failed, config was not sent to Jookie!");
        return;
    }

    //-------------
    // now fill curl stuff and do get
    curl_easy_setopt(curl, CURLOPT_URL,             remoteUrl);         // GET will go to this URL
    //curl_easy_setopt(curl, CURLOPT_POSTFIELDS,      bfrRaw);            // this is the data that will be sent

    res = curl_easy_perform(curl);          // Perform the request, res will get the return code

    if(res != CURLE_OK) {
        Debug::out(LOG_ERROR, "CURL curl_easy_perform failed: %s", curl_easy_strerror(res));
    } else {
        Debug::out(LOG_DEBUG, "Config was sent to Jookie.");
    }

    curl_easy_cleanup(curl);
}

void Downloader::handleReportVersions(CURL *curl, const char *reportUrl)
{
    // specify url
    curl_easy_setopt(curl, CURLOPT_URL, reportUrl);

    // specify where the retrieved data should go
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_write_func_reportVersions);

    // specify the POST data
    char postFields[256];

    char appVersion[16];
    Version::getAppVersion(appVersion);

    sprintf(postFields, "mainapp=%s&distro=%s&rpirevision=%s&rpiserial=%s", appVersion, distroString, rpiConfig.revision, rpiConfig.serial);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields);

    // Perform the request, res will get the return code
    CURLcode res = curl_easy_perform(curl);

    // Check for errors
    if(res != CURLE_OK) {
        Debug::out(LOG_ERROR, "CURL curl_easy_perform() failed: %s", curl_easy_strerror(res));
    }

    // always cleanup
    curl_easy_cleanup(curl);
}

void Downloader::stop(void)
{
    pthread_mutex_lock(&downloadQueueMutex);
    shouldStop = true;
    pthread_cond_signal(&downloadQueueNotEmpty);
    pthread_mutex_unlock(&downloadQueueMutex);
}

void Downloader::run(void)
{
    CURLcode cres;
    int res;
    FILE *outfile;

    while(!shouldStop) {
        pthread_mutex_lock(&downloadQueueMutex);       // lock the mutex

        while(downloadQueue.size() == 0 && !shouldStop) {                 // nothing to do?
            pthread_cond_wait(&downloadQueueNotEmpty, &downloadQueueMutex);
        }
        if(shouldStop) {
            pthread_mutex_unlock(&downloadQueueMutex);     // unlock the mutex
            break;
        }

        downloadCurrent = downloadQueue.front();        // get the 'oldest' element from queue
        downloadQueue.erase(downloadQueue.begin());     // and remove it from queue
        pthread_mutex_unlock(&downloadQueueMutex);     // unlock the mutex

        Debug::out(LOG_DEBUG, "Downloader: downloading %s", downloadCurrent.srcUrl.c_str());
        //-------------------
        // check if this isn't local file, and if it is, do the rest localy
        res = access(downloadCurrent.srcUrl.c_str(), F_OK);

        if(res != -1) {                                                         // it's a local file!
            if(downloadCurrent.downloadType == DWNTYPE_UPDATE_LIST) {           // if it's a 'update list'
                handleUsbUpdateFile(downloadCurrent.srcUrl.c_str(), downloadCurrent.dstDir.c_str());
            } else if(downloadCurrent.downloadType == DWNTYPE_SEND_CONFIG) {            // if it should be local config upload / post
                handleSendConfig(downloadCurrent.srcUrl.c_str(), downloadCurrent.dstDir.c_str());
            } else if(downloadCurrent.downloadType == DWNTYPE_LOG_HTTP) {            // if it should be an http log request
                handleLogHttp(downloadCurrent.srcUrl.c_str());
            }
            continue;
        }

        //-------------------
        CURL *curl = curl_easy_init();

        if(!curl) {
            Debug::out(LOG_ERROR, "CURL init failed, the file %s was not downloaded...", downloadCurrent.srcUrl.c_str());
            continue;
        }

        //-------------------
        // if this is a request to report versions, do it in a separate function
        if(downloadCurrent.downloadType == DWNTYPE_REPORT_VERSIONS) {
            handleReportVersions(curl, downloadCurrent.srcUrl.c_str());
            curl = NULL;
            continue;
        }
        //-------------------

        std::string urlPath, fileName;
        Utils::splitFilenameFromPath(downloadCurrent.srcUrl, urlPath, fileName);

        std::string tmpFile, finalFile;

        updateStatusByte(downloadCurrent, DWNSTATUS_DOWNLOADING);       // mark that we're downloading

        finalFile = downloadCurrent.dstDir;
        Utils::mergeHostPaths(finalFile, fileName);         // create final local filename with path

        tmpFile = finalFile + "_dwnldng";                   // create temp local filename with path
        outfile = fopen(tmpFile.c_str(), "wb");    // try to open the tmp file

        if(!outfile) {
            Debug::out(LOG_ERROR, "Downloader - failed to create local file: %s", fileName.c_str());
            continue;
        }

        Debug::out(LOG_DEBUG, "Downloader - download remote file: %s to local file: %s", downloadCurrent.srcUrl.c_str(), fileName.c_str());

        // now configure curl
        curl_easy_setopt(curl, CURLOPT_URL,                 downloadCurrent.srcUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,           outfile);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,       my_write_func);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION,        my_read_func);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS,          0L);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION,    my_progress_func);
        curl_easy_setopt(curl, CURLOPT_PROGRESSDATA,        &downloadCurrent);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR,         true);

        cres = curl_easy_perform(curl);                      // start the transfer

        fclose(outfile);

        if(cres == CURLE_OK) {                               // if download went OK
            updateStatusByte(downloadCurrent, DWNSTATUS_VERIFYING);     // mark that we're verifying checksum

            //-----------
            // support for testing and automatic depacking of floppy images

            bool b = Downloader::verifyChecksum(tmpFile.c_str(), downloadCurrent.checksum);

            if(b) {             // checksum is OK?
                bool isZIPfile = Utils::isZIPfile(finalFile.c_str());

                Debug::out(LOG_ERROR, "Downloader - finalFile: %s, isZIPfile: %d", finalFile.c_str(), isZIPfile);

                res = rename(tmpFile.c_str(), finalFile.c_str());

                if(res != 0) {  // download OK, checksum OK, rename BAD?
                    updateStatusByte(downloadCurrent, DWNSTATUS_DOWNLOAD_FAIL);
                    Debug::out(LOG_ERROR, "Downloader - failed to rename %s to %s after download", tmpFile.c_str(), finalFile.c_str());
                } else {        // download OK, checksum OK, rename OK?
                    updateStatusByte(downloadCurrent, DWNSTATUS_DOWNLOAD_OK);
                    Debug::out(LOG_DEBUG, "Downloader - file %s was downloaded with success, is FLOPPYIMG: %d, is ZIP file: %d.", downloadCurrent.srcUrl.c_str(), downloadCurrent.downloadType == DWNTYPE_FLOPPYIMG, isZIPfile);

                    // if it's a floppy image download and it's a ZIP file
                    if(downloadCurrent.downloadType == DWNTYPE_FLOPPYIMG && isZIPfile) {
                        handleZIPedFloppyImage(finalFile);
                    }
                }
            } else {            // download OK, checksum bad?
                updateStatusByte(downloadCurrent, DWNSTATUS_DOWNLOAD_FAIL);

                res = remove(tmpFile.c_str());

                if(res == 0) {
                    Debug::out(LOG_ERROR, "Downloader - file %s was downloaded, but verifyChecksum() failed, so file %s was deleted.", tmpFile.c_str(), tmpFile.c_str());
                } else {
                    Debug::out(LOG_DEBUG, "Downloader - file %s was downloaded, but verifyChecksum() failed, and then failed to delete that file.", tmpFile.c_str());
                }
            }

            downloadCurrent.downPercent = 100;
        } else {                                            // if download failed
            updateStatusByte(downloadCurrent, DWNSTATUS_DOWNLOAD_FAIL);

            Debug::out(LOG_ERROR, "Downloader - download of %s didn't finish with success, deleting incomplete file.", downloadCurrent.srcUrl.c_str());

            res = remove(tmpFile.c_str());

            if(res != 0) {
                Debug::out(LOG_ERROR, "Downloader - failed to delete file %s", tmpFile.c_str());
            }
        }

        curl_easy_cleanup(curl);
        curl = NULL;
    }
}

void *downloadThreadCode(void *ptr)
{
    Debug::out(LOG_DEBUG, "Download thread starting...");

    Downloader::run();

    Debug::out(LOG_DEBUG, "Download thread finished.");
    return 0;
}

