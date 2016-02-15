#ifndef _CONTROLLERROUTER_H_
#define _CONTROLLERROUTER_H_

#include "../../CivetServer.h"
#include "service/configservice.h"
#include "service/floppyservice.h"
#include "service/screencastservice.h"

class ControllerRouter: public CivetHandler
{
public:
	ControllerRouter(ConfigService* pxDateService, FloppyService* pxFloppyService, ScreencastService* pxScreencastService);
    bool handleGet(CivetServer *server, struct mg_connection *conn);
    bool handlePost(CivetServer *server, struct mg_connection *conn);
private:
    void helperHttpHeader(struct mg_connection *conn, const char* pcStatus, int iLength);
	ConfigService* pxDateService;
	FloppyService* pxFloppyService;
    ScreencastService* pxScreencastService;
};

#endif
