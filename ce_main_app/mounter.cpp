#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <signal.h>
#include <pthread.h>
#include <queue>          

#define LOGFILE1	"/tmp/mount.log"
#define LOGFILE2	"/tmp/mount.err"
#define MOUNTDUMP	"/tmp/mount.dmp"

#define MAX_STR_SIZE	1024

#include "mounter.h"
#include "utils.h"
#include "debug.h"
#include "update.h"

pthread_mutex_t mountThreadMutex = PTHREAD_MUTEX_INITIALIZER;
std::queue<TMounterRequest> mountQueue;

void mountAdd(TMounterRequest &tmr)
{
	pthread_mutex_lock(&mountThreadMutex);				// try to lock the mutex
	mountQueue.push(tmr);								// add this to queue
	pthread_mutex_unlock(&mountThreadMutex);			// unlock the mutex
}

void *mountThreadCode(void *ptr)
{
	Mounter mounter;

	Debug::out(LOG_INFO, "Mount thread starting...");

	while(sigintReceived == 0) {
		pthread_mutex_lock(&mountThreadMutex);			// lock the mutex

		if(mountQueue.size() == 0) {					// nothing to do?
			pthread_mutex_unlock(&mountThreadMutex);	// unlock the mutex
			sleep(1);									// wait 1 second and try again
			continue;
		}
		
		TMounterRequest tmr = mountQueue.front();		// get the 'oldest' element from queue
		mountQueue.pop();								// and remove it form queue
		pthread_mutex_unlock(&mountThreadMutex);		// unlock the mutex

		if(tmr.action == MOUNTER_ACTION_MOUNT) {		// should we mount this?
			if(tmr.deviceNotShared) {					// mount device?
				mounter.mountDevice((char *) tmr.devicePath.c_str(), (char *) tmr.mountDir.c_str());	
			} else {									// mount shared?
				mounter.mountShared((char *) tmr.shared.host.c_str(), (char *) tmr.shared.hostDir.c_str(), tmr.shared.nfsNotSamba, (char *) tmr.mountDir.c_str(), (char *) tmr.shared.username.c_str(), (char *) tmr.shared.password.c_str());
			}
			
			continue;
		} 
		
		if(tmr.action == MOUNTER_ACTION_UMOUNT) {		// should umount?
			mounter.umountIfMounted((char *) tmr.mountDir.c_str());
			continue;
		}
		
		if(tmr.action == MOUNTER_ACTION_RESTARTNETWORK) {   // restart network?
			mounter.restartNetwork();
            continue;
		}

        if(tmr.action == MOUNTER_ACTION_SYNC) {             // sync system caches?
            mounter.sync();
            continue;
        }
	}
	
	Debug::out(LOG_INFO, "Mount thread terminated.");
	return 0;
}

bool Mounter::mountDevice(char *devicePath, char *mountDir)
{
	char cmd[MAX_STR_SIZE];

	if(isAlreadyMounted(devicePath)) {
		Debug::out(LOG_INFO, "Mounter::mountDevice -- The device %s is already mounted, not doing anything.\n", devicePath);
		return true;
	}

    // was: sudo
	snprintf(cmd, MAX_STR_SIZE, "mount -v %s %s >> %s 2>> %s", devicePath, mountDir, LOGFILE1, LOGFILE2);

	int len = strnlen(cmd, MAX_STR_SIZE);	// get the length

	// if the final command string did not fit in the buffer
	if(len == MAX_STR_SIZE) {
		Debug::out(LOG_ERROR, "Mounter::mountDevice - cmd string not large enough for mount!\n");
		return false;
	}
	
	// now do the mount stuff
	return mount(cmd, mountDir);
}

