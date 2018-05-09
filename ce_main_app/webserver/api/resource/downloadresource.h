#ifndef DOWNLOADRESOURCE_H
#define	DOWNLOADRESOURCE_H

#include <string>

#include "../../CivetServer.h"

#include "iapiresource.h"

class DownloadResource : public IApiResource {
public:
    DownloadResource(void);
    virtual ~DownloadResource();
    bool dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo="" );

private:

};

#endif
