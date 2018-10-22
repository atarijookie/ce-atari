#include "webserver/webserver.h"

#include <string>
#include <sstream>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/input.h>
#include <unistd.h>

#include "lib/cjson-code-58/cJSON.h"
#include "debug.h"
#include "global.h"
#include "downloadresource.h"
#include "../../../floppy/imagelist.h"
#include "../../../floppy/imagestorage.h"
#include "../../../utils.h"
#include "../../../downloader.h"
#include "../../../periodicthread.h"

extern SharedObjects shared;

DownloadResource::DownloadResource()
{
}

DownloadResource::~DownloadResource()
{
}

void DownloadResource::sendResponse(mg_connection *conn, std::ostringstream &stringStream)
{
    std::string sJson = stringStream.str();
    mg_printf(conn, "Content-Length: %lu\r\n\r\n",(unsigned long) sJson.length());	// Always set Content-Length
    mg_write(conn, sJson.c_str(), sJson.length());	// send content
}

void DownloadResource::onGetImageList(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo)
{
    std::ostringstream stringStream;

    if(!shared.imageList->exists()) {               // if the file does not yet exist, tell ST that we're downloading
        stringStream << "{\"imagelist\": \"not_exists\", \"totalPages\": 0, \"currentPage\": 0}";
        sendResponse(conn, stringStream);
        return;
    }

    if(!shared.imageList->loadList()) {      // try to load the list, if failed, error
        stringStream << "{\"imagelist\": \"not_loaded\", \"totalPages\": 0, \"currentPage\": 0}";
        sendResponse(conn, stringStream);
        return;
    }

    // list exists and is loaded, we can work with it
    const char *qs = req_info->query_string;
    int qs_len = (qs == NULL) ? 0 : strlen(qs);     // get length of query string

    char searchString[128];
    memset(searchString, 0, sizeof(searchString));  // clear the buffer which will receive the search string

    int res = -1;
    if(qs_len > 0) {    // if the query string contains something, we can try to get some vars
        res = mg_get_var(qs, qs_len, "search", searchString, sizeof(searchString) - 1); // try to get the search string
    }

    if(res > 0) {   // if got valid search string
        shared.imageList->search(searchString);
    } else {        // invalid search string
        shared.imageList->search("");
    }

    int page = 0;
    if(qs_len > 0) {    // if the query string contains something, we can try to get some vars
        char pageString[4];
        memset(pageString, 0, sizeof(pageString));

        res = mg_get_var(qs, qs_len, "page", pageString, sizeof(pageString) - 1); // try to get the page string

        if(res > 0) {   // if page string was found, try to get it as int
            res = sscanf(pageString, "%d", &page);
        }
    }

    // future improvement: get pageSize from request
    int pageSize = 15;

    int pageStart = page * pageSize;            // starting index of this page
    int pageEnd = (page + 1) * pageSize;        // ending index of this page (actually start of new page)

    int results = shared.imageList->getSearchResultsCount();

    pageStart = MIN(pageStart, results);
    pageEnd = MIN(pageEnd, results);

    int realPage = pageStart / pageSize;         // calculate the real page number
    int totalPages = (results / pageSize) + 1;   // calculate the count of pages we have

    stringStream << "{\"totalPages\": " << totalPages << ", ";
    stringStream << "\"currentPage\": " << realPage << ", ";
    stringStream << "\"imageList\": ["; // start of image list

    for(int i=pageStart; i<pageEnd; i++) {  // fill in the image list
        shared.imageList->getResultByIndex(i, stringStream);
    }

    stringStream << "]}";               // end of image list
    sendResponse(conn, stringStream);
}

void DownloadResource::onGetDownloadingList(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo)
{
    std::ostringstream stringStream;

    stringStream << "{\"downloading\": ["; // start of list

    Downloader::statusJson(stringStream, DWNTYPE_FLOPPYIMG);    // get list of what is now downloading

    stringStream << "]}";               // end of list
    sendResponse(conn, stringStream);
}

void DownloadResource::onDownloadItem(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo)
{
    std::ostringstream stringStream;

    const char *qs = req_info->query_string;
    int qs_len = (qs == NULL) ? 0 : strlen(qs);     // get length of query string

    if(qs_len == 0) {   // no query string? fail
        stringStream << "{\"status\": \"error\", \"reason\": \"no query string\"}";
        sendResponse(conn, stringStream);
        return;
    }

    // try to get the image name
    char imageName[128];
    memset(imageName, 0, sizeof(imageName));

    int res = -1;
    res = mg_get_var(qs, qs_len, "image", imageName, sizeof(imageName) - 1);

    if(res < 0) {   // failed?
        stringStream << "{\"status\": \"error\", \"reason\": \"no image name\"}";
        sendResponse(conn, stringStream);
        return;
     }

    // check if there is some storage available
    bool bres = shared.imageStorage->doWeHaveStorage();

    if(!bres) {
        stringStream << "{\"status\": \"error\", \"reason\": \"no storage available\"}";
        sendResponse(conn, stringStream);
        return;
    }

    // check if we got this floppy image file, and if we do, just return OK
    bres = shared.imageStorage->weHaveThisImage(imageName);

    if(bres) {      // we got this image, no need to download
        stringStream << "{\"status\": \"ok\"}";
        sendResponse(conn, stringStream);
        return;
    }

    // get full image url into download request url
    TDownloadRequest tdr;

    bres = shared.imageList->getImageUrl(imageName, tdr.srcUrl);   // try to get source URL

    if(!bres) {     // failed to get URL? fail
        stringStream << "{\"status\": \"error\", \"reason\": \"couldn't get url\"}";
        sendResponse(conn, stringStream);
        return;
    }

    std::string storagePath;
    shared.imageStorage->getStoragePath(storagePath);   // get path of where we should store the images

    // start downloading the image
    tdr.checksum        = 0;                    // special case - don't check checsum
    tdr.dstDir          = storagePath;          // save it here
    tdr.downloadType    = DWNTYPE_FLOPPYIMG;
    tdr.pStatusByte     = NULL;                 // don't update this status byte
    Downloader::add(tdr);

    // return the response
    stringStream << "{\"status\": \"ok\"}";
    sendResponse(conn, stringStream);
}

