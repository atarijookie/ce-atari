#ifndef _VIRTUALKEYBOARDSERVICE_H_
#define _VIRTUALKEYBOARDSERVICE_H_

class VirtualKeyboardService
{
public:
  VirtualKeyboardService();
	void start();
	void stop();
    void sendPacket(int iKeyCode, int iValue);
private:
    void openFifo();
    void closeFifo();
    
    int fifoHandle;
    bool initialized;
};
#endif