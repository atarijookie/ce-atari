#ifndef MOUSERESOURCE_H
#define	MOUSERESOURCE_H

#include "webserver/CivetServer.h"

#include "iapiresource.h"
#include "service/virtualmouseservice.h"

class MouseResource : public IApiResource {
public:
    MouseResource(VirtualMouseService *pxMouseServiceDi);
    virtual ~MouseResource();
    bool dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo="");
private:
    void openFifo(); 
    void closeFifo(); 
    void sendMousePacket(int iX,int iY);
    void sendMouseButton(int iButton,int iState);

    int fifoHandle;
    VirtualMouseService *pxMouseService;
};

#endif

