#include <stdio.h>
#include <errno.h>  
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <string.h>
#include <iostream>
#include <ctime>

#include "screencastservice.h"
#include "settings.h"
#include "debug.h"

pthread_mutex_t screencastServicePaletteMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t screencastServiceScreenMutex = PTHREAD_MUTEX_INITIALIZER;

ScreencastService::ScreencastService() 
{
}

//get NTP time and set system time accordingly
void ScreencastService::start() 
{
    pxPalette=new unsigned char[16*2];
    memset(pxPalette,0,32);
    pxPalette[0]=-1;
    pxPalette[1]=-1;
    
    pxScreen=new unsigned char[32000];
    memset(pxScreen,0,32000);
    Debug::out(LOG_DEBUG, "ScreencastService: init done.");
}

void ScreencastService::stop() 
{
    delete[] pxScreen;
    delete[] pxPalette;
    pxScreen=NULL;
    pxPalette=NULL;
}

int ScreencastService::getFrameSkip()
{
    Settings s;
    int iFrameSkip = s.getInt ((char *) "SCREENCAST_FRAMESKIP",        20);

    return iFrameSkip;
}

void ScreencastService::setSTResolution(unsigned char iSTResolution)
{
	this->iSTResolution=iSTResolution;
}

unsigned char ScreencastService::getSTResolution()
{
	return iSTResolution;
}

void ScreencastService::setPalette(void *pxPalette)
{
    pthread_mutex_lock(&screencastServicePaletteMutex);
    //copy given palette
    memcpy(this->pxPalette,pxPalette,16*2);
    
    //special case monochrome palette
    //ignore inverting for now (have to retest behaviour on real hardware)
    if( getSTResolution()==2 ){
        this->pxPalette[0]=this->pxPalette[1]=0xff;        
        this->pxPalette[2]=this->pxPalette[3]=0x00;        
    }
    
    pthread_mutex_unlock(&screencastServicePaletteMutex);
}

void ScreencastService::getPalette(void *pxPalette)
{
    pthread_mutex_lock(&screencastServiceScreenMutex);
    //copy current palette out
    memcpy(pxPalette,this->pxPalette,16*2);
    pthread_mutex_unlock(&screencastServiceScreenMutex);
}

void ScreencastService::setScreen(void *pxScreen)
{
    pthread_mutex_lock(&screencastServiceScreenMutex);
    //copy given screen
    memcpy(this->pxScreen,pxScreen,32000);
    pthread_mutex_unlock(&screencastServiceScreenMutex);
}

void ScreencastService::getScreen(void *pxScreen)
{
    pthread_mutex_lock(&screencastServiceScreenMutex);
    //copy current screen out
    memcpy(pxScreen,this->pxScreen,32000);
    pthread_mutex_unlock(&screencastServiceScreenMutex);
}
