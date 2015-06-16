#ifndef SCREENCASTCONTROLLER_H
#define	SCREENCASTCONTROLLER_H

#include "iwebcontroller.h"

#include "../../CivetServer.h"
#include "service/screencastservice.h"

class ScreencastController : public IWebController {
public:
    ScreencastController(ScreencastService* pxScreencastService);
    virtual ~ScreencastController();
    bool getscreenAction(mg_connection *conn, mg_request_info *req_info);
    bool getpaletteAction(mg_connection *conn, mg_request_info *req_info);
private:
    ScreencastService* pxScreencastService;
    unsigned char *pxScreen;
    unsigned char *pxPalette;
};

#endif

