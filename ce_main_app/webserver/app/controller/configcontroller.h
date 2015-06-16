#ifndef CONFIGCONTROLLER_H
#define	CONFIGCONTROLLER_H

#include "iwebcontroller.h"

#include "../../CivetServer.h"
#include "service/configservice.h"
#include "service/floppyservice.h"

class ConfigController : public IWebController {
public:
    ConfigController(ConfigService* pxDateService, FloppyService* pxFloppyService);
    virtual ~ConfigController();
    bool showAction(mg_connection *conn, mg_request_info *req_info);
private:
	ConfigService* pxDateService;
	FloppyService* pxFloppyService;
};

#endif

