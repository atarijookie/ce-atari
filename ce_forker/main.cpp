#include <stdio.h>
#include <string.h>
#include <string>
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
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <limits.h>

/*
*
* appviasock usage:
* appviasock /PATH/TO/SOCKET 'command_to_execute -with -parameters'
*
* e.g.:
* appviasock /tmp/ce_shell term
* appviasock /tmp/ce_conf /ce/app/ce_conf.sh
*
*/

// Link with -lutil

struct termios ctrlOriginal;

//----------------------------------------------------
volatile int fProcPid;  // PID of child if running
int fdPty;              // fd of the PTY which the process as input + output
int fdListen;           // fd of the socked which listens for new incoming connections
int fdClient;           // fd of the socket which is used for sending + receiving data to CE main app

char pathPid[256];
char pathSocket[256];
char command[256];

#define BACKLOG 5

//----------------------------------------------------
void startProcess(void);
void terminateAllChildren(uint32_t sleepTime);
void closeAllFds(void);
void forkLinuxTerminal(void);
void forkCommand(void);
void openListeningSockets(void);
int openSocket(void);
void closeFd(int& fd);
bool otherInstanceIsRunning(void);
uint32_t getCurrentMs(void);
uint32_t getEndTime(uint32_t offsetFromNow);

volatile sig_atomic_t sigintReceived = 0;

void handlerSIGCHLD(int sig)
{
    pid_t childPid = wait(NULL);
    printf("child terminated: %d\n", childPid);
    fProcPid = 0;                  // we don't have child with this PID anymore
}

void sigint_handler(int sig)
{
    printf("Some SIGNAL received, terminating.\n");
    sigintReceived = 1;
}

#define MIN(X, Y)   ((X) < (Y) ? (X) : (Y))

