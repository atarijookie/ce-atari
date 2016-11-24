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

void updateStatusByte(TDownloadRequest &tdr, BYTE newStatus);

pthread_mutex_t downloadThreadMutex = PTHREAD_MUTEX_INITIALIZER;
std::vector<TDownloadRequest>   downloadQueue;
TDownloadRequest                downloadCurrent;

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
    pthread_mutex_lock(&downloadThreadMutex);               // try to lock the mutex
    tdr.downPercent = 0;                                    // mark that the download didn't start
    updateStatusByte(tdr, DWNSTATUS_WAITING);               // mark that the download didn't start
    downloadQueue.push_back(tdr);                           // add this to queue
    pthread_mutex_unlock(&downloadThreadMutex);             // unlock the mutex
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
}

void Downloader::status(std::string &status, int downloadTypeMask)
{
    pthread_mutex_lock(&downloadThreadMutex);               // try to lock the mutex

    std::string line;
    status.clear();

    // create status reports for things waiting to be downloaded
    int cnt = downloadQueue.size();
    for(int i=0; i<cnt; i++) {
        TDownloadRequest &tdr = downloadQueue[i];

        if((tdr.downloadType & downloadTypeMask) == 0) {    // if the mask doesn't match the download type, skip it
            continue;
        }

        formatStatus(tdr, line);
        status += line + "\n";
    }    

    // and for the currently downloaded thing
    if(downloadCurrent.downPercent < 100) {
        if((downloadCurrent.downloadType & downloadTypeMask) != 0) {    // if the mask matches the download type, add it
            formatStatus(downloadCurrent, line);
            status += line + "\n";
        }
    }

    pthread_mutex_unlock(&downloadThreadMutex);             // unlock the mutex
}

int Downloader::count(int downloadTypeMask)
{
    pthread_mutex_lock(&downloadThreadMutex);               // try to lock the mutex

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

    pthread_mutex_unlock(&downloadThreadMutex);             // unlock the mutex

    return typeCnt;
}

bool Downloader::verifyChecksum(char *filename, WORD checksum)
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

bool Downloader::handleZIPedImage(const char *destDirectory, const char *zipFilePath)
{
    system("rm    -rf /tmp/zipedfloppy");           // delete this dir, if it exists
    system("mkdir -p  /tmp/zipedfloppy");           // create that dir
    
    char unzipCommand[512];
    sprintf(unzipCommand, "unzip -o %s -d /tmp/zipedfloppy", zipFilePath);
    system(unzipCommand);                           // unzip the downloaded ZIP file into that tmp directory
    
    // find the first usable floppy image
    DIR *dir = opendir("/tmp/zipedfloppy");         // try to open the dir
    
    if(dir == NULL) {                               // not found?
        return false;
    }

    bool found          = false;
    struct dirent *de   = NULL;
    
    while(1) {                                      // avoid buffer overflow
        de = readdir(dir);                          // read the next directory entry
    
        if(de == NULL) {                            // no more entries?
            break;
        }

        if(de->d_type != DT_REG) {                  // not a file? skip it
            continue;
        }

        int fileNameLen = strlen(de->d_name);       // get length of filename

        if(fileNameLen < 3) {                       // if it's too short, skip it
            continue;
        }

        char *pExt = de->d_name + fileNameLen - 3;  // get pointer to extension 

        if(strcasecmp(pExt, ".st") == 0 || strcasecmp(pExt, "msa") == 0) {  // the extension of the file is valid for a floppy image? 
            found = true;
            break;
        }
    }

    closedir(dir);                                  // close the dir
    
    if(found) {                                     // if we got some valid floppy image
        unlink(zipFilePath);                        // delete the downloaded ZIP file
        
        char moveCommand[512];
        sprintf(moveCommand, "mv -f /tmp/zipedfloppy/%s %s", de->d_name, destDirectory);
        system(moveCommand);                        // move the file to destination directory
    }
    
    return found;
}

static size_t my_write_func_reportVersions(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    // for now just return fake written size
    return (size * nmemb);
}

static size_t my_write_func(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  return fwrite(ptr, size, nmemb, stream);
}
 
static size_t my_read_func(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
  return fread(ptr, size, nmemb, stream);
}

static int my_progress_func(void *clientp, double downTotal, double downNow, double upTotal, double upNow)
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
        pthread_mutex_lock(&downloadThreadMutex);               // try to lock the mutex
        TDownloadRequest *tdr = (TDownloadRequest *) clientp;   // convert pointer type
        tdr->downPercent = percI;                               // mark that the download didn't start
        pthread_mutex_unlock(&downloadThreadMutex);             // unlock the mutex
    }
    
    return abortTransfer;                                       // return 0 to continue transfer, or non-0 to abort transfer
}

void updateStatusByte(TDownloadRequest &tdr, BYTE newStatus) 
{
    if(tdr.pStatusByte == NULL) {           // don't have pointer? skip this
        return;
    }
    
    *tdr.pStatusByte = newStatus;           // store the new status
}

void handleUsbUpdateFile(char *localZipFile, char *localDestDir)
{
    char command[1024];
    snprintf(command, 1023, "unzip -o %s -d /tmp/", localZipFile);          // unzip the update file to ce_update folder
    system(command);
}

void handleSendConfig(char *localConfigFile, char *remoteUrl)
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

void handleReportVersions(CURL *curl, const char *reportUrl)
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

