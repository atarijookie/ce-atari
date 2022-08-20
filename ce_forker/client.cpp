#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <pty.h>
#include <sys/file.h>
#include <errno.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

struct termios ctrlOriginal;

#define SOCK_PATH       "/tmp/ce_"

// Link with -lutil

//----------------------------------------------------
void startProcesses(void);
int openSocket(const char *name);
void closeFd(int& fd);
volatile sig_atomic_t sigintReceived = 0;
bool lowRes = false;            // if true, show low resolution, if false show mid resolution (starts default in mid res)

void sigint_handler(int sig)
{
    printf("\n\nSome SIGNAL received, terminating.\n");
    sigintReceived = 1;
}

void setTerminalOptions(void)
{
    struct termios ctrl;
    tcgetattr(STDIN_FILENO, &ctrl);
    ctrlOriginal = ctrl;        // make copy of original settins

    ctrl.c_lflag &= ~ICANON;    // turning off canonical mode makes input unbuffered
    ctrl.c_lflag &= ~ECHO;      // turn off echo on master's side
    ctrl.c_lflag &= ~ISIG;      // turn off signal generation on Ctrl+C and similar ones

    tcsetattr(STDIN_FILENO, TCSANOW, &ctrl);
}

void restoreTerminalSettings(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &ctrlOriginal);
}

#define MIN(X, Y)   ((X) < (Y) ? (X) : (Y))

ssize_t forwardData(int& fdIn, int& fdOut, char* bfr, int bfrLen, bool fromSock)
{
    int bytesAvailable = 0;
    int ires = ioctl(fdIn, FIONREAD, &bytesAvailable);       // how many bytes we can read?

    if(ires == -1 || bytesAvailable < 1) {
        return 0;
    }

    size_t bytesRead = MIN(bytesAvailable, bfrLen); // how many bytes can we read into buffer?
    ssize_t sres = 0;

    if(fromSock) {      // if from sock to pty, then use recv to get data from sock
        sres = recv(fdIn, bfr, bytesRead, MSG_NOSIGNAL);      // read to buffer
    } else {
        sres = read(fdIn, bfr, bytesRead);      // read to buffer
    }

    if(sres < 0 && errno == EPIPE) {            // close fd when socket was closed
        closeFd(fdIn);
        return 0;
    }

    if(!fromSock) {         // if reading from keyboard here
        for(int i=0; i<bytesRead; i++) {
            if(bfr[i] == 0x1a) {        // expected quit key received? (Pause/Break) quit
                sigintReceived = 1;
            }

            if(bfr[i] == 0x23) {        // '#' key pressed? transform into set resolution command
                lowRes = !lowRes;
                bfr[i] = lowRes ? 0xfa : 0xfb;
            }
        }
    }

    if(fromSock) {      // if from sock to pty, then use write to send data to pty
        sres = write(fdOut, (const void *)bfr, bytesRead);
    } else {
        sres = send(fdOut, (const void *)bfr, bytesRead, MSG_NOSIGNAL);
    }

    if(sres < 0 && errno == EPIPE) {            // close fd when socket was closed
        closeFd(fdOut);
        return 0;
    }

    return sres;
}

int main(int argc, char *argv[])
{
    const char* serviceName = "term";

    if(argc > 1) {
        serviceName = argv[1];
    }

    printf("\n\nWill connect to service: %s\n", serviceName);

    if(signal(SIGINT, sigint_handler) == SIG_ERR) {         // register SIGINT handler
        printf("Cannot register SIGINT handler!\n");
    }

    if(signal(SIGHUP, sigint_handler) == SIG_ERR) {         // register SIGHUP handler
        printf("Cannot register SIGHUP handler!\n");
    }

    int fd = 0;
    while(fd <= 0 && !sigintReceived) {     // while not connected
        printf("Connecting...\n");
        fd = openSocket(serviceName);       // connect to server

        if(fd <= 0) {
            sleep(3);
        }
    }

    setTerminalOptions();       // disable echo and caching

    printf("Entering loop...\n\n>>> To quit, press PAUSE/BREAK key, press '#' to switch resolution (40/80 cols)! <<<\n\n");
    sleep(1);

    char bfr[512];
    while(!sigintReceived) {
        ssize_t got = 0;
        int stdOut = STDOUT_FILENO;
        int stdIn = STDIN_FILENO;

        got += forwardData(fd, stdOut, bfr, sizeof(bfr), true);      // from sock to terminal
        got += forwardData(stdIn, fd, bfr, sizeof(bfr), false);      // from keyboard to sock

        if(got < 1) {
            usleep(1000);
        }
    }

    restoreTerminalSettings();  // enable echo and caching
    closeFd(fd);                // close socket

    printf("Terminated...\n");
    return 0;
}

int openSocket(const char *name)
{
    char fullname[128];

    // construct filename for socket
    strcpy(fullname, SOCK_PATH);        // start with path
    strcat(fullname, name);             // add socket name

    struct sockaddr_un addr;
    ssize_t numRead;

    // Create a new client socket with domain: AF_UNIX, type: SOCK_STREAM, protocol: 0
    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sfd == -1) {
        return -1;
    }

    // Construct server address, and make the connection.
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, fullname, sizeof(addr.sun_path) - 1);

    if (connect(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1) {
        return -1;
    }

    return sfd;
}

void closeFd(int& fd)
{
    if(fd > 0) {
        close(fd);
        fd = 0;
    }
}
