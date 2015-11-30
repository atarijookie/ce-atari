#ifndef DEBUGCONTROLLER_H
#define	DEBUGCONTROLLER_H

#include "iwebcontroller.h"

#include "../../CivetServer.h"
#include "service/configservice.h"
#include "service/floppyservice.h"

class DebugController : public IWebController {
public:
    DebugController(ConfigService* pxDateService, FloppyService* pxFloppyService);
    virtual ~DebugController();
    bool indexAction(mg_connection *conn, mg_request_info *req_info);

    bool getlogAction(mg_connection *conn, mg_request_info *req_info);
    bool action_get_ceconf_prg(mg_connection *conn, mg_request_info *req_info);
    bool action_get_ceconf_msa(mg_connection *conn, mg_request_info *req_info);
    bool action_get_ceconf_tar(mg_connection *conn, mg_request_info *req_info);
    bool action_get_cedd(mg_connection *conn, mg_request_info *req_info);
    
    bool getFile(mg_connection *conn, std::string &sDownloadedFileName, std::string &sContentType, std::string &sCeFilePath, bool sendFileSize);
private:
	std::string urlencode(const std::string &c);
	std::string char2hex( char dec );

	ConfigService* pxDateService;
	FloppyService* pxFloppyService;
};

#endif

