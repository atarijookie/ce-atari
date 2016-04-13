#ifndef STATUSCONTROLLER_H
#define	STATUSCONTROLLER_H

#include "iwebcontroller.h"

#include "../../CivetServer.h"
#include "service/configservice.h"
#include "service/floppyservice.h"

class StatusController : public IWebController {
public:
    StatusController(ConfigService* pxDateService, FloppyService* pxFloppyService);
    virtual ~StatusController();
    bool indexAction(mg_connection *conn, mg_request_info *req_info);

    bool statusAction(mg_connection *conn, mg_request_info *req_info);
    
private:
	ConfigService* pxDateService;
	FloppyService* pxFloppyService;
};

#endif

