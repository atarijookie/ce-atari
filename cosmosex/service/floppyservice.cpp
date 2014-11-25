#include "floppyservice.h"
#include "debug.h"
#include "utils.h"
#include "ccorethread.h" 
 
FloppyService::FloppyService():pxImageSilo(NULL),pxCoreThread(NULL),iInitState(INIT_NONE) 
{
}

void FloppyService::start() 
{
}

void FloppyService::stop() 
{
}

void FloppyService::setImageSilo(ImageSilo* pxImageSilo)
{
    this->pxImageSilo=pxImageSilo;
    if( this->pxCoreThread!=NULL )
    {
        iInitState=INIT_OK;
    }
}

void FloppyService::setCoreThread(CCoreThread* pxCoreThread)
{
    this->pxCoreThread=pxCoreThread;
    if( this->pxImageSilo!=NULL )
    {
        iInitState=INIT_OK;
    }
}

bool FloppyService::isInitialized()
{
    return getInitState()==INIT_OK;
}

int FloppyService::getInitState()
{
    return iInitState;
}

std::string FloppyService::getImageName(int iSlot)
{
    if( !isInitialized() )
    {
        return std::string("");
    }
    SiloSlot *pxSlot=pxImageSilo->getSiloSlot(iSlot);
    if( pxSlot==NULL )
    {
        return std::string("");
    }
    return std::string(pxSlot->imageFile);
}

bool FloppyService::setImage(int iSlot, std::string sLocalFileWPath)
{
    if( !isInitialized() )
    {
        return false;
    }
    if( iSlot < 0 || iSlot > 2 ) 
    {                        
        return false;        // index out of range? fail
    } 
    
    std::string sPath;
    std::string sFile;
    Utils::splitFilenameFromPath(sLocalFileWPath, sPath, sFile);
    
    std::string sEmpty;
    pxImageSilo->add(iSlot, sFile, sLocalFileWPath, sEmpty, sEmpty, true); 
        
    return true;
}

bool FloppyService::setActiveSlot(int iSlot)
{
    if( !isInitialized() )
    {
        return false;
    }
    pxImageSilo->setCurrentSlot(iSlot);                 // set the slot for valid index, set the empty image for invalid slot
    if( iSlot==3 ){
        pxCoreThread->setFloppyImageLed(0xff);
    }else{
        pxCoreThread->setFloppyImageLed(iSlot);
    }
    return true;
}

int FloppyService::getActiveSlot()
{
    return pxImageSilo->getCurrentSlot(); 
}
