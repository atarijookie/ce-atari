/* 
 * File:   MouseResource.h
 * Author: me
 *
 * Created on 18. Juli 2014, 09:19
 */

#ifndef IAPIRESOURCE_H
#define	IAPIRESOURCE_H

#include <string>
#include "../../CivetServer.h"

class IApiResource {
public:
   public:
        virtual ~IApiResource(){};
        virtual bool dispatch(mg_connection *conn, mg_request_info *req_info, std::string sResourceInfo="" )=0;
private:
        virtual void get(mg_request_info *req_info){};
        virtual void post(mg_request_info *req_info){};
        virtual void put(mg_request_info *req_info){};
        virtual void del(mg_request_info *req_info){};

};

#endif	/* MOUSERESOURCE_H */

