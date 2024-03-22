#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include "extensiondefs.h"
#include "main.h"
#include "recv.h"
#include "fifo.h"

bool threadCreated = false;
pthread_t recvThreadInfo;
volatile sig_atomic_t sigintReceived;

Fifo* fifoAudio;
Fifo* fifoVideo;

void *recvThreadCode(void *ptr)
{
    printf("recvThreadCode starting");
    int sock = createRecvSocket(SOCK_PATH_RECV_FFMPEG_AUDIO);

    if(sock < 0) {              // without socket this thread has no use
        return 0;
    }

    char bfr[1024];

    fifoAudio = new Fifo();
    fifoVideo = new Fifo();

    while(sigintReceived == 0) {
        struct timeval timeout;
        timeout.tv_sec = 1;                             // short timeout
        timeout.tv_usec = 0;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        int res = select(sock + 1, &readfds, NULL, NULL, &timeout);     // wait for data or timeout here

        if(res < 0 || !FD_ISSET(sock, &readfds)) {          // if select() failed or cannot read from fd, skip rest
            continue;
        }

        ssize_t recvCnt = recv(sock, bfr, sizeof(bfr), 0);  // receive now

        if(recvCnt < 1) {                                   // nothing received?
            continue;
        }

    }

    close(sock);

    delete fifoAudio;
    delete fifoVideo;

    printf("recvThreadCode terminated.");
    return 0;
}

void createRecvThreadIfNeeded(void)
{
    if(threadCreated) {
        return;
    }

    int res = pthread_create(&recvThreadInfo, NULL, (void* (*)(void*)) recvThreadCode, NULL);

    if(res != 0) {
        printf("Failed to create recv thread");
        return;
    }

    threadCreated = true;
    printf("recv thread created");
    pthread_setname_np(recvThreadInfo, "recvThread");
}
