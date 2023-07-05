#include <string>
#include <unistd.h>
#include <pty.h>
#include <sys/un.h>
#include <netinet/in.h>

#define PORT 15000

int fdListen;         // fd of the socked which listens for new incoming connections
int fdClient;         // fd of the socket which is used for sending + receiving data

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void closeFd(int& fd)
{
    if(fd > 0) {
        close(fd);
        fd = 0;
    }
}

void closeAllFds(void)
{
    closeFd(fdListen);
    closeFd(fdClient);
}

void acceptConnectionIfWaiting(int& fdListening, fd_set* fdSet, int& fdOfClient)
{
    if(fdListening <= 0) {      // no listening socket? quit here
        return;
    }

    struct sockaddr_un client_sockaddr;
    memset(&client_sockaddr, 0, sizeof(struct sockaddr_un));

    if(!FD_ISSET(fdListening, fdSet)) {            // listening socket not ready? quit
        return;
    }

    closeFd(fdOfClient);                            // close communication socket if open
    fdOfClient = accept(fdListening, (struct sockaddr *) &client_sockaddr, (socklen_t*) &client_sockaddr);  // accept new connection
}

int openSocket(void)
{
    struct sockaddr_un addr;

    // Zero out the address, and set family and path.
    sockaddr_in servAddr;
    bzero((char*) &servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(PORT);

    //open stream oriented socket with internet address
    int serverSd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSd < 0) {
        printf("Error establishing the server socket\n");
        exit(0);
    }

    //bind the socket to its local address
    int bindStatus = bind(serverSd, (struct sockaddr*) &servAddr, sizeof(servAddr));
    if(bindStatus < 0) {
        printf("Error binding socket to local address\n");
        exit(0);
    }

    // mark the socket as *passive*
    if (listen(serverSd, 5) == -1) {
        return -1;
    }

    // return listening socket. we need to listen and accept on it.
    return serverSd;
}

void openListeningSocket(int& fdListening)
{
    if(fdListening != 0) {                  // already got listening socket? skip it
        return;
    }

    fdListening = openSocket();             // create listening socket

    if(fdListening < 0) {
        printf("Failed to create listening socket!\n");
    }
}

uint32_t getCurrentMs(void)
{
    struct timespec tp;
    int res;

    res = clock_gettime(CLOCK_MONOTONIC, &tp);                  // get current time

    if(res != 0) {                                              // if failed, fail
        return 0;
    }

    uint32_t val = (tp.tv_sec * 1000) + (tp.tv_nsec / 1000000);    // convert to milli seconds
    return val;
}

uint32_t getEndTime(uint32_t offsetFromNow)
{
    uint32_t val;

    val = getCurrentMs() + offsetFromNow;

    return val;
}

int main(int argc, char *argv[])
{
    printf("\n\nStarting server\n");

    openListeningSocket(fdListen);

    if(fdListen < 0) {
        printf("This app has no use without running listening socket, so terminating!\n\n");
        return 0;
    }

    fd_set fdSet;               // fd flags
    struct timeval timeout;

    struct sockaddr_un client_sockaddr;
    memset(&client_sockaddr, 0, sizeof(struct sockaddr_un));

    printf("Entering main loop...\n");

    uint32_t timeWhenConnected = getCurrentMs();

    #define BFR_SIZE    65536
    char bfr[BFR_SIZE];

    uint32_t showTime = getCurrentMs();
    uint32_t receivedBytes = 0;

    while(true) {
        uint32_t now = getCurrentMs();
        uint32_t diff = now - showTime;
        if(diff >= 1000) {
            showTime = now;
            float kBps = ((float) receivedBytes) / 1024.0;        // bytes to kilo-bytes
            float fDiff = ((float) diff) / 1000;            // ms to s
            kBps = kBps / fDiff;

            receivedBytes = 0;
            printf("Receiving: %.1f kB/s\n", kBps);
        }

        FD_ZERO(&fdSet);        // clear flags

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // set all valid FDs to set
        if(fdListen > 0) FD_SET(fdListen, &fdSet);      // check for new MAIN connection
        if(fdClient > 0) FD_SET(fdClient, &fdSet);      // check for data connection

        // wait for some FD to be ready for reading
        if (select(FD_SETSIZE, &fdSet, NULL, NULL, &timeout) <= 0) {    // error or timeout? try again
            continue;
        }

        // find out which FD is ready and handle it
        acceptConnectionIfWaiting(fdListen, &fdSet, fdClient);    // accept RW connection if waiting

        if(FD_ISSET(fdClient, &fdSet)) {
            int bytesAvailable = 0;
            int ires = ioctl(fdClient, FIONREAD, &bytesAvailable);       // how many bytes we can read?

            if(ires < 0) {
                continue;
            }

            int bytesRead = MIN(bytesAvailable, BFR_SIZE);
            recv(fdClient, bfr, bytesRead, MSG_NOSIGNAL);       // read to buffer
            receivedBytes += bytesRead;                         // add to bytes received during last recv
        }
    }

    closeAllFds();

    printf("Terminated...\n");
    return 0;
}
