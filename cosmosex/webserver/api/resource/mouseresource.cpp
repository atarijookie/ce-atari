#include "mouseresource.h"

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

MouseResource::MouseResource(VirtualMouseService *pxMouseServiceDi) : pxMouseService(pxMouseServiceDi)
{
}

MouseResource::~MouseResource() 
{
}

bool MouseResource::dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo /*=""*/) 
{
    if( strstr(req_info->request_method,"GET")==0 && strstr(req_info->request_method,"POST")==0 ){
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed %s\r\n\r\n",req_info->request_method);
        return true;
    }

    int len=0;
    char jsonMouseRel[1024];
    
    if( strstr(req_info->request_method,"GET")>0 ){
        const char *qs = req_info->query_string;
        len=mg_get_var(qs, strlen(qs == NULL ? "" : qs), "rel", jsonMouseRel, sizeof(jsonMouseRel));
    }
     
    if( strstr(req_info->request_method,"POST")>0 ){
        len = mg_read(conn, jsonMouseRel, sizeof(jsonMouseRel)-1);
        jsonMouseRel[len]=0; 
    }
     
    int iX=0,iY=0;
    if( len>0 ){
        cJSON *root = cJSON_Parse(jsonMouseRel);
        if( root!=NULL ){
            char* pcType=cJSON_GetObjectItem(root,"type")->valuestring;
            if( pcType==NULL ){
                Debug::out(LOG_ERROR, "unknown mouse packet type : %s",pcType);
                mg_printf(conn, "HTTP/1.1 400 Bad Request - unknown mouse packet type '%s'\r\n\r\n",pcType);
                return true;
            }
            if( strstr(pcType,"relative")!=NULL ){
                iX = cJSON_GetObjectItem(root,"x")->valueint;
                iY = cJSON_GetObjectItem(root,"y")->valueint;
                pxMouseService->sendMousePacket(iX,iY);
            }
            if( strstr(pcType,"buttonleft")!=NULL || strstr(pcType,"buttonright")!=NULL ){
                char* pcState=cJSON_GetObjectItem(root,"state")->valuestring;
                if( pcState==NULL ){
                    Debug::out(LOG_ERROR, "mouse button state missing: %s",jsonMouseRel);
                    mg_printf(conn, "HTTP/1.1 400 Bad Request - mouse button state missing\r\n\r\n");
                    return true;
                }
                int iState=-1;
                if( strstr(pcState,"down")!=NULL ){
                    iState=1;
                }
                if( strstr(pcState,"up")!=NULL ){
                    iState=0;
                }
                if( iState==-1 ){
                    Debug::out(LOG_ERROR, "unknown mouse button state : %s",pcState);
                    mg_printf(conn, "HTTP/1.1 400 Bad Request - unknown mouse button state '%s'\r\n\r\n",pcState);
                    return true;
                }
                if( strstr(pcType,"buttonleft")!=NULL ){
                    pxMouseService->sendMouseButton(0,iState);
                } else{
                    pxMouseService->sendMouseButton(1,iState);
                }
            }
        } else{
            Debug::out(LOG_ERROR, "json malformed: %s",jsonMouseRel);
            mg_printf(conn, "HTTP/1.1 400 Bad Request - json malformed\r\n\r\n");
            return true;
        }
    }
    
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n");
    mg_printf(conn, "Cache: no-cache\r\n");
    mg_printf(conn, "Content-Length: %d\r\n\r\n",0);        // Always set Content-Length 
         
 /*
    mg_printf(conn, "<html><body>");
    mg_printf(conn, "%d %d",iX,iY);
    mg_printf(conn, "<h2>This is the mouse resource</h2>");
    mg_printf(conn, "<p>The request was:<br><pre>%s %s HTTP/%s</pre></p>",
              req_info->request_method, req_info->uri, req_info->http_version);
    mg_printf(conn, "</body></html>\n");
 */
    return true;    
}
