#ifndef KBDRESOURCE_H
#define	KBDRESOURCE_H

#include "../../CivetServer.h"

#include "iapiresource.h"
#include "service/virtualkeyboardservice.h"

class KbdResource : public IApiResource {
public:
    KbdResource(VirtualKeyboardService *pxKbdServiceDi);
    virtual ~KbdResource();
    bool dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo="");
private:
    void openFifo(); 
    void closeFifo(); 
    void sendPacket(int iKeyCode, int iValue);
    
    int fifoHandle;
    VirtualKeyboardService *pxKbdService;
};

#endif

