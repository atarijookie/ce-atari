#ifndef _APIV1ROUTER_H_
#define _APIV1ROUTER_H_

#include "../../CivetServer.h"

#include "service/virtualmouseservice.h"
#include "service/virtualkeyboardservice.h"
#include "service/floppyservice.h"

#include "../resource/mouseresource.h"
#include "../resource/kbdresource.h"
#include "../resource/floppyresource.h"
#include "../resource/configresource.h"

class ApiV1Router: public CivetHandler
{
public:
    ApiV1Router(VirtualKeyboardService* pxVKbdService,VirtualMouseService* pxVMouseService,FloppyService* pxFloppyService);
    ~ApiV1Router();
    bool handleGet(CivetServer *server, struct mg_connection *conn);
    bool handlePost(CivetServer *server, struct mg_connection *conn);
    bool handlePut(CivetServer *server, struct mg_connection *conn);
private:
    MouseResource *pxMouse; 
    KbdResource *pxKeyboard;
    FloppyResource *pxFloppy; 
    ConfigResource *pxConfig; 
};

#endif