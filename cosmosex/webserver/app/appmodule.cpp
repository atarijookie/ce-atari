/**
 * This defines the controller-based App-Server
 * Route is /app/[controller]/[action]
 */

#include "appmodule.h"

#include "../../debug.h"

#include "router/controllerrouter.h"
#include "service/configservice.h"
#include "service/floppyservice.h"

AppModule::AppModule(ConfigService* pxDateService, FloppyService* pxFloppyService, ScreencastService *pxScreencastService):
    pxDateService(pxDateService),pxFloppyService(pxFloppyService),pxScreencastService(pxScreencastService)
{
}


void AppModule::install(CivetServer *pxServer)
{
    Debug::out(LOG_DEBUG, "Webserver installing module AppModule.");
    pxServer->addHandler("/app/*", new ControllerRouter(pxDateService,pxFloppyService,pxScreencastService));
}

void AppModule::uninstall(CivetServer *pxServer)
{
}
