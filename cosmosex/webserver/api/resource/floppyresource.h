#ifndef FLOPPYRESOURCE_H
#define	FLOPPYRESOURCE_H

#include <string>


#include "../../CivetServer.h"

#include "iapiresource.h"
#include "service/floppyservice.h"

class FloppyResource : public IApiResource {
public:
    FloppyResource(FloppyService *pxFloppyService);
    virtual ~FloppyResource();
    bool dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo="" );
private:
    FloppyService *pxFloppyService;
};

#endif

