#ifndef DOWNLOADRESOURCE_H
#define	DOWNLOADRESOURCE_H

#include <string>

#include "../../CivetServer.h"

#include "iapiresource.h"

class ImageList;

class DownloadResource : public IApiResource {
public:
    DownloadResource(void);
    virtual ~DownloadResource();
    bool dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo="" );

    void onGetImageList(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo);

private:
    ImageList *imageList;
};

#endif
