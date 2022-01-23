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

struct termios ctrlOriginal;

// Link with -lutil

#define SOCK_PATH       "/tmp/ce_"

//----------------------------------------------------
#define PROC_TYPE_NONE          0
#define PROC_TYPE_LINUX_TERM    1
#define PROC_TYPE_CE_CONF       2
#define PROC_TYPE_CE_DOWNLOADER 3

typedef struct {
    int type;           // type of proces - PROC_TYPE_...
    volatile int pid;   // PID of child if running

    int fdPty;          // fd of the PTY which the process as input + output

    int fdListen;       // fd of the socked which listens for new incoming connections
    int fdClient;       // fd of the socket which is used for sending + receiving data to CE main app
} ForkedProc;

#define FORKED_PROC_COUNT       3
ForkedProc fProcs[FORKED_PROC_COUNT];

#define BACKLOG 5

//----------------------------------------------------
void startProcess(int i);
void terminateAllChildren(void);
void closeAllFds(void);
void forkLinuxTerminal(ForkedProc *fproc);
void openListeningSockets(void);
int openSocket(const char *name);
void closeFd(int& fd);
const char* sockNameForType(int type);

volatile sig_atomic_t sigintReceived = 0;

void handlerSIGCHLD(int sig)
{
    pid_t childPid = wait(NULL);

    printf("child terminated: %d\n", childPid);

    // find which child has terminated and set its PID to 0
    for(int i=0; i<FORKED_PROC_COUNT; i++) {
        if(fProcs[i].pid == childPid) {         // child at this index is the one that was terminated
            fProcs[i].pid = 0;                  // we don't have child with this PID anymore
            break;
        }
    }
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
    printf("\n\nStarting...\n");

    if(signal(SIGCHLD, handlerSIGCHLD) == SIG_ERR) {
        printf("Cannot register SIGCHLD handler!\n");
    }

    if(signal(SIGINT, sigint_handler) == SIG_ERR) {         // register SIGINT handler
        printf("Cannot register SIGINT handler!\n");
    }

    if(signal(SIGHUP, sigint_handler) == SIG_ERR) {         // register SIGHUP handler
        printf("Cannot register SIGHUP handler!\n");
    }

    fProcs[0].type = PROC_TYPE_LINUX_TERM;      // set type
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
        for(int i=0; i<FORKED_PROC_COUNT; i++) {
            if(fProcs[i].fdListen > 0)  FD_SET(fProcs[i].fdListen, &fdSet);     // check for new connections

            if(fProcs[i].fdClient > 0) {                // if got some client connected
                FD_SET(fProcs[i].fdClient, &fdSet);     // wait for data from client

                if(fProcs[i].pid <= 0) {                // don't have PID?
                    startProcess(i);                    // fork process and get handle to pty
                }

                if(fProcs[i].fdPty > 0) {               // hopefully we got the fd now
                    FD_SET(fProcs[i].fdPty, &fdSet);
                }
            }
        }

        // wait for some FD to be ready for reading
        if (select(FD_SETSIZE, &fdSet, NULL, NULL, &timeout) <= 0) {    // error or timeout? try again
            continue;
        }

        // find out which FD is ready and handle it
        for(int i=0; i<FORKED_PROC_COUNT; i++) {            // go through the list of processes
            // first try to accept all new connections
            if(fProcs[i].fdListen > 0) {
                if(FD_ISSET(fProcs[i].fdListen, &fdSet)) {  // listening socket ready?
                    closeFd(fProcs[i].fdClient);              // close communication socket if open
                    fProcs[i].fdClient = accept(fProcs[i].fdListen, (struct sockaddr *)&client_sockaddr, (socklen_t*)&client_sockaddr);  // accept new connection
                    printf("Accepted connection from client.\n");

                    // for linux terminal type show this message on client to let him know he has entered remote terminal (without it it's harder to notice)
                    if(fProcs[i].type == PROC_TYPE_LINUX_TERM && fProcs[i].fdClient > 0) {
                        const char* msg = ">>> YOU ARE ON REMOTE TERMINAL <<<\n";
                        write(fProcs[i].fdClient, (const void *) msg, strlen(msg));
                    }
                }
            }

            if(fProcs[i].fdClient > 0 && FD_ISSET(fProcs[i].fdClient, &fdSet)) {            // got data waiting in sock?
                forwardData(fProcs[i].fdClient, fProcs[i].fdPty, bfr, sizeof(bfr));       // from socket to pty
            }

            if(fProcs[i].fdPty > 0 && FD_ISSET(fProcs[i].fdPty, &fdSet)) {              // got data waiting in pty?
                forwardData(fProcs[i].fdPty, fProcs[i].fdClient, bfr, sizeof(bfr));       // from socket to pty
            }
        }
    }

    terminateAllChildren();
    closeAllFds();

    printf("Terminated...\n");
    return 0;
}