void *downloadThreadCode(void *ptr)
{
    CURLcode cres;
    int res;
    FILE *outfile;

    Debug::out(LOG_DEBUG, "Download thread starting...");

    while(sigintReceived == 0) {
        pthread_mutex_lock(&downloadThreadMutex);       // lock the mutex

        if(downloadQueue.size() == 0) {                 // nothing to do?
            pthread_mutex_unlock(&downloadThreadMutex); // unlock the mutex
            sleep(1);                                   // wait 1 second and try again
            continue;
        }
        
        downloadCurrent = downloadQueue.front();        // get the 'oldest' element from queue
        downloadQueue.erase(downloadQueue.begin());     // and remove it from queue
        pthread_mutex_unlock(&downloadThreadMutex);     // unlock the mutex
    
        //-------------------
        // check if this isn't local file, and if it is, do the rest localy
        res = access((char *) downloadCurrent.srcUrl.c_str(), F_OK);

        if(res != -1) {                                                         // it's a local file!
            if(downloadCurrent.downloadType == DWNTYPE_UPDATE_LIST) {           // if it's a 'update list'
                handleUsbUpdateFile((char *) downloadCurrent.srcUrl.c_str(), (char *) downloadCurrent.dstDir.c_str());
            }           
            
            if(downloadCurrent.downloadType== DWNTYPE_SEND_CONFIG) {            // if it should be local config upload / post
                handleSendConfig((char *) downloadCurrent.srcUrl.c_str(), (char *) downloadCurrent.dstDir.c_str());
            }
            
            continue;
        }
        
        //-------------------
        CURL *curl = curl_easy_init();
        
        if(!curl) {
            Debug::out(LOG_ERROR, "CURL init failed, the file %s was not downloaded...", (char *) downloadCurrent.srcUrl.c_str());
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
        outfile = fopen((char *) tmpFile.c_str(), "wb");    // try to open the tmp file

        if(!outfile) {
            Debug::out(LOG_ERROR, "Downloader - failed to create local file: %s", (char *) fileName.c_str());
            continue;
        }

        Debug::out(LOG_DEBUG, "Downloader - download remote file: %s to local file: %s", (char *) downloadCurrent.srcUrl.c_str(), (char *) fileName.c_str());
     
        // now configure curl
        curl_easy_setopt(curl, CURLOPT_URL,                 (char *) downloadCurrent.srcUrl.c_str());
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
            bool isZIPedFloppyImage = false;
            
            if(downloadCurrent.downloadType == DWNTYPE_FLOPPYIMG) {                     // it's a floppy image download?
                if(finalFile.length() >= 4) {                                           // if the length is long enough to contain '.zip' 
                    std::string extension = finalFile.substr(finalFile.length() - 4);   // get last 4 characters
                    int iRes = strcasecmp(extension.c_str(), ".zip");                   // compare the extension to .ZIP
                    
                    if(iRes == 0) {                                                     // if the extension is .ZIP, then it's a ZIPed floppy image
                        isZIPedFloppyImage = true;
                    }
                }
            }

            //-----------
            
            bool b = Downloader::verifyChecksum((char *) tmpFile.c_str(), downloadCurrent.checksum);

            if(b) {             // checksum is OK?
                if(isZIPedFloppyImage) {    // it's a ZIPed floppy?
                    res = Downloader::handleZIPedImage(downloadCurrent.dstDir.c_str(), tmpFile.c_str());
                } else {                    // not ZIPed floppy, just rename it
                    res = rename(tmpFile.c_str(), finalFile.c_str());
                }

                if(res != 0) {  // download OK, checksum OK, rename BAD?
                    updateStatusByte(downloadCurrent, DWNSTATUS_DOWNLOAD_FAIL);
                    Debug::out(LOG_ERROR, "Downloader - failed to rename %s to %s after download", (char *) tmpFile.c_str(), (char *) finalFile.c_str());
                } else {        // download OK, checksum OK, rename OK?
                    updateStatusByte(downloadCurrent, DWNSTATUS_DOWNLOAD_OK);
                    Debug::out(LOG_DEBUG, "Downloader - file %s was downloaded with success.", (char *) downloadCurrent.srcUrl.c_str());
                }
            } else {            // download OK, checksum bad?
                updateStatusByte(downloadCurrent, DWNSTATUS_DOWNLOAD_FAIL);
                
                res = remove(tmpFile.c_str());

                if(res == 0) {
                    Debug::out(LOG_ERROR, "Downloader - file %s was downloaded, but verifyChecksum() failed, so file %s was deleted.", (char *) tmpFile.c_str(), (char *) tmpFile.c_str());
                } else {
                    Debug::out(LOG_DEBUG, "Downloader - file %s was downloaded, but verifyChecksum() failed, and then failed to delete that file.", (char *) tmpFile.c_str());
                }
            }

            downloadCurrent.downPercent = 100;
        } else {                                            // if download failed
            updateStatusByte(downloadCurrent, DWNSTATUS_DOWNLOAD_FAIL);

            Debug::out(LOG_ERROR, "Downloader - download of %s didn't finish with success, deleting incomplete file.", (char *) downloadCurrent.srcUrl.c_str());

            res = remove(tmpFile.c_str());

            if(res != 0) {
                Debug::out(LOG_ERROR, "Downloader - failed to delete file %s", (char *) tmpFile.c_str());
            }
        }

        curl_easy_cleanup(curl);
        curl = NULL;
    }

    Debug::out(LOG_DEBUG, "Download thread finished.");
    return 0;
}

