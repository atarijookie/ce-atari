#ifndef CONFIGRESOURCE_H
#define	CONFIGRESOURCE_H

#include <string>

#include "../../CivetServer.h"

#include "iapiresource.h"

class ConfigResource : public IApiResource {
public:
    ConfigResource();
    virtual ~ConfigResource();
    bool dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo="" );
private:
    int sendTerminalCommand(unsigned char cmd, unsigned char param);
    unsigned char *in_bfr,*tmp_bfr;
};

#endif

