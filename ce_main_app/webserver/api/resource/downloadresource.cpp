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
#include "../../../utils.h"

DownloadResource::DownloadResource()
{
    imageList = new ImageList();
}

DownloadResource::~DownloadResource()
{
    delete imageList;
}

void DownloadResource::onGetImageList(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo)
{
    std::ostringstream stringStream;

    if(!imageList->exists()) {               // if the file does not yet exist, tell ST that we're downloading
        stringStream << "{\"imagelist\": \"not_exists\", \"totalPages\": 0, \"currentPage\": 0}";
    } else if(!imageList->loadList()) {      // try to load the list, if failed, error
        stringStream << "{\"imagelist\": \"not_loaded\", \"totalPages\": 0, \"currentPage\": 0}";
    } else {                                // list exists and is loaded, we can work with it
        const char *qs = req_info->query_string;
        int qs_len = (qs == NULL) ? 0 : strlen(qs);     // get length of query string

        char searchString[128];
        memset(searchString, 0, sizeof(searchString));  // clear the buffer which will receive the search string

        int res = -1;
        if(qs_len > 0) {    // if the query string contains something, we can try to get some vars
            res = mg_get_var(qs, qs_len, "search", searchString, sizeof(searchString) - 1); // try to get the search string
        }

        if(res > 0) {   // if got valid search string
            imageList->search(searchString);
        } else {        // invalid search string
            imageList->search("");
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

        int results = imageList->getSearchResultsCount();

        pageStart = MIN(pageStart, results);
        pageEnd = MIN(pageEnd, results);

        int realPage = pageStart / pageSize;         // calculate the real page number
        int totalPages = (results / pageSize) + 1;   // calculate the count of pages we have

        stringStream << "{\"totalPages\": " << totalPages << ", ";
        stringStream << "\"currentPage\": " << realPage << ", ";
        stringStream << "\"imageList\": ["; // start of image list

        for(int i=pageStart; i<pageEnd; i++) {  // fill in the image list
            imageList->getResultByIndex(i, stringStream);
        }

        stringStream << "]}";               // end of image list
    }

    std::string sJson = stringStream.str();
    mg_printf(conn, "Content-Length: %lu\r\n\r\n",(unsigned long) sJson.length());	// Always set Content-Length
    mg_write(conn, sJson.c_str(), sJson.length());	// send content
    return;
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
    if( strstr(req_info->request_method,"GET")>0 ){
        // standard header on response
        mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n");
        mg_printf(conn, "Cache: no-cache\r\n");

        if(strcmp(path, "havefiles") == 0) {		// download/havefiles -- return list of files we already have
            std::ostringstream stringStream;
            stringStream << "{\"havefiles\": [\"A_008.zip\", \"A_013.zip\", \"A_020.zip\"]}";

            std::string sJson = stringStream.str();
            mg_printf(conn, "Content-Length: %lu\r\n\r\n",(unsigned long) sJson.length());	// Always set Content-Length
            mg_write(conn, sJson.c_str(), sJson.length());	// send content
            return true;
        }

        if(strcmp(path, "imagelist") == 0) {		// download/imagelist -- return list images which we can search trhough
            onGetImageList(conn, req_info, sResourceInfo);
            return true;
        }
	}

    return false;
}
