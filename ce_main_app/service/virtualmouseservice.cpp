#include "virtualmouseservice.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <unistd.h>
#include <pthread.h>

#include "debug.h"

pthread_mutex_t virtualMouseServiceMutex = PTHREAD_MUTEX_INITIALIZER;

VirtualMouseService::VirtualMouseService():initialized(false){

}

void VirtualMouseService::start() 
{
    //this service is allowed to initialize only once
    if( initialized ){
        return;
    }
    openFifo();
}

void VirtualMouseService::stop() 
{
    //only deinitialize when we are initialized
    if( !initialized ){
        return;
    }
    closeFifo();
}

void VirtualMouseService::openFifo() 
{
    int res;

    res = mkdir("/tmp/vdev/",0666);
    if (res == -1 && errno != EEXIST) {     // if failed to create dir, and it's not because it already exists
        Debug::out(LOG_ERROR, "Virtual mouse could not create /tmp/vdev/ - errno: %d", errno);
    }
    Debug::out(LOG_DEBUG, "Virtual mouse creating /tmp/vdev/mouse.");
    //try to remove if it's a file
    unlink("/tmp/vdev/mouse");
    int err = mkfifo("/tmp/vdev/mouse",0666);
    if(err < 0) {
        Debug::out(LOG_ERROR, "Virtual mouse could not create /tmp/vdev/mouse - errno: %d", errno);
        if(errno == EEXIST) {
            Debug::out(LOG_ERROR, "    /tmp/vdev/mouse already exists.");
        }
	}    
    initialized=true;
}

void VirtualMouseService::closeFifo() 
{
    Debug::out(LOG_DEBUG, "Virtual keyboard closing /tmp/vdev/mouse.");
    unlink("/tmp/vdev/mouse");
}

void VirtualMouseService::sendMouseButton(int iButton,int iState){

    pthread_mutex_lock(&virtualMouseServiceMutex);    //we could be accessed from any thread, so better lcok this

    int fd = open("/tmp/vdev/mouse", O_WRONLY | O_NONBLOCK);
    input_event xEvent;
    gettimeofday(&xEvent.time, NULL);
    xEvent.type=EV_KEY;
    if( iButton==0 ){
      xEvent.code=BTN_LEFT;
    } else if( iButton==1 ){
      xEvent.code=BTN_RIGHT;
    }
    xEvent.value=iState;
    Debug::out(LOG_DEBUG, "write mouse button");
    write(fd, &xEvent, sizeof(xEvent));
    close(fd);

    pthread_mutex_unlock(&virtualMouseServiceMutex);

}

void VirtualMouseService::sendMousePacket(int iX,int iY){

    pthread_mutex_lock(&virtualMouseServiceMutex);    //we could be accessed from any thread, so better lcok this

    int fd = open("/tmp/vdev/mouse", O_WRONLY | O_NONBLOCK);
    input_event xEvent;
    gettimeofday(&xEvent.time, NULL);
    xEvent.type=EV_REL;
    xEvent.code=REL_X;
    xEvent.value=iX;
    Debug::out(LOG_DEBUG, "write mouse X");
    write(fd, &xEvent, sizeof(xEvent));
    Debug::out(LOG_DEBUG, "write mouse X done");
    
    xEvent.code=REL_Y;
    xEvent.value=iY;
    Debug::out(LOG_DEBUG, "write mouse Y");
    write(fd, &xEvent, sizeof(xEvent));
    Debug::out(LOG_DEBUG, "write mouse Y done");
    
    close(fd);

    pthread_mutex_unlock(&virtualMouseServiceMutex);

}