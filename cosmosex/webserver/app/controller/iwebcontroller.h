#ifndef IWEBCONTROLLER_H
#define	IWEBCONTROLLER_H

#include <string>
#include "../../CivetServer.h"

class IWebController {
public:
    public:
        virtual ~IWebController(){};
    protected:
		std::string replaceAll(std::string str, const std::string& from, const std::string& to);
};

#endif

