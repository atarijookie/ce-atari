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
        stringStream << "{\"imagelist\": \"not_exists\"}";
    } else if(!imageList->loadList()) {      // try to load the list, if failed, error
        stringStream << "{\"imagelist\": \"not_loaded\"}";
    } else {                                // list exists and is loaded, we can work with it
        const char *qs = req_info->query_string;
        int qs_len = (qs == NULL) ? 0 : strlen(qs);     // get length of query string

        char searchString[64];
        memset(searchString, 0, sizeof(searchString));  // clear the buffer which will receive the search string

        int res = -1;
        if(qs_len > 0) {    // if the query string contains something, we can try to get some vars
            res = mg_get_var(qs, qs_len, "search", searchString, sizeof(searchString - 1)); // try to get the search string
        }

        if(res > 0) {   // if got valid search string
            imageList->search(searchString);
        } else {        // invalid search string
            imageList->search("");
        }

        // TODO:
        // - retrieve page from request
        // - retrieve search result from imageList and pass it to stringStream


        stringStream << "{\"imagelist\": \"http://www.exxoshost.co.uk/atari/games/automation/files/A_000.zip,0x0000,Sinbad and the Throne of the Falcon,Myth\\nhttp://www.exxoshost.co.uk/atari/games/automation/files/A_001.zip,0x0000,Crystal Castles,Star Wars: The Empire Strikes Back,Northstar w/ docs,Star Raiders,Robotron: 2084,Trail Blazer,Football Manager: World Cup Edition\\nhttp://www.exxoshost.co.uk/atari/games/automation/files/A_002.zip,0x0000,Into the Eagle's Nest,Trantor: The Last Stormtrooper,Extensor,Knightmare,Plutos,Virus,Joe Blade,Trifide II\\n\"}";
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