bool Mounter::mountShared(char *host, char *hostDir, bool nfsNotSamba, char *mountDir, char *username, char *password)
{
	// build and run the command
	char cmd[MAX_STR_SIZE];
    char auth[MAX_STR_SIZE];
	char source[MAX_STR_SIZE];
	int len;
        
    if(strlen(username) == 0) {             // no user name? 
        if(nfsNotSamba) {                   // for NFS - don't specify auth
            memset(auth, 0, MAX_STR_SIZE);
        } else {                            // for CIFS - specify guest
            strcpy(auth, ",username=guest");
        }
    } else {                                // got user name?
        if(strlen(password) == 0) {         // don't have password?
            snprintf(auth, MAX_STR_SIZE, ",username=%s", username);
        } else {                            // and got password?
            snprintf(auth, MAX_STR_SIZE, ",username=%s,password=%s", username, password);
        }
    }
	
	createSource(host, hostDir, nfsNotSamba, source);		// create source path
	
	// check if we're not trying to mount something we already have mounted
	if(isAlreadyMounted(source)) {
		Debug::out(LOG_INFO, "Mounter::mountShared -- The source %s is already mounted, not doing anything.\n", source);
		return true;
	}
		
	if(nfsNotSamba) {		// for NFS
        // was: sudo
		snprintf(cmd, MAX_STR_SIZE, "mount -v -t nfs -o nolock%s %s %s >> %s 2>> %s", auth, source, mountDir, LOGFILE1, LOGFILE2);
	} else {				// for Samba
		passwd *psw = getpwnam("pi");
		
		if(psw == NULL) {
			Debug::out(LOG_ERROR, "Mounter::mountShared - failed, because couldn't get uid and gid for user 'pi'\n");
			return false;
		}
        
        // was: sudo
		snprintf(cmd, MAX_STR_SIZE, "mount -v -t cifs -o gid=%d,uid=%d%s %s %s >> %s 2>> %s", psw->pw_gid, psw->pw_uid, auth, source, mountDir, LOGFILE1, LOGFILE2);
	}

	len = strnlen(cmd, MAX_STR_SIZE);	// get the length

	// if the final command string did not fit in the buffer
	if(len == MAX_STR_SIZE) {
		Debug::out(LOG_ERROR, "Mounter::mountShared - cmd string not large enough for mount!\n");
		return false;
	}
	
	// now do the mount stuff
	return mount(cmd, mountDir);
}

bool Mounter::mount(char *mountCmd, char *mountDir)
{
	// create mount dir if possible
	int ires = mkdir(mountDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);		// try to create the mount dir, mod: 0x775
	
	if(ires != 0 && errno != EEXIST) {										// if failed to create dir, and it's not because it already exists...
		Debug::out(LOG_ERROR, "Mounter::mount - failed to create directory %s", mountDir);
		return false;
	}

	// check if we should do unmount first
	if(isMountdirUsed(mountDir)) {
		if(!tryUnmount(mountDir)) {
			Debug::out(LOG_ERROR, "Mounter::mount - Mount dir %s is busy, and unmount failed, not doing mount!\n", mountDir);
			return false;
		}
		
		Debug::out(LOG_INFO, "Mounter::mount - Mount dir %s was used and was unmounted.\n", mountDir);
	}
	
	// delete previous log files (if there are any)
	unlink(LOGFILE1);
	unlink(LOGFILE2);
    
    // now fill those log files with at least one line of something - TOS 1.02 doesn't like to show empty files (it dumps bullshit indefinitely instead of terminating)
    char tmp[256];
    sprintf(tmp, "echo -e \"Mount log file: \n\r\" > %s", LOGFILE1);
    system(tmp);
    sprintf(tmp, "echo -e \"Mount error file: \n\r\" > %s", LOGFILE2);
    system(tmp);
    
	Debug::out(LOG_INFO, "Mounter::mount - mount command:\n%s\n", mountCmd);
	
	// build and run the command
	int ret = system(mountCmd);
	
	// handle the result
	if(WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
		Debug::out(LOG_INFO, "Mounter::mount - mount succeeded! (mount dir: %s)\n", mountDir);
		return true;
	} 
	
	Debug::out(LOG_ERROR, "Mounter::mount - mount failed. (mount dir: %s)\n", mountDir);
		
    // copy the content of mount output to log file so we can examine it later...
    copyTextFileToLog((char *) LOGFILE1);
    copyTextFileToLog((char *) LOGFILE2);

	// move the logs to mount dir
	char cmd[MAX_STR_SIZE];
		
    // was: sudo
	sprintf(cmd, "mv %s %s/", LOGFILE1, mountDir);
	system(cmd);

    // was: sudo
	sprintf(cmd, "mv %s %s/", LOGFILE2, mountDir);
	system(cmd);

	return false;
}

bool Mounter::isAlreadyMounted(char *source)
{
	return mountDumpContains(source);
}

bool Mounter::isMountdirUsed(char *mountDir)
{
	return mountDumpContains(mountDir);
}

bool Mounter::mountDumpContains(char *searchedString)
{
	char line[MAX_STR_SIZE];

	unlink(MOUNTDUMP);							// delete mount dump file
	
	// create new dump file and also filter it for searched string
	sprintf(line, "mount | grep '%s' > %s", searchedString, MOUNTDUMP);
	system(line);
	
	// open the file
	FILE *f = fopen(MOUNTDUMP, "rt");			// open the file
	
	if(!f) {									// if open failed, say that it's not mounted
		return false;
	}

	// search the file
	while(!feof(f)) {							// go through the lines of that file
		memset(line, 0, MAX_STR_SIZE);
		fgets(line, MAX_STR_SIZE, f);
		
		if(strstr(line, searchedString) != 0) {	// if the source was found in the mount dump, then it's already mounted
			fclose(f);
			return true;
		}
	}

	// on fail
	fclose(f);									// close mount dump file
	return false;								// if we got here, then we didn't find the mount
}

