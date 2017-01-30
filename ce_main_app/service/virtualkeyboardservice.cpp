// vim: shiftwidth=4 softtabstop=4 tabstop=4 expandtab
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <linux/input.h>
#include <unistd.h>
#include <pthread.h>

#include "virtualkeyboardservice.h"

#include "debug.h"

pthread_mutex_t virtualKeyboardServiceMutex = PTHREAD_MUTEX_INITIALIZER;

VirtualKeyboardService::VirtualKeyboardService()
 : VirtualInputService("/tmp/vdev/kbd", "keyboard")
{
}

void VirtualKeyboardService::sendPacket(int iKeyCode, int iState)
{
    ssize_t res;
    pthread_mutex_lock(&virtualKeyboardServiceMutex);    //we could be accessed from any thread, so better lcok this

    input_event xEvent;
    gettimeofday(&xEvent.time, NULL);
    xEvent.type = EV_KEY;
    xEvent.code = iKeyCode;
    // ev->value -- 1: down, 2: auto repeat, 0: up
    xEvent.value = iState;
    res = write(fd, &xEvent, sizeof(xEvent));
    Debug::out(LOG_DEBUG, "write kbd res=%d", (int)res);
    if(res < 0) {
        Debug::out(LOG_ERROR, "VirtualKeyboardService::sendPacket(0x%x, %d) write() : %s", iKeyCode, iState, strerror(errno));
    }

    pthread_mutex_unlock(&virtualKeyboardServiceMutex);
}
