#ifndef _FLOPPYSERVICE_H_
#define _FLOPPYSERVICE_H_

#include <string>

class CCoreThread;
class ImageSilo;

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
    int getImageState();
	bool isImageReady();
    bool setActiveSlot(int iSlot);
    int getActiveSlot();
private:
    ImageSilo* pxImageSilo;
    CCoreThread* pxCoreThread;
    enum eInitState {INIT_NONE=0, INIT_OK=1};
    enum eImageState {IMAGE_NOTREADY=0, IMAGE_OK=1};
    int iInitState;
};
#endif
