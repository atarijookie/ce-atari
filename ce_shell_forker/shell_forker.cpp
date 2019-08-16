#define _POSIX_C_SOURCE

#include <sys/types.h>
#include <sys/wait.h>
#include <linux/limits.h>

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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

//#define PIDFILE "/var/run/shell_forker.pid"
#define PIDFILE "/tmp/shell_forker.pid"

#define FIFO_PATH_OUT   "/tmp/ce_shell_out"
#define FIFO_PATH_IN    "/tmp/ce_shell_in"

// compile like this: g++ shell_forker.cpp -lutil -pthread

pid_t childPid;                               // pid of forked child

int linuxConsole_fdMaster;                    // file descriptors for linux console
int fifo_out, fifo_in;

bool otherInstanceIsRunning(void);
int  singleInstanceSocketFd;
void createFifos(void);
void *readWriteThreadCode(void *ptr);

int main(int argc, char *argv[])
{
    if(otherInstanceIsRunning()) {
        printf("\nOther instance of shell_forker is running, terminate it before starting a new one!\n\n\n");
        return 0;
    }

    pthread_t readWriteThreadInfo;
    pthread_create(&readWriteThreadInfo, NULL, readWriteThreadCode, NULL);

    while(1) {
        bool startIt = false;

        if(childPid != 0) {     // if got the child
            int status;
            waitpid(childPid, &status, 0);          // sleep until state of child changes
            printf("waitPid status: %d\n", status);
            startIt = true;
        } else {
            startIt = true;
        }

        if(startIt) {           // if should start child
            printf("starting child...\n");
            childPid = forkpty(&linuxConsole_fdMaster, NULL, NULL, NULL);

            if(childPid == 0) {                             // code executed only by child
                const char *shell = "/bin/sh";              // default shell
                const char *term = "vt52";
                shell = getenv("SHELL");

                if(access("/etc/terminfo/a/atari", R_OK) == 0)
                    term = "atari";

                if(setenv("TERM", term, 1) < 0) {
                    fprintf(stderr, "Failed to setenv(\"TERM\", \"%s\"): %s\n", term, strerror(errno));
                }

                execlp(shell, shell, "-i", (char *) NULL);  // -i for interactive
                return 0;
            }
        }

        // parent (full app) continues here
        printf("Forked child pid: %d\n", childPid);
    }

    printf("Terminated\n");
    return 0;
}

bool writePidFile(int self_pid)
{
    FILE *f = fopen(PIDFILE, "w");

    if(!f) {
        printf("otherInstanceIsRunning - failed to open %s for writing\n", PIDFILE);
        return false;   // write fail - maybe someone else is writing
    }

    fprintf(f, "%d", self_pid);

    if(fclose(f) == 0) {
        printf("otherInstanceIsRunning -- pid %d written to %s\n", self_pid, PIDFILE);
        return true;    // write good
    }

    printf("otherInstanceIsRunning -- FAILED to write pid %d to %s : %s\n", self_pid, PIDFILE, strerror(errno));
    return false;       // write fail
}

bool otherInstanceIsRunning(void)
{
    FILE * f;
    int other_pid = 0;
    int self_pid = 0;
    int res;
    char proc_path[256];
    char other_exe[PATH_MAX], *other_base;
    char self_exe[PATH_MAX], *self_base;

    memset(other_exe, 0, sizeof(other_exe));
    memset(self_exe, 0, sizeof(self_exe));

    self_pid = getpid();
    f = fopen(PIDFILE, "r");

    if(!f) {    // can't open file? other instance probably not running (or is, but can't figure out, so screw it)
        printf("otherInstanceIsRunning - couldn't fopen %s, returning false\n", PIDFILE);
        writePidFile(self_pid);
        return false;
    }

    // pid file opened good
    res = fscanf(f, "%d", &other_pid);
    fclose(f);

    if(res != 1) {    // if couldn't read from pid file, assume nothing is running
        printf("otherInstanceIsRunning - can't read pid from file %s, returning false\n", PIDFILE);
        writePidFile(self_pid);
        return false;
    }

    // read pid from pidfile went fine
    printf("otherInstanceIsRunning - %s pid=%d (own pid=%d)\n", PIDFILE, other_pid, self_pid);
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", other_pid);

    res = readlink("/proc/self/exe", self_exe, sizeof(self_exe));       // get filename of our process

    if(res < 0) {    // if failed to read link for our file, assume we're not running?
        printf("otherInstanceIsRunning readlink(%s) failed: %s", "/proc/self/exe\n", strerror(errno));
        writePidFile(self_pid);
        return false;
    }

    res =  readlink(proc_path, other_exe, sizeof(other_exe));           // try to find the filename of the other process with the other pid from file

    if(res < 0) {   // if failed to read this link, then the process with that pid doesn't exist anymore
        printf("otherInstanceIsRunning readlink(%s): %s - other process not running\n", proc_path, strerror(errno));
        writePidFile(self_pid);
        return false;
    }

    // if we got here, we've managed to find out filename, other process filename, check that other process still lives

    other_base = basename(other_exe);
    self_base = basename(self_exe);

    if(strcmp(other_base, self_base) == 0) {  // other process lives and has the same filename - other instance is running
        printf("otherInstanceIsRunning - found another instance of %s with pid %d\n", other_base, other_pid);
        return true;
    }

    // if we got here, some other process might be living, but it's not our filename, so asusme we're not running
    printf("otherInstanceIsRunning - something else runs (%s - %d) but it's not us (%s - %d), returning false\n", other_base, other_pid, self_base, self_pid);
    writePidFile(self_pid);
    return false;
}

