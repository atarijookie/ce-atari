// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab
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
#include <limits.h>

#include "global.h"
#include "debug.h"
#include "utils.h"

bool otherInstanceIsRunning(void);
int  singleInstanceSocketFd;
void networkServerMain(void);

int logLevel = LOG_DEBUG;

int main(int argc, char *argv[])
{
    printf("\033[H\033[2J\n");

    Utils::loadDotEnv();                    // load dotEnv before setting default log file
    Debug::setLogLevel(LOG_DEBUG);          // init current log level to LOG_ERROR
    Debug::setDefaultLogFile();             // set log file after env vars available
    Debug::out(LOG_INFO, "\n\n"); Debug::out(LOG_INFO, "---------------------------------------------------");
    Debug::printfLogLevelString();

    if(otherInstanceIsRunning()) {
        Debug::out(LOG_ERROR, "Other instance of CE discovery is running, terminate it before starting a new one!");
        printf("\nOther instance of CE discovery is running, terminate it before starting a new one!\n\n\n");
        return 0;
    }

    networkServerMain();
    return 0;
}

bool otherInstanceIsRunning(void)
{
    FILE * f;
    int other_pid = 0;
    int self_pid = 0;
    char proc_path[256];
    char other_exe[PATH_MAX];
    char self_exe[PATH_MAX];

    self_pid = getpid();
    std::string pidDir = Utils::dotEnvValue("PID_DIR", PID_DIR_DEFAULT);
    std::string pidFilePath = Utils::mergeHostPaths3(pidDir, "ce_discovery.pid");

    f = fopen(pidFilePath.c_str(), "r");
    if(!f) {    // can't open file? other instance probably not running (or is, but can't figure out, so screw it)
        Debug::out(LOG_DEBUG, "otherInstanceIsRunning - couldn't open %s, returning false", pidFilePath.c_str());
    } else {
        int r = fscanf(f, "%d", &other_pid);
        fclose(f);
        if(r != 1) {
            Debug::out(LOG_ERROR, "otherInstanceIsRunning - can't read pid in %s, returning false", pidFilePath.c_str());
        } else {
            Debug::out(LOG_DEBUG, "otherInstanceIsRunning - %s pid=%d (own pid=%d)", pidFilePath.c_str(), other_pid, self_pid);
            snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", other_pid);
            if(readlink("/proc/self/exe", self_exe, sizeof(self_exe)) < 0) {
                Debug::out(LOG_ERROR, "otherInstanceIsRunning readlink(%s): %s", "/proc/self/exe", strerror(errno));
            } else if(readlink(proc_path, other_exe, sizeof(other_exe)) < 0) {
                Debug::out(LOG_ERROR, "otherInstanceIsRunning readlink(%s): %s", proc_path, strerror(errno));
            } else if(strcmp(other_exe, self_exe) == 0) {
                // NOTE: is it needed to check the process status to discard zombies ???
                Debug::out(LOG_DEBUG, "otherInstanceIsRunning - found another instance of %s with pid %d", other_exe, other_pid);
                return true;
            }
        }
    }

    Utils::intToFile(self_pid, pidFilePath.c_str());
    Debug::out(LOG_DEBUG, "otherInstanceIsRunning -- pid %d written to %s", self_pid, pidFilePath.c_str());

    return false;
}
