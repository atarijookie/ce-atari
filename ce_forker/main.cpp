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
#include <sys/socket.h>
#include <sys/un.h>

/*
*
* ce_forker usage:
* ce_forker /PATH/TO/SOCKET 'command_to_execute -with -parameters'
*
* e.g.:
* ce_forker /tmp/ce_shell term
* ce_forker /tmp/ce_conf /ce/app/ce_conf.sh
*
*/

// Link with -lutil

struct termios ctrlOriginal;

//----------------------------------------------------
typedef struct {
    volatile int pid;   // PID of child if running

    int fdPty;          // fd of the PTY which the process as input + output

    int fdListen;       // fd of the socked which listens for new incoming connections
    int fdClient;       // fd of the socket which is used for sending + receiving data to CE main app
} ForkedProc;

ForkedProc fProc;

char pathSocket[256];
char command[256];

#define BACKLOG 5

//----------------------------------------------------
void startProcess(void);
void terminateAllChildren(void);
void closeAllFds(void);
void forkLinuxTerminal(void);
void forkCommand(void);
void openListeningSockets(void);
int openSocket(void);
void closeFd(int& fd);

volatile sig_atomic_t sigintReceived = 0;

void handlerSIGCHLD(int sig)
{
    pid_t childPid = wait(NULL);
    printf("child terminated: %d\n", childPid);
    fProc.pid = 0;                  // we don't have child with this PID anymore
}

void sigint_handler(int sig)
{
    printf("Some SIGNAL received, terminating.\n");
    sigintReceived = 1;
}

#define MIN(X, Y)   ((X) < (Y) ? (X) : (Y))

int forwardData(int fdIn, int fdOut, char *bfr, int bfrLen)
{
    int bytesAvailable = 0;
    int ires = ioctl(fdIn, FIONREAD, &bytesAvailable);       // how many bytes we can read?

    if(ires == -1 || bytesAvailable < 1) {
        return 0;
    }

    size_t bytesRead = MIN(bytesAvailable, bfrLen); // how many bytes can we read into buffer?
    ires = read(fdIn, bfr, bytesRead);      // read to buffer
    write(fdOut, (const void *)bfr, bytesRead);

    return ires;
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

    printf("\n\nStarting ce_forker\n");

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
        if(fProc.fdListen > 0)  FD_SET(fProc.fdListen, &fdSet);     // check for new connections

        if(fProc.fdClient > 0) {                // if got some client connected
            FD_SET(fProc.fdClient, &fdSet);     // wait for data from client

            if(fProc.pid <= 0) {                // don't have PID?
                startProcess();                 // fork process and get handle to pty
            }

            if(fProc.fdPty > 0) {               // hopefully we got the fd now
                FD_SET(fProc.fdPty, &fdSet);
            }
        }

        // wait for some FD to be ready for reading
        if (select(FD_SETSIZE, &fdSet, NULL, NULL, &timeout) <= 0) {    // error or timeout? try again
            continue;
        }

        // find out which FD is ready and handle it
        // first try to accept all new connections
        if(fProc.fdListen > 0) {
            if(FD_ISSET(fProc.fdListen, &fdSet)) {  // listening socket ready?
                closeFd(fProc.fdClient);              // close communication socket if open
                fProc.fdClient = accept(fProc.fdListen, (struct sockaddr *)&client_sockaddr, (socklen_t*)&client_sockaddr);  // accept new connection
                printf("Accepted connection from client.\n");
            }
        }

        if(fProc.fdClient > 0 && FD_ISSET(fProc.fdClient, &fdSet)) {        // got data waiting in sock?
            forwardData(fProc.fdClient, fProc.fdPty, bfr, sizeof(bfr));     // from socket to pty
        }

        if(fProc.fdPty > 0 && FD_ISSET(fProc.fdPty, &fdSet)) {              // got data waiting in pty?
            forwardData(fProc.fdPty, fProc.fdClient, bfr, sizeof(bfr));     // from socket to pty
        }
    }

    terminateAllChildren();
    closeAllFds();

    printf("Terminated...\n");
    return 0;
}

void openListeningSockets(void)
{
    if(fProc.fdListen != 0) {           // already got listening socket? skip it
        return;
    }

    // TODO: get sockName
    fProc.fdListen = openSocket();      // create listening socket
}

void startProcess(void)
{
    closeFd(fProc.fdPty);               // close fd to pty if exists

    if(fProc.pid > 0) {                 // kill process by PID if exists
        kill(fProc.pid, SIGKILL);
        fProc.pid = 0;
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

    int childPid = forkpty(&fProc.fdPty, NULL, NULL, NULL);

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
    fProc.pid = childPid;          // store PID
}

void forkCommand(void)
{
    printf("fork() command: %s\n", command);

    int childPid = forkpty(&fProc.fdPty, NULL, NULL, NULL);

    if(childPid == 0) {                             // code executed only by child
        execlp(command, command, (char *) NULL);
        return;
    }

    // parent continues here
    fProc.pid = childPid;          // store PID
}

void closeAllFds(void)
{
    closeFd(fProc.fdPty);
    closeFd(fProc.fdListen);
    closeFd(fProc.fdClient);
}

void terminateAllChildren(void)
{
    if(!fProc.pid) {                    // no pid? skip
        return;
    }

    kill(fProc.pid, SIGTERM);           // ask child to terminate - nicely

    sleep(1);                           // give children some time to terminate

    kill(fProc.pid, SIGKILL);           // ask child to terminate - this instant
    fProc.pid = 0;                      // forget the pid
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
