#ifndef _WEBSERVER_H_
#define _WEBSERVER_H_

#include <list> 

#include "CivetServer.h"
#include "iwebmodule.h"

class WebServer
{
private:
    CivetServer *pxServer;
    std::list<IWebModule*> lHandlers;  
    static void onUpload(struct mg_connection *conn, const char *path);
public:
    WebServer();
    void addModule(IWebModule *pxModule);
    void start();
    void stop();
    static std::string sLastUploadedFile;
    bool bInited;
};

#endif
