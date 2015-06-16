#include "kbdresource.h"

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

KbdResource::KbdResource(VirtualKeyboardService *pxKbdServiceDi) : pxKbdService(pxKbdServiceDi)  
{
}

KbdResource::~KbdResource() 
{
}

bool KbdResource::dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo /*=""*/) 
{
    if( strstr(req_info->request_method,"GET")==0 && strstr(req_info->request_method,"POST")==0 ){
        mg_printf(conn, "HTTP/1.1 405 Method Not Allowed %s\r\n",req_info->request_method);
        return true;
    }

    int len=0;
    char jsonKey[1024];
    
    if( strstr(req_info->request_method,"GET")>0 ){
        const char *qs = req_info->query_string;
        //this guarentees jsonkey to be 0-terminated
        len=mg_get_var(qs, strlen(qs == NULL ? "" : qs), "key", jsonKey, sizeof(jsonKey));
    }
     
    if( strstr(req_info->request_method,"POST")>0 ){
        //raw read, not automaticly 0-terminated
        len = mg_read(conn, jsonKey, sizeof(jsonKey)-1);
        jsonKey[len]=0; 
    }
     
    if( len>0 ){
        Debug::out(LOG_DEBUG, "json:%s",jsonKey);
        cJSON *root = cJSON_Parse(jsonKey);
        if( root!=NULL ){
            cJSON *pxCJSONObject=cJSON_GetObjectItem(root,"code");
            if( pxCJSONObject==NULL ){
                Debug::out(LOG_ERROR, "key code missing");                      
                mg_printf(conn, "HTTP/1.1 400 Bad Request - key code missing\r\n\r\n");
                return true;
            }
            int iCode = pxCJSONObject->valueint; 
            if( iCode==0 ){
                Debug::out(LOG_ERROR, "invalid key code: %d",iCode);
                mg_printf(conn, "HTTP/1.1 400 Bad Request - invalid key code\r\n\r\n");
                return true;
            }
            pxCJSONObject=cJSON_GetObjectItem(root,"state");
            if( pxCJSONObject==NULL ){
                Debug::out(LOG_ERROR, "key state missing");
                mg_printf(conn, "HTTP/1.1 400 Bad Request - key state missing\r\n\r\n");
                return true;
            }
            char* pcState = pxCJSONObject->valuestring;
            if( pcState==NULL ){
                Debug::out(LOG_ERROR, "key state missing: %s",jsonKey);
                mg_printf(conn, "HTTP/1.1 400 Bad Request - key state missing\r\n\r\n");
                return true;
            }
            int iValue=-1;
            if( strstr(pcState,"down")>0 ){
                iValue=1;
            }
            if( strstr(pcState,"up")>0 ){
                iValue=0;
            }
            if( iValue==-1 ){
                Debug::out(LOG_ERROR, "unknown key state : %s",pcState);
                mg_printf(conn, "HTTP/1.1 400 Bad Request - unknown key state '%s'\r\n\r\n",pcState);
                return true;
            }
            pxKbdService->sendPacket(iCode,iValue);
        } else{
            Debug::out(LOG_ERROR, "json malformed");
            mg_printf(conn, "HTTP/1.1 400 Bad Request - json malformed\r\n\r\n");
            return true;
        }
    }

    Debug::out(LOG_DEBUG, "KbdResource::dispatch");
    mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n");
    mg_printf(conn, "Cache: no-cache\r\n");
    mg_printf(conn, "Content-Length: %d\r\n\r\n",0);        // Always set Content-Length 
    return true;    
}

void KbdResource::sendPacket(int iKeyCode, int iState){
    int fd = open("./vdev/kbd", O_WRONLY | O_NONBLOCK);

    input_event xEvent;
    gettimeofday(&xEvent.time, NULL);
    xEvent.type=EV_KEY;
    xEvent.code=iKeyCode;
    // ev->value -- 1: down, 2: auto repeat, 0: up
    xEvent.value=iState;
    Debug::out(LOG_DEBUG, "write kbd");
    write(fd, &xEvent, sizeof(xEvent));
    
    close(fd);
}