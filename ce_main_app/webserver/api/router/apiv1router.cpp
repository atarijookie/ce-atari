#include "apiv1router.h"

#include <string>
#include <stdlib.h> 

#include "../resource/mouseresource.h"
#include "../resource/kbdresource.h"
#include "../resource/floppyresource.h"
#include "../resource/downloadresource.h"

#include "../../../debug.h"

ApiV1Router::ApiV1Router(VirtualKeyboardService* pxVKbdService,VirtualMouseService* pxVMouseService,FloppyService* pxFloppyService)
{
    //initialize the resources that we are exposing through this router
    pxMouse=new MouseResource(pxVMouseService);
    pxKeyboard=new KbdResource(pxVKbdService);
    pxFloppy=new FloppyResource(pxFloppyService);
    pxDownload=new DownloadResource(pxFloppyService);
    pxConfig=new ConfigResource();
}

ApiV1Router::~ApiV1Router()
{
    //destroy the resources
    delete pxMouse;
    delete pxKeyboard;
    delete pxFloppy;
    delete pxConfig;
    delete pxDownload;
    pxMouse=NULL;
    pxKeyboard=NULL;
    pxFloppy=NULL;
    pxConfig=NULL;
    pxDownload=NULL;
}

bool ApiV1Router::handlePost(CivetServer *server, struct mg_connection *conn)
{
    return handleGet(server, conn);
}

bool ApiV1Router::handlePut(CivetServer *server, struct mg_connection *conn)
{
    return handleGet(server, conn);
}

bool ApiV1Router::handleGet(CivetServer *server, struct mg_connection *conn)
{
    Debug::out(LOG_DEBUG, "Webserver dispatch ApiV1Router");

    /* Handler may access the request info using mg_get_request_info */
    struct mg_request_info * req_info = mg_get_request_info(conn);

    /* well, we don't have C++11,
    std::regex e ("\\A/api/v1/(.*)/");
    std::smatch m;
    if(std::regex_search(std:.string(req_info->uri),m,e))
    {
        mg_printf(conn, *m.begin());
    }
    */

    std::string sResource(req_info->uri);
    sResource.erase(0,8);

    //dispatch resources
    if( sResource=="mouse" )
    {
        bool processed=pxMouse->dispatch(conn,req_info);
        return processed;
    } else if( sResource=="keyboard" )
    {
        bool processed=pxKeyboard->dispatch(conn,req_info);
        return processed;
    } else if( sResource.substr(0,6)=="floppy" )
    {
        std::string sResourceInfo=sResource;
        sResourceInfo.erase(0,7);
        bool processed=pxFloppy->dispatch(conn,req_info,sResourceInfo);
        return processed;
    } else if( sResource.substr(0,6)=="config" )
    {
        std::string sResourceInfo=sResource;
        sResourceInfo.erase(0,7);
        bool processed=pxConfig->dispatch(conn,req_info,sResourceInfo);
        return processed;
    } else if( sResource.substr(0,8)=="download" )
    {
        std::string sResourceInfo=sResource;
        sResourceInfo.erase(0,9);
        bool processed=pxDownload->dispatch(conn,req_info,sResourceInfo);
        return processed;
    }
    mg_printf(conn, "HTTP/1.1 404 Not Found\r\n");
    return true;

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    mg_printf(conn, "<html><body>");
    mg_printf(conn, "<h2>This is the Api handler</h2>");
    mg_printf(conn, "Resource: GET ");
    mg_write (conn, sResource.c_str(), sResource.length());
    mg_printf(conn, "<p>The request was:<br><pre>%s %s HTTP/%s</pre></p>",
              req_info->request_method, req_info->uri, req_info->http_version);
    mg_printf(conn, "</body></html>\n");
    return true;
}
