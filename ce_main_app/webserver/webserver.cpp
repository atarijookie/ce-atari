#include <string.h>
#include <list>

#include "webserver.h"

#include "../debug.h"

#define DOCUMENT_ROOT "/ce/app/webroot/"
#define PORT "80"

std::string WebServer::sLastUploadedFile;

WebServer::WebServer():bInited(false)
{
}

void WebServer::addModule(IWebModule *pxModule)
{
    Debug::out(LOG_DEBUG, "Webserver adding module...");
    lHandlers.push_back(pxModule);
}

void WebServer::start(int portOfset)
{
    Debug::out(LOG_DEBUG, "Webserver starting...");

    const char * options[] = { "document_root", DOCUMENT_ROOT,
                               "listening_ports", PORT, 0
                             };
    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.upload = WebServer::onUpload; 

    pxServer=new CivetServer(options,&callbacks);
    if( pxServer->getContext()==NULL ){
        Debug::out(LOG_ERROR, "Webserver could not start.");
        return;
    }
    Debug::out(LOG_DEBUG, "Webserver installing modules...");
    for(std::list<IWebModule*>::iterator xIter = lHandlers.begin();xIter != lHandlers.end(); xIter++)
    {
        (*xIter)->install(pxServer);
    }
    Debug::out(LOG_DEBUG, "Webserver installing modules done.");
    
    Debug::out(LOG_DEBUG, "Webserver running...");
    bInited=true;
}
void WebServer::stop()
{
    if( !bInited )
    {
        return;
    }
    
    for(std::list<IWebModule*>::iterator xIter = lHandlers.begin();xIter != lHandlers.end(); xIter++)
    {
        (*xIter)->uninstall(pxServer);
        delete (*xIter); 
    }
    delete pxServer;
    pxServer=NULL;
    Debug::out(LOG_DEBUG, "Webserver stopped.");
}  

void WebServer::onUpload(struct mg_connection *conn, const char *path)
{
    WebServer::sLastUploadedFile=std::string(path);
}   
    
    