void DownloadResource::onInsertItem(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo)
{
    std::ostringstream stringStream;

    const char *qs = req_info->query_string;
    int qs_len = (qs == NULL) ? 0 : strlen(qs);     // get length of query string

    if(qs_len == 0) {   // no query string? fail
        stringStream << "{\"status\": \"error\", \"reason\": \"no query string\"}";
        sendResponse(conn, stringStream);
        return;
    }

    // try to get the image name
    char imageName[128];
    memset(imageName, 0, sizeof(imageName));

    int res = -1;
    res = mg_get_var(qs, qs_len, "image", imageName, sizeof(imageName) - 1);

    if(res < 0) {   // failed?
        stringStream << "{\"status\": \"error\", \"reason\": \"no image name\"}";
        sendResponse(conn, stringStream);
        return;
    }

    // try to get slot number as string
    char slotString[4];
    memset(slotString, 0, sizeof(slotString));
    res = mg_get_var(qs, qs_len, "slot", slotString, sizeof(slotString) - 1);

    if(res < 0) {   // failed?
        stringStream << "{\"status\": \"error\", \"reason\": \"no slot number\"}";
        sendResponse(conn, stringStream);
        return;
    }

    int slotNo = -1;    // try to convert slot number from string to int
    res = sscanf(slotString, "%d", &slotNo);

    if(res < 1 || slotNo < 0 || slotNo > 2) {   // failed to convert string to int or bad slot number? fail
        stringStream << "{\"status\": \"error\", \"reason\": \"slot string to int failed\"}";
        sendResponse(conn, stringStream);
        return;
    }

    // check if we got this floppy image file
    bool bres = shared.imageStorage->weHaveThisImage(imageName);

    if(!bres) { // we don't have this image downloaded? fail
        stringStream << "{\"status\": \"error\", \"reason\": \"we don't have this image downloaded\"}";
        sendResponse(conn, stringStream);
        return;
    }

    // get local path for this image
    std::string localImagePath;
    shared.imageStorage->getImageLocalPath(imageName, localImagePath);


    // TODO: set floppy image to slot



    // return the response
    stringStream << "{\"status\": \"ok\"}";
    sendResponse(conn, stringStream);
}

bool DownloadResource::dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo)
{
	const char *path = sResourceInfo.c_str();

    Debug::out(LOG_DEBUG, "DownloadResource::dispatch -- %s", path);

    if( strstr(req_info->request_method,"GET")==0 && strstr(req_info->request_method,"POST")==0 && strstr(req_info->request_method,"PUT")==0 ){
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed %s\r\n",req_info->request_method);
        return true;
    }

	// for GET methods
    if( strstr(req_info->request_method,"GET")>0 ) {
        // standard header on response
        mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n");
        mg_printf(conn, "Cache: no-cache\r\n");

        if(strcmp(path, "imagelist") == 0) {		// url: download/imagelist -- return list images which we can search trhough
            pthread_mutex_lock(&shared.mtxImages);      // lock images objects - download resource is using them

            onGetImageList(conn, req_info, sResourceInfo);

            pthread_mutex_unlock(&shared.mtxImages);    // unlock images objects
            return true;
        }

        if(strcmp(path, "downloading") == 0) {		// url: download/downloading -- return list images which are now downloaded
            pthread_mutex_lock(&shared.mtxImages);      // lock images objects - download resource is using them

            onGetDownloadingList(conn, req_info, sResourceInfo);

            pthread_mutex_unlock(&shared.mtxImages);    // unlock images objects
            return true;
        }

        if(strcmp(path, "download") == 0) {		    // url: download/download -- download this file from the image list
            pthread_mutex_lock(&shared.mtxImages);      // lock images objects - download resource is using them

            onDownloadItem(conn, req_info, sResourceInfo);

            pthread_mutex_unlock(&shared.mtxImages);    // unlock images objects
            return true;
        }

        if(strcmp(path, "insert") == 0) {		    // url: download/insert -- insert this file from the image list into specified floppy slot
            pthread_mutex_lock(&shared.mtxImages);      // lock images objects - download resource is using them

            onInsertItem(conn, req_info, sResourceInfo);

            pthread_mutex_unlock(&shared.mtxImages);    // unlock images objects
            return true;
        }
	}

    return false;
}
