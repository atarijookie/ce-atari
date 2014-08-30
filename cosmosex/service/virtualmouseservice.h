#ifndef _VIRTUALMOUSESERVICE_H_
#define _VIRTUALMOUSESERVICE_H_

class VirtualMouseService
{
public:
  VirtualMouseService();
	void start();
	void stop();
    void sendMousePacket(int iX,int iY);
    void sendMouseButton(int iButton,int iState);
private:
    void openFifo();
    void closeFifo();
    
    int fifoHandle;
    bool initialized;
};
#endif