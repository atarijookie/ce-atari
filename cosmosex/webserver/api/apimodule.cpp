/**
 * Defines the resource based API ("RESTly", this ain't REST)
 * e.g. /api/v1/*, /api/v2/* etc.
 */

#include "apimodule.h"

#include "../../debug.h"

#include "router/apiv1router.h"

ApiModule::ApiModule(VirtualKeyboardService* pxVKbdService,VirtualMouseService* pxVMouseService,FloppyService* pxFloppyService)
: pxVKbdService(pxVKbdService),pxVMouseService(pxVMouseService),pxFloppyService(pxFloppyService)
{
}

void ApiModule::install(CivetServer *pxServer)
{
    Debug::out(LOG_DEBUG, "Webserver installing module ApiV1Module.");

    apiV1Router=new ApiV1Router(pxVKbdService,pxVMouseService,pxFloppyService);
    pxServer->addHandler("/api/v1/*", apiV1Router);
}

void ApiModule::uninstall(CivetServer *pxServer)
{
    delete apiV1Router;
}
