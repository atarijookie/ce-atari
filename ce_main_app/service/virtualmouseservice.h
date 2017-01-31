// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _VIRTUALMOUSESERVICE_H_
#define _VIRTUALMOUSESERVICE_H_

#include "virtualinputservice.h"

class VirtualMouseService : public VirtualInputService
{
public:
    VirtualMouseService();
    void sendMousePacket(int iX,int iY);
    void sendMouseButton(int iButton,int iState);
};
#endif
