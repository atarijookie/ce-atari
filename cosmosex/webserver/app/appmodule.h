#ifndef _APPMODULE_H_
#define _APPMODULE_H_

#include "../CivetServer.h"
#include "../iwebmodule.h"
#include "service/configservice.h"
#include "service/floppyservice.h"
#include "service/screencastservice.h"

class AppModule : public IWebModule
{
public:
	AppModule(ConfigService* pxDateService, FloppyService* pxFloppyService, ScreencastService* pxScreencastService);
    virtual void install(CivetServer *pxServer);
    virtual void uninstall(CivetServer *pxServer);
private:
	ConfigService* pxDateService;
	FloppyService* pxFloppyService;
	ScreencastService* pxScreencastService;
};


#endif
