// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#ifndef _VIRTUALKEYBOARDSERVICE_H_
#define _VIRTUALKEYBOARDSERVICE_H_

#include "virtualinputservice.h"

class VirtualKeyboardService : public VirtualInputService
{
public:
    VirtualKeyboardService();
    void sendPacket(int iKeyCode, int iValue);
};
#endif
