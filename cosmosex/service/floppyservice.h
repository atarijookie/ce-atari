#ifndef _FLOPPYSERVICE_H_
#define _FLOPPYSERVICE_H_

#include <string>
#include "floppy/imagesilo.h"

class CCoreThread;

class FloppyService
{
public:
    FloppyService();
    void start();
    void stop();
    void setImageSilo(ImageSilo* pxImageSilo);
    void setCoreThread(CCoreThread* pxCoreThread);
    bool isInitialized();
    int getInitState();
    std::string getImageName(int iSlot);
    bool setImage(int iSlot, std::string sLocalFileWPath);
    bool setActiveSlot(int iSlot);
private:
    ImageSilo* pxImageSilo;
    CCoreThread* pxCoreThread;
    enum eInitState {INIT_NONE=0, INIT_OK=1};
    int iInitState;
};
#endif