bool Mounter::tryUnmount(char *mountDir)
{
	char line[MAX_STR_SIZE];
	
    system("sync");                             // sync the caches
    
	// build and execute the command - was: sudo
	snprintf(line, MAX_STR_SIZE, "umount %s", mountDir);
	Debug::out(LOG_DEBUG, "Mounter::tryUnmount - umount command: %s", line);
    
    int ret = system(line);
	
	// handle the result
	if(WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
		Debug::out(LOG_INFO, "Mounter::tryUnmount - umount succeeded\n");
		return true;
	} 
	
	return false;
}

void Mounter::createSource(char *host, char *hostDir, bool nfsNotSamba, char *source)
{
	if(strlen(host) < 1 || strlen(hostDir) < 1) {		// input string(s) too short? fail
		source[0] = 0;
		return;
	}

	if(nfsNotSamba) {									// for nfs
		int len;
		
		strcpy(source, host);
		len = strlen(source);
		
		if(source[len - 1] == '/') {					// ends with slash? remove it
			source[len - 1] = 0;
		}
		
		strcat(source, ":");							// append ':'
		
		if(hostDir[0] != '/') {							// doesn't start with slash? add it!
			strcat(source, "/");
		}
		
		strcat(source, hostDir);						// append the host dir
	} else {											// for samba
		strcpy(source, "//");							// start with double-slash
		
		strcat(source, host);							// append host
		
		if(hostDir[0] != '/') {							// host dir doesn't start with slash? add it
			strcat(source, "/");
		}
		
		strcat(source, hostDir);
	}
}

void Mounter::umountIfMounted(char *mountDir)
{
	if(isMountdirUsed(mountDir)) {				// if mountDir is used, umount
		tryUnmount(mountDir);
	}
}

void Mounter::restartNetwork(void)
{
	Debug::out(LOG_INFO, "Mounter::restartNetwork - starting to restart the network\n");

    system("sync");                                                 // first sync the filesystem caches...

    bool gotWlan0 = wlan0IsPresent();                               // first find out if we got wlan0 or not

	system("ifdown eth0");                                          // shut down ethernet

    if(gotWlan0) {                                                  // if got wlan, shut it down
    	system("ifdown wlan0");
    	system("wpa_cli reconfigure");                              // also try to reconnect to wifi AP (wifi settings might have changed)
    }

	system("ifup eth0");                                            // bring up ethernet

    if(gotWlan0) {                                                  // if got wlan, bring it up
    	system("ifup wlan0");
    }
	
	Debug::out(LOG_INFO, "Mounter::restartNetwork - done\n");
}

bool Mounter::wlan0IsPresent(void)
{
    system("ifconfig -a | grep wlan0 > /tmp/wlan0dump.txt");                    // first check ifconfig for presence of wlan0 interface

	struct stat attr;
    int res = stat("/tmp/wlan0dump.txt", &attr);							    // get the file status
	
	if(res != 0) {
    	Debug::out(LOG_DEBUG, "Mounter::wlan0IsPresent() -- stat() failed\n");
        return false;
    }

    if(attr.st_size == 0) {                                                     // if the file is empty, ifconfig doesn't contain wlan0
    	Debug::out(LOG_DEBUG, "Mounter::wlan0IsPresent() -- file empty, wlan0 not present\n");
        return false;
    }

  	Debug::out(LOG_DEBUG, "Mounter::wlan0IsPresent() -- wlan0 is present\n");
    return true;                                                                // ifconfig contains wlan0, so we got wlan0
}

void Mounter::sync(void)                        // just do sync on filesystem
{
    system("sync");
	Debug::out(LOG_DEBUG, "Mounter::sync - filesystem sync done\n");
}

void Mounter::copyTextFileToLog(char *path)
{
    FILE *f = fopen(path, "rt");

    if(!f) {
        Debug::out(LOG_DEBUG, "Mounter::copyTextFileToLog -- failed to open file %s", path);
        return;
    }

    char line[1024];

    Debug::out(LOG_DEBUG,"--------------------------------");
    Debug::out(LOG_DEBUG,"Start of content of %s:", path);

    while(!feof(f)) {
        memset(line, 0, 1024);
        char *res = fgets(line, 1023, f);

        if(!res) {
            break;
        }

        Debug::out(LOG_DEBUG, line);        
    }

    Debug::out(LOG_DEBUG,"End of content of %s:", path);
    Debug::out(LOG_DEBUG,"--------------------------------");

    fclose(f);
}



