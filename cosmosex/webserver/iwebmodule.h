#ifndef _IWEBMODULE_H_
#define _IWEBMODULE_H_

#include "CivetServer.h"

class IWebModule
{
    public:
        virtual ~IWebModule(){};
        virtual void install(CivetServer *pxServer)=0;
        virtual void uninstall(CivetServer *pxServer)=0;
};

#endif
