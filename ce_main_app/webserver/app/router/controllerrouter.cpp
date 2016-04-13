#include "controllerrouter.h"

#include <string>
//#include <regex>

#include "../controller/configcontroller.h"
#include "../controller/statuscontroller.h"
#include "../controller/debugcontroller.h"
#include "../controller/screencastcontroller.h"
#include "../../../config/configstream.h"
#include "../../../debug.h"
#include "../../../global.h"

ControllerRouter::ControllerRouter(ConfigService* pxDateService, FloppyService* pxFloppyService, ScreencastService *pxScreencastService):
    pxDateService(pxDateService),pxFloppyService(pxFloppyService),pxScreencastService(pxScreencastService)
{
}

bool ControllerRouter::handleGet(CivetServer *server, struct mg_connection *conn)
{
    /* Handler may access the request info using mg_get_request_info */
    struct mg_request_info * req_info = mg_get_request_info(conn);

    std::string controllerAction(req_info->uri);
    controllerAction.erase(0,5);

    //dispatch controller/actions
    if( controllerAction=="config/show" )
    {
        ConfigController *pxController=new ConfigController(pxDateService,pxFloppyService);
        bool processed=pxController->showAction(conn,req_info);
        delete pxController;
        return processed;
    }
    if( controllerAction=="status/" )
    {
        StatusController *pxController=new StatusController(pxDateService,pxFloppyService);
        bool processed=pxController->indexAction(conn,req_info);
        delete pxController;
        return processed;
    }
    if( controllerAction=="debug/" || controllerAction=="debug" || controllerAction=="debug/index" )
    {
        DebugController *pxController=new DebugController(pxDateService,pxFloppyService);
        bool processed=pxController->indexAction(conn,req_info);
        delete pxController;
        return processed;
    }
    if( controllerAction=="debug/getlog" )
    {
        DebugController *pxController=new DebugController(pxDateService,pxFloppyService);
        bool processed=pxController->getlogAction(conn,req_info);
        delete pxController;
        return processed;
    }
    if( controllerAction=="debug/getconfig" )
    {
        ConfigStream cs;
        cs.createConfigDump();

        DebugController *pxController=new DebugController(pxDateService,pxFloppyService);
        bool processed=pxController->getConfigAction(conn,req_info);
        delete pxController;
        return processed;
    }

    if( controllerAction=="debug/get_ceconf_prg" )
    {
        DebugController *pxController=new DebugController(pxDateService,pxFloppyService);
        bool processed=pxController->action_get_ceconf_prg(conn,req_info);
        delete pxController;
        return processed;
    }
    
    if( controllerAction=="debug/get_ceconf_msa" )
    {
        DebugController *pxController=new DebugController(pxDateService,pxFloppyService);
        bool processed=pxController->action_get_ceconf_msa(conn,req_info);
        delete pxController;
        return processed;
    }
    
    if( controllerAction=="debug/get_ceconf_tar" )
    {
        DebugController *pxController=new DebugController(pxDateService,pxFloppyService);
        bool processed=pxController->action_get_ceconf_tar(conn,req_info);
        delete pxController;
        return processed;
    }
    
    if( controllerAction=="debug/get_cedd" )
    {
        DebugController *pxController=new DebugController(pxDateService,pxFloppyService);
        bool processed=pxController->action_get_cedd(conn,req_info);
        delete pxController;
        return processed;
    }

    if( controllerAction=="screencast/screenshot_query_vbl" ) {
        // create JSON
        std::string sJson;
        sJson  = "{\"screenShotVblEnabled\":";
        sJson += events.screenShotVblEnabled ? "true" : "false";
        sJson += "}"; 

        // send HTTP header
        this->helperHttpHeader(conn,"200 OK",sJson.length());
        mg_write(conn, sJson.c_str(), sJson.length());                  // add the content

        return true;
    }
    
    if( controllerAction=="screencast/screenshot_vbl_enable" ) {
        Debug::out(LOG_DEBUG, "ScreenCast - enabling screenshot VBL in CE_DD");
        events.screenShotVblEnabled = true;
        this->helperHttpHeader(conn,"200 OK",0);
        return true;
    }
    
    if( controllerAction=="screencast/screenshot_vbl_disable" ) {
        Debug::out(LOG_DEBUG, "ScreenCast - disabling screenshot VBL in CE_DD");
        events.screenShotVblEnabled = false;
        this->helperHttpHeader(conn,"200 OK",0);
        return true;
    }

    if( controllerAction=="screencast/do_screenshot" ) {
        Debug::out(LOG_DEBUG, "ScreenCast - request for screenshot");
        events.doScreenShot = true;
        this->helperHttpHeader(conn,"200 OK",0);
        return true;
    }
    
    if( controllerAction=="screencast/getscreen" )
    {
        ScreencastController *pxController=new ScreencastController(pxScreencastService);
        bool processed=pxController->getscreenAction(conn,req_info);
        delete pxController;
        return processed;
    }
    if( controllerAction=="screencast/getpalette" )
    {
        ScreencastController *pxController=new ScreencastController(pxScreencastService);
        bool processed=pxController->getpaletteAction(conn,req_info);
        delete pxController;
        return processed;
    }
    return false;

    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    mg_printf(conn, "<html><body>");
    mg_printf(conn, "<h2>This is the Api handler</h2>");
    mg_printf(conn, "Resource: GET ");
    mg_write(conn, controllerAction.c_str(), controllerAction.length());
    mg_printf(conn, "<p>The request was:<br><pre>%s %s HTTP/%s</pre></p>",
              req_info->request_method, req_info->uri, req_info->http_version);
    mg_printf(conn, "</body></html>\n");
    return true;
}

bool ControllerRouter::handlePost(CivetServer *server, struct mg_connection *conn)
{
    //the same for now
    return handleGet(server, conn);
}

void ControllerRouter::helperHttpHeader(struct mg_connection *conn, const char* pcStatus, int iLength)
{
        // send HTTP header
        mg_printf(conn, "HTTP/1.1 %s\r\n",pcStatus);
        mg_printf(conn, "Cache: no-cache\r\n");
        mg_printf(conn, "Content-Type: application/json\r\n");
        mg_printf(conn, "Content-Length: %d\r\n\r\n", iLength);  // Always set Content-Length
}