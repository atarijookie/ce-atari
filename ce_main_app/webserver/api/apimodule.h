#ifndef _APIMODULE_H_
#define _APIMODULE_H_

#include "../CivetServer.h"
#include "../iwebmodule.h"

#include "service/virtualkeyboardservice.h"
#include "service/virtualmouseservice.h"
#include "service/floppyservice.h"

#include "router/apiv1router.h"

class ApiModule : public IWebModule
{
public:
    ApiModule(VirtualKeyboardService* pxVKbdService,VirtualMouseService* pxVMouseService, FloppyService* pxFloppyService);
    virtual void install(CivetServer *pxServer);
    virtual void uninstall(CivetServer *pxServer);
private:
    ApiV1Router* apiV1Router;
    VirtualKeyboardService* pxVKbdService;
    VirtualMouseService* pxVMouseService;
    FloppyService* pxFloppyService;    
};


#endif