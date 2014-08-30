#include "configresource.h"
#include "webserver/webserver.h"

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <linux/input.h>
#include <unistd.h>

#include "../../../lib/cjson-code-58/cJSON.h"
#include "../../../debug.h"

ConfigResource::ConfigResource()  
{
}

ConfigResource::~ConfigResource() 
{
}

bool ConfigResource::dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo /*=""*/ ) 
{
    Debug::out(LOG_DEBUG, "ConfigResource::dispatch");

    if( strstr(req_info->request_method,"GET")==0 && strstr(req_info->request_method,"POST")==0 ){
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed %s\r\n",req_info->request_method);
        return true;
    }

    //return config info
    if( strstr(req_info->request_method,"GET")>0 ){
        Debug::out(LOG_DEBUG, "/config GET");
        const char *qs = req_info->query_string;
        mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n");
        mg_printf(conn, "Cache: no-cache\r\n");
        std::ostringstream stringStream;
        stringStream << "{\"config\":[";
        stringStream << "]}"; 
        std::string sJson=stringStream.str();
        mg_printf(conn, "Content-Length: %d\r\n\r\n",sJson.length());        // Always set Content-Length
        mg_printf(conn, sJson.c_str());        // Always set Content-Length
        return true;
    }
     
    //set loglevel
    if( strstr(req_info->request_method,"POST")>0 && sResourceInfo=="loglevel" ){
        Debug::out(LOG_DEBUG, "/config/loglevel POST");

        char post_data[1024], loglevel[sizeof(post_data)];
        int post_data_len = mg_read(conn, post_data, sizeof(post_data));

        // Parse form data. input1 and input2 are guaranteed to be NUL-terminated
        mg_get_var(post_data, post_data_len, "loglevel", loglevel, sizeof(loglevel));
        std::string sLogLevel=std::string(loglevel);
        if( sLogLevel!="ll1" && sLogLevel!="ll2" && sLogLevel!="ll3" && sLogLevel!="" )
        {
            mg_printf(conn, "HTTP/1.1 400 unknown loglevel\r\n");
            return true;
        }

        std::ofstream fileLogLevel("/tmp/ce_startupmode");
        fileLogLevel << sLogLevel;

        mg_printf(conn, "HTTP/1.1 200 OK\r\n\r\n");
        return true;    
    }

    return false;
}
