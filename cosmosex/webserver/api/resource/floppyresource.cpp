#include "floppyresource.h"
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

#include "../../../lib/cjson-code-58/cJSON.h"
#include "../../../debug.h"

#include "service/floppyservice.h"

FloppyResource::FloppyResource(FloppyService *pxFloppyService) : pxFloppyService(pxFloppyService)  
{
}

FloppyResource::~FloppyResource() 
{
}

bool FloppyResource::dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo /*=""*/ ) 
{
    Debug::out(LOG_DEBUG, "FloppyResource::dispatch");

    if( strstr(req_info->request_method,"GET")==0 && strstr(req_info->request_method,"POST")==0 ){
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed %s\r\n",req_info->request_method);
        return true;
    }

    //return slot info
    if( strstr(req_info->request_method,"GET")>0 ){
        const char *qs = req_info->query_string;
        mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n");
        mg_printf(conn, "Cache: no-cache\r\n");
        std::ostringstream stringStream;
        stringStream << "{\"slots\":[";
        stringStream << "\"" << pxFloppyService->getImageName(0) << "\",";
        stringStream << "\"" << pxFloppyService->getImageName(1) << "\",";
        stringStream << "\"" << pxFloppyService->getImageName(2) << "\"";
        stringStream << "]}"; 
        std::string sJson=stringStream.str();
        mg_printf(conn, "Content-Length: %d\r\n\r\n",sJson.length());        // Always set Content-Length
        mg_printf(conn, sJson.c_str());        // Always set Content-Length
        return true;
    }
     
    //upload image to slot
    if( strstr(req_info->request_method,"POST")>0 ){
        Debug::out(LOG_DEBUG, "/floppy POST");

        int iSlot=atoi(sResourceInfo.c_str());        
        if( iSlot<0 || iSlot>2 )
        {
            mg_printf(conn, "HTTP/1.1 400 Selected slot not in range 0-2\r\n");
            return true;
        }

        int iFiles=mg_upload(conn, "/tmp");

        if( iFiles!=1 )
        {
            mg_printf(conn, "HTTP/1.1 400 No file data\r\n");
            return true;
        }

        pxFloppyService->setImage(iSlot,WebServer::sLastUploadedFile);
        
        //just a test
        pxFloppyService->setActiveSlot(iSlot);

        mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
        mg_printf(conn, "<html><body>\n"); 
        mg_printf(conn, "Slot %d\n",iSlot); 
        mg_printf(conn, "File %d %s\n",iFiles,WebServer::sLastUploadedFile.c_str()); 
        mg_printf(conn, "</body></html>\n"); 
        return true;    
    }

    return false;
}
