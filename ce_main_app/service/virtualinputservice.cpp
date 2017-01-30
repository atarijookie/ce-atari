// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "virtualinputservice.h"

#include "debug.h"

VirtualInputService::VirtualInputService(const char * path, const char * type)
 : fd(-1), initialized(false), devpath(path), devtype(type)
{
}

VirtualInputService::~VirtualInputService()
{
    stop();
}

void VirtualInputService::start() 
{
    if( initialized ){
        return;
    } 
    openFifo();
}

void VirtualInputService::stop() 
{
    if( !initialized ){
        return;
    } 
    closeFifo();
    initialized = false;
}

void VirtualInputService::openFifo() 
{
    int res;

    res = mkdir("/tmp/vdev/",0666);
    if (res == -1 && errno != EEXIST) {     // if failed to create dir, and it's not because it already exists
        Debug::out(LOG_ERROR, "Virtual %s could not create /tmp/vdev/ - errno: %d", devtype, errno);
    }
    
    Debug::out(LOG_DEBUG, "Virtual %s creating %s", devtype, devpath);
    
    //try to remove if it's a file
    unlink(devpath);
    int err = mkfifo(devpath, 0666);
    if(err < 0) {
        Debug::out(LOG_ERROR, "Virtual %s could not create %s - errno: %d", devtype, devpath, errno);
		if(errno == EEXIST) {
            Debug::out(LOG_ERROR, "    %s already exists.", devpath);
		}
	}    
    //fd = open(devpath, O_WRONLY | O_NONBLOCK);
    fd = open(devpath, O_RDWR | O_NONBLOCK);    // we open Read/Write in order to initialize the fifo
    if(fd < 0) {
        Debug::out(LOG_ERROR, "VirtualInputService::openFifo(): open(%s) : %s", devpath, strerror(errno));
    }
}

void VirtualInputService::closeFifo() 
{
    Debug::out(LOG_DEBUG, "Virtual %s closing %s", devtype, devpath);
    close(fd);
    fd = -1;
    unlink(devpath);
}