void forwardData(int& fdIn, int& fdOut, char* bfr, int bfrLen, bool fromSock)
{
    int bytesAvailable = 0;
    int ires = ioctl(fdIn, FIONREAD, &bytesAvailable);       // how many bytes we can read?

    if(ires == -1 || bytesAvailable < 1) {
        if(fromSock && ires == 0 && bytesAvailable == 0) {  // when reading from socket, ioctl succeeded, but bytes that can be read are 0, client has disconnected
            printf("Client disconnected, closed input socket.\n");
            closeFd(fdIn);              // close this socket
            terminateAllChildren(100);  // also terminate the child app - to get new app session next time client connects
        }

        return;
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
        return;
    }

    if(fromSock) {                              // if data comes from sock, we should check if it isn't one of the commands
        for(int i=0; i<bytesRead; i++) {
            if(bfr[i] == 0xfa || bfr[i] == 0xfb) {      // LOW of MED resolution cmd?
                winsize ws;
                ws.ws_col = (bfr[i] == 0xfa) ? 40 : 80; // 40 cols for LOW, 80 cols for MED resolution
                ws.ws_row = 23;                         // 23 rows
                ioctl(fdPty, TIOCSWINSZ, &ws);
                bfr[i] = ' ';                           // replace with space
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
        return;
    }
}

int main(int argc, char *argv[])
{
    // make sure the correct number of arguments is specified
    if(argc != 3) {
        printf("\n\n%s -- Wrong number of arguments specified.\nUsage:\n", argv[0]);
        printf("%s /PATH/TO/SOCKET 'command_to_execute -with -parameters'\n", argv[0]);
        printf("Terminated.\n\n");
        return 0;
    }

    // copy in the socket path and command
    memset(pathSocket, 0, sizeof(pathSocket));
    memset(command, 0, sizeof(command));
    strncpy(pathSocket, argv[1], sizeof(pathSocket) - 1);
    strncpy(command, argv[2], sizeof(command) - 1);

    if(otherInstanceIsRunning()) {
        printf("\n\nOther instance is running for this socket, terminating!\n");
        return 0;
    }

    printf("\n\nStarting appviasock\n");

    if(signal(SIGCHLD, handlerSIGCHLD) == SIG_ERR) {
        printf("Cannot register SIGCHLD handler!\n");
    }

    if(signal(SIGINT, sigint_handler) == SIG_ERR) {         // register SIGINT handler
        printf("Cannot register SIGINT handler!\n");
    }

    if(signal(SIGHUP, sigint_handler) == SIG_ERR) {         // register SIGHUP handler
        printf("Cannot register SIGHUP handler!\n");
    }

    openListeningSockets();     // open listening socket for all processes with defined types

    fd_set fdSet;               // fd flags
    struct timeval timeout;

    struct sockaddr_un client_sockaddr;
    memset(&client_sockaddr, 0, sizeof(struct sockaddr_un));

    printf("Entering main loop...\n");

    char bfr[512];
    while(!sigintReceived) {
        FD_ZERO(&fdSet);        // clear flags

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // set all valid FDs to set
        if(fdListen > 0)  FD_SET(fdListen, &fdSet);     // check for new connections

        if(fdClient > 0) {                // if got some client connected
            FD_SET(fdClient, &fdSet);     // wait for data from client

            if(fProcPid <= 0) {                 // don't have PID?
                startProcess();                 // fork process and get handle to pty
            }

            if(fdPty > 0) {                     // hopefully we got the fd now
                FD_SET(fdPty, &fdSet);
            }
        }

        // wait for some FD to be ready for reading
        if (select(FD_SETSIZE, &fdSet, NULL, NULL, &timeout) <= 0) {    // error or timeout? try again
            continue;
        }

        // find out which FD is ready and handle it
        // first try to accept all new connections
        if(fdListen > 0) {
            if(FD_ISSET(fdListen, &fdSet)) {            // listening socket ready?
                closeFd(fdClient);                      // close communication socket if open
                fdClient = accept(fdListen, (struct sockaddr *)&client_sockaddr, (socklen_t*)&client_sockaddr);  // accept new connection
                printf("Accepted connection from client.\n");
            }
        }

        if(fdClient > 0 && FD_ISSET(fdClient, &fdSet)) {        // got data waiting in sock?
            forwardData(fdClient, fdPty, bfr, sizeof(bfr), true);       // from socket to pty
        }

        if(fdPty > 0 && FD_ISSET(fdPty, &fdSet)) {              // got data waiting in pty?
            forwardData(fdPty, fdClient, bfr, sizeof(bfr), false);     // from pty to socket
        }
    }

    terminateAllChildren(1000);
    closeAllFds();

    printf("Terminated...\n");
    return 0;
}

void openListeningSockets(void)
{
    if(fdListen != 0) {           // already got listening socket? skip it
        return;
    }

    // TODO: get sockName
    fdListen = openSocket();      // create listening socket
}

void startProcess(void)
{
    closeFd(fdPty);               // close fd to pty if exists

    if(fProcPid > 0) {                 // kill process by PID if exists
        printf("startProcess - will kill PID: %d\n", fProcPid);
        kill(fProcPid, SIGKILL);
        fProcPid = 0;
    }

    if(strcasecmp(command, "term") == 0) {      // if command is just term, then fork linux terminal
        forkLinuxTerminal();
    } else {                            // for other commands fork pty and run specified command in it
        forkCommand();
    }

    usleep(100);                        // wait a little until process opens pty and forks
    // in this moment we should have valid pid and fdPty
}

void forkLinuxTerminal(void)
{
    printf("fork() linux terminal\n");

    int childPid = forkpty(&fdPty, NULL, NULL, NULL);

    if(childPid == 0) {                             // code executed only by child
        const char *shell = "/bin/sh";              // default shell
//      const char *term = "vt52";
        shell = getenv("SHELL");

//        if(setenv("TERM", term, 1) < 0) {
//            fprintf(stderr, "Failed to setenv(\"TERM\", \"%s\"): %s\n", term, strerror(errno));
//        }

        execlp(shell, shell, "-i", (char *) NULL);  // -i for interactive
        return;
    }

    // parent continues here
    fProcPid = childPid;          // store PID
    printf("child PID: %d\n", childPid);

    // set terminal size
    winsize ws;
    ws.ws_col = 80;                 // start with 80 cols for MED resolution
    ws.ws_row = 23;                 // 23 rows
    ioctl(fdPty, TIOCSWINSZ, &ws);
}

void forkCommand(void)
{
    printf("fork() command: %s\n", command);

    int childPid = forkpty(&fdPty, NULL, NULL, NULL);

    if(childPid == 0) {                             // code executed only by child
        execlp(command, command, (char *) NULL);
        return;
    }

    // parent continues here
    fProcPid = childPid;          // store PID
    printf("child PID: %d\n", childPid);

    // set terminal size
    winsize ws;
    ws.ws_col = 80;                 // start with 80 cols for MED resolution
    ws.ws_row = 23;                 // 23 rows
    ioctl(fdPty, TIOCSWINSZ, &ws);    
}

void closeAllFds(void)
{
    closeFd(fdPty);
    closeFd(fdListen);
    closeFd(fdClient);
}

void terminateAllChildren(uint32_t sleepTime)
{
    if(fProcPid <= 0) {                 // no pid? skip
        return;
    }

    printf("will kill PID: %d with SIGTERM\n", fProcPid);

    kill(fProcPid, SIGTERM);           // ask child to terminate - nicely

    uint32_t endTime = getEndTime(sleepTime);

    while(true) {                       // wait for the specified time between SIGTERM and SIGKILL
        if(getpgid(fProcPid) < 0) {    // other process not running? quit now
            break;
        }

        if(getCurrentMs() >= endTime) { // time out? quit now
            break;
        }
    }

    if(fProcPid > 0 && getpgid(fProcPid) >= 0) {    // other process still running?
        printf("will kill PID: %d with SIGKILL\n", fProcPid);
        kill(fProcPid, SIGKILL);       // ask child to terminate - this instant
    }

    fProcPid = 0;                      // forget the pid
}

int openSocket(void)
{
    struct sockaddr_un addr;

    // Create a new server socket with domain: AF_UNIX, type: SOCK_STREAM, protocol: 0
    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sfd == -1) {
        return -1;
    }

    // Delete any file that already exists at the address. Make sure the deletion
    // succeeds. If the error is just that the file/directory doesn't exist, it's fine.
    if (remove(pathSocket) == -1 && errno != ENOENT) {
        return -1;
    }

    // Zero out the address, and set family and path.
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, pathSocket, sizeof(addr.sun_path) - 1);

    // Bind the socket to the address. Note that we're binding the server socket
    // to a well-known address so that clients know where to connect.
    if (bind(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1) {
        return -1;
    }

    // mark the socket as *passive*
    if (listen(sfd, BACKLOG) == -1) {
        return -1;
    }

    // allow for group and other to read and write to this sock
    chmod(pathSocket, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

    // return listening socket. we need to listen and accept on it.
    return sfd;
}

void closeFd(int& fd)
{
    if(fd > 0) {
        close(fd);
        fd = 0;
    }
}

bool otherInstanceIsRunning(void)
{
    FILE * f;
    int other_pid = 0;
    int self_pid = 0;

    // create path to PID file from path to socket + '.pid'
    strcpy(pathPid, pathSocket);
    strcat(pathPid, ".pid");

    self_pid = getpid();
    printf("Main process PID: %d\n", self_pid);

    f = fopen(pathPid, "r");

    if(f) {                 // if file opened
        int r = fscanf(f, "%d", &other_pid);
        fclose(f);

        if(r == 1) {        // got the PID from file
            if(getpgid(other_pid) >= 0) {
                printf("Other instance running: YES\n");
                return true;
            } else {
                printf("Other instance running: NO\n");
            }
        } else {
            printf("\nFailed to read PID from file %s\n", pathPid);
        }
    } else {
        printf("\nFailed to open PID file for reading: %s\n", pathPid);
    }

    // write our PID to file
    f = fopen(pathPid, "w");
    if(f) {
        fprintf(f, "%d", self_pid);
        fclose(f);
    } else {
        printf("\nFailed to write PID to file: %s\n", pathPid);
    }

    // allow for group and other to read and write to this pid file
    chmod(pathPid, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

    return false;
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