void openListeningSockets(void)
{
    for(int i=0; i<FORKED_PROC_COUNT; i++) {            // go through the list of processes
        if(fProcs[i].type == PROC_TYPE_NONE) {          // if no type specified, skip
            continue;
        }

        if(fProcs[i].fdListen != 0) {                   // already got listening socket? skip it
            continue;
        }

        const char* sockName = sockNameForType(fProcs[i].type); // get socket name for process type
        fProcs[i].fdListen = openSocket(sockName);              // create listening socket
    }
}

void startProcess(int i)
{
    if(i < 0 || i > FORKED_PROC_COUNT) {            // index out of bounds? fail
        return;
    }

    ForkedProc *fp = &fProcs[i];        // get pointer to right process structure

    closeFd(fp->fdPty);                 // close fd to pty if exists

    if(fp->pid > 0) {                  // kill process by PID if exists
        kill(fp->pid, SIGKILL);
        fp->pid = 0;
    }

    // start process based on type
    switch(fp->type) {
        case PROC_TYPE_LINUX_TERM:      forkLinuxTerminal(&fProcs[i]); break;
        case PROC_TYPE_CE_CONF:         break;
        case PROC_TYPE_CE_DOWNLOADER:   break;
    }

    usleep(100);        // wait a little until process opens pty and forks
    // in this moment we should have valid pid and fdPty
}

const char* sockNameForType(int type)
{
    switch(type) {
        case PROC_TYPE_LINUX_TERM:      return "term";
        case PROC_TYPE_CE_CONF:         return "conf";
        case PROC_TYPE_CE_DOWNLOADER:   return "dwnl";
    }

    return "";
}

void forkLinuxTerminal(ForkedProc *fproc)
{
    printf("fork() linux terminal\n");

    int childPid = forkpty(&fproc->fdPty, NULL, NULL, NULL);

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
    fproc->pid = childPid;          // store PID
}

void closeAllFds(void)
{
    for(int i=0; i<FORKED_PROC_COUNT; i++) {    // go through the list of processes
        closeFd(fProcs[i].fdPty);
        closeFd(fProcs[i].fdListen);
        closeFd(fProcs[i].fdClient);
    }
}

void terminateAllChildren(void)
{
    int got = 0;                                // how many running child processes we got

    for(int i=0; i<FORKED_PROC_COUNT; i++) {    // go through the list of processes
        if(!fProcs[i].pid) {                    // no pid? skip
            continue;
        }

        kill(fProcs[i].pid, SIGTERM);           // ask child to terminate - nicely
        got++;
    }

    if(!got) {                                  // no children? just quit immediately
        return;
    }

    sleep(1);                                   // give children some time to terminate

    for(int i=0; i<FORKED_PROC_COUNT; i++) {    // go through the list of processes
        if(!fProcs[i].pid) {                    // no pid? skip
            continue;
        }

        kill(fProcs[i].pid, SIGKILL);           // ask child to terminate - this instant
        fProcs[i].pid = 0;                      // forget the pid
    }
}

int openSocket(const char *name)
{
    char fullname[128];

    // construct filename for pipe going from process to ce - READ pipe
    strcpy(fullname, SOCK_PATH);        // start with path
    strcat(fullname, name);             // add socket name

    struct sockaddr_un addr;

    // Create a new server socket with domain: AF_UNIX, type: SOCK_STREAM, protocol: 0
    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sfd == -1) {
        return -1;
    }

    // Delete any file that already exists at the address. Make sure the deletion
    // succeeds. If the error is just that the file/directory doesn't exist, it's fine.
    if (remove(fullname) == -1 && errno != ENOENT) {
        return -1;
    }

    // Zero out the address, and set family and path.
    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, fullname, sizeof(addr.sun_path) - 1);

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
