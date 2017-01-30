// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <linux/input.h>
#include <pthread.h>

#include "virtualmouseservice.h"
#include "debug.h"

pthread_mutex_t virtualMouseServiceMutex = PTHREAD_MUTEX_INITIALIZER;

VirtualMouseService::VirtualMouseService()
 : VirtualInputService("/tmp/vdev/mouse", "mouse")
{
}

void VirtualMouseService::sendMouseButton(int iButton,int iState)
{
	ssize_t res;
    pthread_mutex_lock(&virtualMouseServiceMutex);    //we could be accessed from any thread, so better lcok this

    input_event xEvent;
    gettimeofday(&xEvent.time, NULL);
    xEvent.type = EV_KEY;
    if( iButton == 0 ){
      xEvent.code = BTN_LEFT;
    } else if( iButton==1 ){
      xEvent.code = BTN_RIGHT;
    } // TODO : other buttons ?
    xEvent.value = iState;
    Debug::out(LOG_DEBUG, "write mouse button");
    res = write(fd, &xEvent, sizeof(xEvent));
	if(res < 0 ) {
		Debug::out(LOG_ERROR, "VirtualMouseService::sendMouseButton() write(%s) : %s", devpath, strerror(errno));
	}
    pthread_mutex_unlock(&virtualMouseServiceMutex);
}

void VirtualMouseService::sendMousePacket(int iX,int iY)
{
	ssize_t res;
    pthread_mutex_lock(&virtualMouseServiceMutex);    //we could be accessed from any thread, so better lcok this

    input_event ev[2];
    gettimeofday(&ev[0].time, NULL);
	memcpy(&ev[1].time, &ev[0].time, sizeof(ev[0].time));
    ev[0].type = EV_REL;
    ev[0].code = REL_X;
    ev[0].value = iX;
    ev[1].type = EV_REL;
    ev[1].code = REL_Y;
    ev[1].value = iY;
    Debug::out(LOG_DEBUG, "write mouse X, Y");
    res = write(fd, ev, sizeof(ev));
    Debug::out(LOG_DEBUG, "write mouse X, Y done res=%d", (int)res);
	if(res < 0) {
		Debug::out(LOG_ERROR, "VirtualMouseService::sendMousePacket() write(%s) : %s", devpath, strerror(errno));
	}
    
    pthread_mutex_unlock(&virtualMouseServiceMutex);
}
