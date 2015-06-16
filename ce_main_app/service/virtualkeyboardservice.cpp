#include "virtualkeyboardservice.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/input.h>
#include <unistd.h>
#include <pthread.h>

#include "debug.h"

pthread_mutex_t virtualKeyboardServiceMutex = PTHREAD_MUTEX_INITIALIZER;

VirtualKeyboardService::VirtualKeyboardService():initialized(false){

}

void VirtualKeyboardService::start() 
{
    if( initialized ){
        return;
    } 
    openFifo();
}

void VirtualKeyboardService::stop() 
{
    if( !initialized ){
        return;
    } 
    closeFifo();
}

void VirtualKeyboardService::openFifo() 
{
    if (mkdir("/tmp/vdev/",0666) == -1) {
        Debug::out(LOG_ERROR, "Virtual keyboard could not create /tmp/vdev/.");
    }
    Debug::out(LOG_DEBUG, "Virtual keyboard creating /tmp/vdev/kbd.");
    //try to remove if it's a file
    unlink("/tmp/vdev/kbd");
    int err=mkfifo("/tmp/vdev/kbd",0666);
    if(err < 0) 
    {
        Debug::out(LOG_ERROR, "Virtual keyboard could not create /tmp/vdev/kbd.");
		if(errno == EEXIST) {
            Debug::out(LOG_ERROR, "    /tmp/vdev/kbd already exists.");
		}
		    //printf("errno is set as %d\n", errno);
	}    
}

void VirtualKeyboardService::closeFifo() 
{
    Debug::out(LOG_DEBUG, "Virtual keyboard closing /tmp/vdev/kbd.");
    unlink("/tmp/vdev/kbd");
}

void VirtualKeyboardService::sendPacket(int iKeyCode, int iState){

    pthread_mutex_lock(&virtualKeyboardServiceMutex);    //we could be accessed from any thread, so better lcok this

    int fd = open("/tmp/vdev/kbd", O_WRONLY | O_NONBLOCK);

    input_event xEvent;
    gettimeofday(&xEvent.time, NULL);
    xEvent.type=EV_KEY;
    xEvent.code=iKeyCode;
    // ev->value -- 1: down, 2: auto repeat, 0: up
    xEvent.value=iState;
    Debug::out(LOG_DEBUG, "write kbd");
    write(fd, &xEvent, sizeof(xEvent));
    
    close(fd);

    pthread_mutex_unlock(&virtualKeyboardServiceMutex);

}