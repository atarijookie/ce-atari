#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "extensiondefs.h"
#include "main.h"
#include "recv.h"
#include "fifo.h"
#include "utils.h"

bool threadCreated = false;
pthread_t recvThreadInfo;
extern volatile sig_atomic_t shouldStop;

Fifo* fifoAudio;
Fifo* fifoVideo;

void createUnixRecvStreamSocket(int &fdListen, const char* pathToSocket)
{
    if(fdListen >= 0) { // if the listening socket seems to be already created, just quit
        return;
    }

	// create a UNIX STREAM socket
	int sock = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sock < 0) {
	    log(LOG_WARNING, "createRecvSocket - failed to create socket!");
	    return;
	}

    fdListen = sock;                        // store socket fd

    fchmod(sock, S_IRUSR | S_IWUSR);        // restrict permissions before bind

    unlink(pathToSocket);                   // delete sock file if exists

    struct sockaddr_un addr;
    strcpy(addr.sun_path, pathToSocket);
    addr.sun_family = AF_UNIX;

    int res = bind(sock, (struct sockaddr *) &addr, strlen(addr.sun_path) + sizeof(addr.sun_family));
    if (res < 0) {
	    log(LOG_WARNING, "createRecvSocket - failed to bind socket to %s - errno: %d", pathToSocket, errno);
	    return;
    }

    listen(sock, 2);                        // marks the socket referred to by sockfd as a passive
    fcntl(fdListen, F_SETFL, O_NONBLOCK);   // change the socket into non-blocking

    chmod(addr.sun_path, 0666);             // loosen permissions

    log(LOG_DEBUG, "createRecvSocket - %s created, sock: %d", pathToSocket, sock);
}

void acceptSocketIfNeededAndPossible(int fdListen, int& fdClient)
{
    // if already got client socket, no need to do anything here
    if(fdClient >= 0) {
        return;
    }

    // don't have client socket, try accept()

    struct sockaddr_un remote;
    socklen_t addrSize = sizeof(remote);

    int newSock = accept(fdListen, (struct sockaddr *) &remote, &addrSize);

    if(newSock < 0) {       // nothing to accept, would block? quit
        log(LOG_DEBUG, "acceptSocketIfNeededAndPossible - nothing to accept");
        return;
    }

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500000;    // 500 ms timeout on blocking reads
    setsockopt(newSock, SOL_SOCKET, SO_RCVTIMEO, (const char*) &tv, sizeof(tv));

    log(LOG_DEBUG, "acceptSocketIfNeededAndPossible - accepted socket %d", newSock);

    // got the new client socket now
    fdClient = newSock;
}

void closeSocket(int& fd) {
    if(fd > 0) {
        close(fd);
        fd = -1;
    }
}

void readFromSockToFifo(int sock, Fifo* fifo, uint8_t* bfr, uint32_t bfrLen)
{
    int bytesAvailable;
    int rv = ioctl(sock, FIONREAD, &bytesAvailable);    // how many bytes we can read?

    if(rv == -1) {  // ioctl failed?
        bytesAvailable = 0;
        return;
    }

    int bytesToRead = MIN(bytesAvailable, (int) bfrLen);    // we can only read either bytes that are ready or up to buffer size

    ssize_t recvCnt = recv(sock, bfr, bytesToRead, 0);

    if(recvCnt > 0) {
        log(LOG_DEBUG, "readFromSockToFifo - bytesToRead: %d, recvCnt: %d", bytesToRead, recvCnt);

        mutexLock();
        fifo->addBfr(bfr, recvCnt);
        mutexUnlock();
    }
}

void *recvThreadCode(void *ptr)
{
    log(LOG_INFO, "recvThreadCode starting");

    // create listening sockets
    int sockListenAudio = -1, sockListenVideo = -1;
    createUnixRecvStreamSocket(sockListenAudio, SOCK_PATH_RECV_FFMPEG_AUDIO);
    createUnixRecvStreamSocket(sockListenVideo, SOCK_PATH_RECV_FFMPEG_VIDEO);

    // create receiving buffer and fifos
    uint8_t bfr[1024*1024];
    fifoAudio = new Fifo();
    fifoVideo = new Fifo();

    int sockRecvAudio = -1, sockRecvVideo = -1;

    while(!shouldStop) {
        struct timeval timeout;
        timeout.tv_sec = 1;         // short timeout
        timeout.tv_usec = 0;

        fd_set readFds;
        FD_ZERO(&readFds);

        // add listen sockets to fds
        FD_SET(sockListenAudio, &readFds);
        FD_SET(sockListenVideo, &readFds);

        // add audio / video receiving socket to read fds, if got it
        if(sockRecvAudio > 0) FD_SET(sockRecvAudio, &readFds);
        if(sockRecvVideo > 0) FD_SET(sockRecvVideo, &readFds);

        int maxSock = MAX(sockRecvAudio, sockRecvVideo);
        int res = select(maxSock + 1, &readFds, NULL, NULL, &timeout);     // wait for data or timeout here

        if(res < 0) {   // select timed out, no other handling
            log(LOG_DEBUG, "recvThreadCode - timeout");
            continue;
        }

        // if listening socket is ready to accept, then accept connection
        if(FD_ISSET(sockListenAudio, &readFds)) acceptSocketIfNeededAndPossible(sockListenAudio, sockRecvAudio);
        if(FD_ISSET(sockListenVideo, &readFds)) acceptSocketIfNeededAndPossible(sockListenVideo, sockRecvVideo);

        // got audio recv socket?
        if(sockRecvAudio > 0 && FD_ISSET(sockRecvAudio, &readFds)) {
            readFromSockToFifo(sockRecvAudio, fifoAudio, bfr, sizeof(bfr));
        }

        // got video recv socket?
        if(sockRecvVideo > 0 && FD_ISSET(sockRecvVideo, &readFds)) {
            readFromSockToFifo(sockRecvVideo, fifoVideo, bfr, sizeof(bfr));
        }
    }

    // close sockets
    closeSocket(sockRecvAudio);
    closeSocket(sockRecvVideo);
    closeSocket(sockListenAudio);
    closeSocket(sockListenVideo);

    // delete fifos
    delete fifoAudio;
    delete fifoVideo;

    log(LOG_INFO, "recvThreadCode terminated.");
    return 0;
}

void createRecvThreadIfNeeded(void)
{
    if(threadCreated) {
        return;
    }

    int res = pthread_create(&recvThreadInfo, NULL, (void* (*)(void*)) recvThreadCode, NULL);

    if(res != 0) {
        log(LOG_WARNING, "Failed to create recv thread");
        return;
    }

    threadCreated = true;
    log(LOG_DEBUG, "recv thread created");
    pthread_setname_np(recvThreadInfo, "recvThread");
}
