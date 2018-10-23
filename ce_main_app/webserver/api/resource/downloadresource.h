#ifndef DOWNLOADRESOURCE_H
#define	DOWNLOADRESOURCE_H

#include <string>

#include "../../CivetServer.h"

#include "iapiresource.h"

class ImageList;
class FloppyService;

class DownloadResource : public IApiResource {
public:
    DownloadResource(FloppyService *pxFloppyService);
    virtual ~DownloadResource();
    bool dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo="" );

private:
    FloppyService *pxFloppyService;

    void sendResponse(mg_connection *conn, std::ostringstream &stringStream);

    void onGetImageList(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo);
    void onGetDownloadingList(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo);

    void onDownloadItem(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo);
    void onInsertItem(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo);
};

#endif