void createFifos(void)
{
    int res, res2;

    fifo_in = -1;
    fifo_out = -1;

    res = mkfifo(FIFO_PATH_OUT, 0666);

    if(res != 0 && errno != EEXIST) {               // if mkfifo failed, and it's not 'file exists' error
        printf("ce_conf_createFifos -- mkfifo() failed, errno: %d\n", errno);
        return;
    }

    res = mkfifo(FIFO_PATH_IN, 0666);

    if(res != 0 && errno != EEXIST) {               // if mkfifo failed, and it's not 'file exists' error
        printf("ce_conf_createFifos -- mkfifo() failed, errno: %d\n", errno);
        return;
    }

    fifo_in  = open(FIFO_PATH_IN,  O_RDWR);         // will be used for reading only
    fifo_out = open(FIFO_PATH_OUT, O_RDWR);         // will be used for writing only

    if(fifo_in == -1 || fifo_out == -1) {
        printf("ce_conf_createFifos -- open() failed\n");
        return;
    }

    res  = fcntl(fifo_in,  F_SETFL, O_NONBLOCK);
    res2 = fcntl(fifo_out, F_SETFL, O_NONBLOCK);

    if(res == -1 || res2 == -1) {
        printf("ce_conf_createFifos -- fcntl() failed\n");
        return;
    }

    printf("ce_conf FIFOs created\n");
}

void *readWriteThreadCode(void *ptr)
{
    #define BUFFER_SIZE     1024
    char bfr[BUFFER_SIZE];
    int res, bytesAvailable, readSize;

    int max_fd = -1;
    struct timeval timeout;
    fd_set readfds;

    #define WAIT_TIME       2500

    createFifos();                              // create and open FIFOs

    while(1) {
        if(linuxConsole_fdMaster < 0) {
            printf("invalid linuxConsole_fdMaster\n");
            usleep(1000);
            continue;
        }

        FD_ZERO(&readfds);                      // clear readfds
        max_fd = -1;

        memset(&timeout, 0, sizeof(timeout));
        timeout.tv_sec = WAIT_TIME / 1000;          // sec
        timeout.tv_usec = (WAIT_TIME % 1000)*1000;  // usec

        if(linuxConsole_fdMaster >= 0) {            // if got linux console fd
            FD_SET(linuxConsole_fdMaster, &readfds);
            max_fd = MAX(max_fd, linuxConsole_fdMaster);
        }

        if(fifo_in >= 0) {                      // if got input fifo
            FD_SET(fifo_in, &readfds);
            max_fd = MAX(max_fd, fifo_in);
        }

        if(select(max_fd + 1, &readfds, NULL, NULL, &timeout) < 0) {
            printf("select() returned error: %s\n", strerror(errno));
            continue;
        }

        res = ioctl(linuxConsole_fdMaster, FIONREAD, &bytesAvailable); // how many bytes we can read?

        if(res != -1 && bytesAvailable > 0) {   // something to read?
            readSize = MIN(BUFFER_SIZE, bytesAvailable);
            read(linuxConsole_fdMaster, bfr, readSize);
            write(fifo_out, bfr, readSize);
        }

        res = ioctl(fifo_in, FIONREAD, &bytesAvailable); // how many bytes we can read?

        if(res != -1 && bytesAvailable > 0) {   // something to read?
            readSize = MIN(BUFFER_SIZE, bytesAvailable);
            read(fifo_in, bfr, readSize);
            write(linuxConsole_fdMaster, bfr, readSize);
        }
    }
}
