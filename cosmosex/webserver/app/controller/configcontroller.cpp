//just an example
#include "configcontroller.h"
#include "service/configservice.h"
#include "service/floppyservice.h"

ConfigController::ConfigController(ConfigService* pxDateService, FloppyService* pxFloppyService):pxDateService(pxDateService),pxFloppyService(pxFloppyService)
{
}

ConfigController::~ConfigController()
{
}

bool ConfigController::showAction(mg_connection *conn, mg_request_info *req_info)
{
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");
    mg_printf(conn, "<html><body>");
    mg_printf(conn, "<h2>This is the config/show action</h2>");
    mg_printf(conn, "<p>The request was:<br><pre>%s %s HTTP/%s</pre></p>",
              req_info->request_method, req_info->uri, req_info->http_version);
    mg_printf(conn, "</body></html>\n");
    return true;
}
